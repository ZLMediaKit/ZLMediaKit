/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "FFMpegDecoder.h"
#define MAX_DELAY_SECOND 60

using namespace std;
using namespace mediakit;

static string ffmpeg_err(int errnum){
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return errbuf;
}

///////////////////////////////////////////////////////////////////////////

template<bool decoder = true, typename ...ARGS>
AVCodec *getCodecByName(ARGS ...names);

template<bool decoder = true, typename ...ARGS>
AVCodec *getCodecByName(const char *name) {
    auto codec = decoder ? avcodec_find_decoder_by_name(name) : avcodec_find_encoder_by_name(name);
    if (codec) {
        InfoL << (decoder ? "got decoder:" : "got encoder:") << name;
    }
    return codec;
}

template<bool decoder = true, typename ...ARGS>
AVCodec *getCodecByName(const char *name, ARGS ...names) {
    auto codec = getCodecByName<decoder>(names...);
    if (codec) {
        return codec;
    }
    return getCodecByName<decoder>(name);
}

template<bool decoder = true>
AVCodec *getCodec(enum AVCodecID id) {
    auto codec = decoder ? avcodec_find_decoder(id) : avcodec_find_encoder(id);
    if (codec) {
        InfoL << (decoder ? "got decoder:" : "got encoder:") << avcodec_get_name(id);
    }
    return codec;
}

template<bool decoder = true, typename ...ARGS>
AVCodec *getCodec(enum AVCodecID id, ARGS ...names) {
    auto codec = getCodecByName<decoder>(names...);
    if (codec) {
        return codec;
    }
    return getCodec<decoder>(id);
}

///////////////////////////////////////////////////////////////////////////

FFmpegFrame::FFmpegFrame(std::shared_ptr<AVFrame> frame) {
    if (frame) {
        _frame = std::move(frame);
    } else {
        _frame.reset(av_frame_alloc(), [](AVFrame *ptr) {
            av_frame_unref(ptr);
            av_frame_free(&ptr);
        });
    }
}

FFmpegFrame::~FFmpegFrame(){
    if (_data) {
        delete[] _data;
        _data = nullptr;
    }
}

AVFrame *FFmpegFrame::get() const{
    return _frame.get();
}

///////////////////////////////////////////////////////////////////////////

FFmpegSwr::FFmpegSwr(AVSampleFormat output, int channel, int channel_layout, int samplerate){
    _target_format = output;
    _target_channels = channel;
    _target_channel_layout = channel_layout;
    _target_samplerate = samplerate;
    _frame_pool.setSize(8);
}

FFmpegSwr::~FFmpegSwr(){
    if (_ctx) {
        swr_free(&_ctx);
    }
}

FFmpegFrame::Ptr FFmpegSwr::inputFrame(const FFmpegFrame::Ptr &frame){
    if (frame->get()->format == _target_format &&
        frame->get()->channels == _target_channels &&
        frame->get()->channel_layout == _target_channel_layout &&
        frame->get()->sample_rate == _target_samplerate) {
        //不转格式
        return frame;
    }
    if (!_ctx) {
        _ctx = swr_alloc_set_opts(nullptr, _target_channel_layout, _target_format, _target_samplerate,
                                  frame->get()->channel_layout, (AVSampleFormat) frame->get()->format,
                                  frame->get()->sample_rate, 0, nullptr);
        InfoL << "swr_alloc_set_opts:" << av_get_sample_fmt_name((enum AVSampleFormat) frame->get()->format) << " -> "
              << av_get_sample_fmt_name(_target_format);
    }
    if (_ctx) {
        FFmpegFrame::Ptr out = _frame_pool.obtain();
        out->get()->format = _target_format;
        out->get()->channel_layout = _target_channel_layout;
        out->get()->channels = _target_channels;
        out->get()->sample_rate = _target_samplerate;
        out->get()->pkt_dts = frame->get()->pkt_dts;
        out->get()->pkt_pts = frame->get()->pkt_pts;
        out->get()->pts = frame->get()->pts;

        int ret;
        if(0 != (ret = swr_convert_frame(_ctx, out->get(), frame->get()))){
            WarnL << "swr_convert_frame failed:" << ffmpeg_err(ret);
            return nullptr;
        }
        //修正大小
        out->get()->linesize[0] = out->get()->nb_samples * out->get()->channels * av_get_bytes_per_sample((enum AVSampleFormat)out->get()->format);
        return out;
    }

    return nullptr;
}
void FFmpegFrame::fillPicture(AVPixelFormat target_format, int target_width, int  target_height){
    assert(_data == nullptr);
    _data = new char[avpicture_get_size(target_format, target_width, target_height)];
    avpicture_fill((AVPicture *) _frame.get(), (uint8_t *) _data, target_format, target_width, target_height);
}

///////////////////////////////////////////////////////////////////////////

FFmpegDecoder::FFmpegDecoder(const Track::Ptr &track) {
    _frame_pool.setSize(8);
    avcodec_register_all();
    AVCodec *codec = nullptr;
    AVCodec *codec_default = nullptr;
    switch (track->getCodecId()) {
        case CodecH264:
            codec_default = getCodec(AV_CODEC_ID_H264);
            codec = getCodec(AV_CODEC_ID_H264, "h264_cuvid","h264_videotoolbox");
            break;
        case CodecH265:
            codec_default = getCodec(AV_CODEC_ID_HEVC);
            codec = getCodec(AV_CODEC_ID_HEVC, "hevc_cuvid","hevc_videotoolbox");
            break;
        case CodecAAC:
            codec = getCodec(AV_CODEC_ID_AAC);
            break;
        case CodecG711A:
            codec = getCodec(AV_CODEC_ID_PCM_ALAW);
            break;
        case CodecG711U:
            codec = getCodec(AV_CODEC_ID_PCM_MULAW);
            break;
        case CodecOpus:
            codec = getCodec(AV_CODEC_ID_OPUS);
            break;
        default: break;
    }

    if (!codec) {
        throw std::runtime_error("未找到解码器");
    }

    while (true) {
        _context.reset(avcodec_alloc_context3(codec), [](AVCodecContext *ctx) {
            avcodec_close(ctx);
            avcodec_free_context(&ctx);
        });

        if (!_context) {
            throw std::runtime_error("创建解码器失败");
        }

        //保存AVFrame的引用
        _context->refcounted_frames = 1;
        _context->flags |= AV_CODEC_FLAG_LOW_DELAY;
        _context->flags2 |= AV_CODEC_FLAG2_FAST;

        switch (track->getCodecId()) {
            case CodecG711A:
            case CodecG711U: {
                AudioTrack::Ptr audio = static_pointer_cast<AudioTrack>(track);
                _context->channels = audio->getAudioChannel();
                _context->sample_rate = audio->getAudioSampleRate();
                _context->channel_layout = _context->channels == 1 ? AV_CH_LAYOUT_MONO : AV_CH_LAYOUT_STEREO;
                break;
            }
            default:
                break;
        }
        AVDictionary *dict = nullptr;
        av_dict_set(&dict, "threads", to_string(thread::hardware_concurrency()).data(), 0);
        av_dict_set(&dict, "zerolatency", "1", 0);
        av_dict_set(&dict, "strict", "-2", 0);

        if (codec->capabilities & AV_CODEC_CAP_TRUNCATED) {
            /* we do not send complete frames */
            _context->flags |= AV_CODEC_FLAG_TRUNCATED;
        }

        int ret = avcodec_open2(_context.get(), codec, &dict);
        if (ret >= 0) {
            //成功
            InfoL << "打开解码器成功:" << codec->name;
            break;
        }

        if (codec_default && codec_default != codec) {
            //硬件编解码器打开失败，尝试软件的
            WarnL << "打开解码器" << codec->name << "失败，原因是:" << ffmpeg_err(ret) << ", 再尝试打开解码器" << codec_default->name;
            codec = codec_default;
            continue;
        }
        throw std::runtime_error(StrPrinter << "打开解码器" << codec->name << "失败:" << ffmpeg_err(ret));
    }
}

void FFmpegDecoder::flush(){
    while (true) {
        FFmpegFrame::Ptr out_frame = _frame_pool.obtain();
        auto ret = avcodec_receive_frame(_context.get(), out_frame->get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            WarnL << "avcodec_receive_frame failed:" << ffmpeg_err(ret);
            break;
        }
        onDecode(out_frame);
    }
}

const AVCodecContext *FFmpegDecoder::getContext() const{
    return _context.get();
}

void FFmpegDecoder::inputFrame(const Frame::Ptr &frame) {
    inputFrame(frame->data(), frame->size(), frame->dts(), frame->pts());
}

void FFmpegDecoder::inputFrame(const char *data, size_t size, uint32_t dts, uint32_t pts) {
    AVPacket pkt;
    av_init_packet(&pkt);

    pkt.data = (uint8_t *) data;
    pkt.size = size;
    pkt.dts = dts;
    pkt.pts = pts;

    auto ret = avcodec_send_packet(_context.get(), &pkt);
    if (ret < 0) {
        if (ret != AVERROR_INVALIDDATA) {
            WarnL << "avcodec_send_packet failed:" << ffmpeg_err(ret);
        }
        return;
    }

    while (true) {
        FFmpegFrame::Ptr out_frame = _frame_pool.obtain();
        ret = avcodec_receive_frame(_context.get(), out_frame->get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            WarnL << "avcodec_receive_frame failed:" << ffmpeg_err(ret);
            break;
        }
        if (pts - out_frame->get()->pkt_pts > MAX_DELAY_SECOND * 1000 && _ticker.createdTime() > 10 * 1000) {
            //后面的帧才忽略,防止Track无法ready
            WarnL << "解码时，忽略" << MAX_DELAY_SECOND << "秒前的数据:" << pts << " " << out_frame->get()->pkt_pts;
            continue;
        }
        onDecode(out_frame);
    }
}

void FFmpegDecoder::setOnDecode(FFmpegDecoder::onDec cb) {
    _cb = std::move(cb);
}

void FFmpegDecoder::onDecode(const FFmpegFrame::Ptr &frame){
    if (_context->codec_type == AVMEDIA_TYPE_AUDIO) {
        if (!_swr) {
            //固定输出16位整型的pcm
            _swr = std::make_shared<FFmpegSwr>(AV_SAMPLE_FMT_S16, frame->get()->channels, frame->get()->channel_layout, frame->get()->sample_rate);
        }
        //音频情况下，转换音频format类型，比如说浮点型转换为int型
        const_cast<FFmpegFrame::Ptr &>(frame) = _swr->inputFrame(frame);
    }
    if (_cb && frame) {
        _cb(frame);
    }
}

