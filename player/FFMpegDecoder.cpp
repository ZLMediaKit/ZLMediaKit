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
#define MAX_DELAY_SECOND 3

using namespace std;
using namespace mediakit;

static string ffmpeg_err(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return errbuf;
}

std::shared_ptr<AVPacket> alloc_av_packet(){
    auto pkt = std::shared_ptr<AVPacket>(av_packet_alloc(), [](AVPacket *pkt) {
        av_packet_free(&pkt);
    });
    pkt->data = NULL;    // packet data will be allocated by the encoder
    pkt->size = 0;
    return pkt;
}

//////////////////////////////////////////////////////////////////////////////////////////

template<bool decoder = true, typename ...ARGS>
AVCodec *getCodec(ARGS ...names);

template<bool decoder = true>
AVCodec *getCodec(const char *name) {
    auto codec = decoder ? avcodec_find_decoder_by_name(name) : avcodec_find_encoder_by_name(name);
    if (codec) {
        InfoL << (decoder ? "got decoder:" : "got encoder:") << name;
    }
    return codec;
}

template<bool decoder = true>
AVCodec *getCodec(enum AVCodecID id) {
    auto codec = decoder ? avcodec_find_decoder(id) : avcodec_find_encoder(id);
    if (codec) {
        InfoL << (decoder ? "got decoder:" : "got encoder:") << avcodec_get_name(id);
    }
    return codec;
}

template<bool decoder = true, typename First, typename ...ARGS>
AVCodec *getCodec(First first, ARGS ...names) {
    auto codec = getCodec<decoder>(names...);
    if (codec) {
        return codec;
    }
    return getCodec<decoder>(first);
}

//////////////////////////////////////////////////////////////////////////////////////////

FFmpegFrame::FFmpegFrame(std::shared_ptr<AVFrame> frame) {
    if (frame) {
        _frame = std::move(frame);
    } else {
        _frame.reset(av_frame_alloc(), [](AVFrame *ptr) {
            av_frame_free(&ptr);
        });
    }
}

FFmpegFrame::~FFmpegFrame() {
    if (_data) {
        delete[] _data;
        _data = nullptr;
    }
}

AVFrame *FFmpegFrame::get() const {
    return _frame.get();
}

////////////////////////////////////////////////////////////////////////////////////////////////////////////

FFmpegSwr::FFmpegSwr(AVSampleFormat output, int channel, int channel_layout, int samplerate) {
    _target_format = output;
    _target_channels = channel;
    _target_channel_layout = channel_layout;
    _target_samplerate = samplerate;
}

FFmpegSwr::~FFmpegSwr() {
    if (_ctx) {
        swr_free(&_ctx);
    }
}

FFmpegFrame::Ptr FFmpegSwr::inputFrame(const FFmpegFrame::Ptr &frame) {
    if (frame->get()->format == _target_format &&
    frame->get()->channels == _target_channels &&
    frame->get()->channel_layout == (uint64_t)_target_channel_layout &&
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
        auto out = std::make_shared<FFmpegFrame>();
        out->get()->format = _target_format;
        out->get()->channel_layout = _target_channel_layout;
        out->get()->channels = _target_channels;
        out->get()->sample_rate = _target_samplerate;
        out->get()->pkt_dts = frame->get()->pkt_dts;
        out->get()->pts = frame->get()->pts;

        int ret = 0;
        if(0 != (ret = swr_convert_frame(_ctx, out->get(), frame->get()))){
            WarnL << "swr_convert_frame failed:" << ffmpeg_err(ret);
            return nullptr;
        }
        return out;
    }

    return nullptr;
}


///////////////////////////////////////////////////////////////////////////

FFmpegDecoder::FFmpegDecoder(const Track::Ptr &track) {
    avcodec_register_all();
    AVCodec *codec = nullptr;
    AVCodec *codec_default = nullptr;
    switch (track->getCodecId()) {
        case CodecH264:
            codec_default = getCodec(AV_CODEC_ID_H264);
            codec = getCodec("libopenh264", AV_CODEC_ID_H264, "h264_videotoolbox", "h264_cuvid");
            break;
        case CodecH265:
            codec_default = getCodec(AV_CODEC_ID_HEVC);
            codec = getCodec(AV_CODEC_ID_HEVC, "hevc_videotoolbox", "hevc_cuvid");
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
                    _context->channel_layout = av_get_default_channel_layout(_context->channels);
                    break;
                }
                default:
                    break;
        }
        AVDictionary *dict = nullptr;
        av_dict_set(&dict, "threads", "auto", 0);
        av_dict_set(&dict, "zerolatency", "1", 0);
        av_dict_set(&dict, "strict", "-2", 0);

        if (codec->capabilities & AV_CODEC_CAP_TRUNCATED) {
            /* we do not send complete frames */
            _context->flags |= AV_CODEC_FLAG_TRUNCATED;
        } else {
            // 此时业务层应该需要合帧
            _do_merger = true;
        }

        int ret = avcodec_open2(_context.get(), codec, &dict);
        av_dict_free(&dict);
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

    if (track->getTrackType() == TrackVideo) {
        startThread("decoder thread");
    }
}

FFmpegDecoder::~FFmpegDecoder() {
    stopThread();
}

void FFmpegDecoder::flush() {
    while (true) {
        auto out_frame = std::make_shared<FFmpegFrame>();
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

const AVCodecContext *FFmpegDecoder::getContext() const {
    return _context.get();
}

bool FFmpegDecoder::inputFrame_l(const Frame::Ptr &frame) {
    if (_do_merger) {
        return _merger.inputFrame(frame, [&](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer, bool have_idr) {
            decodeFrame(buffer->data(), buffer->size(), dts, pts);
        });
    }
    return decodeFrame(frame->data(), frame->size(), frame->dts(), frame->pts());
}

bool FFmpegDecoder::inputFrame(const Frame::Ptr &frame, bool may_async) {
    if (!may_async || !TaskManager::isEnabled()) {
        return inputFrame_l(frame);
    }
    auto frame_cache = Frame::getCacheAbleFrame(frame);
    addDecodeTask(frame->keyFrame(), [this, frame_cache]() {
        inputFrame_l(frame_cache);
        //此处模拟解码太慢导致的主动丢帧
        //usleep(100 * 1000);
    });
    return true;
}

bool FFmpegDecoder::decodeFrame(const char *data, size_t size, uint32_t dts, uint32_t pts) {
    TimeTicker2(30, TraceL);

    auto pkt = alloc_av_packet();
    pkt->data = (uint8_t *) data;
    pkt->size = size;
    pkt->dts = dts;
    pkt->pts = pts;

    auto ret = avcodec_send_packet(_context.get(), pkt.get());
    if (ret < 0) {
        if (ret != AVERROR_INVALIDDATA) {
            WarnL << "avcodec_send_packet failed:" << ffmpeg_err(ret);
        }
        return false;
    }

    while (true) {
        auto out_frame = std::make_shared<FFmpegFrame>();
        ret = avcodec_receive_frame(_context.get(), out_frame->get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            WarnL << "avcodec_receive_frame failed:" << ffmpeg_err(ret);
            break;
        }
        if (pts - out_frame->get()->pts > MAX_DELAY_SECOND * 1000 && _ticker.createdTime() > 10 * 1000) {
            //后面的帧才忽略,防止Track无法ready
            WarnL << "解码时，忽略" << MAX_DELAY_SECOND << "秒前的数据:" << pts << " " << out_frame->get()->pts;
            continue;
        }
        onDecode(out_frame);
    }
    return true;
}

void FFmpegDecoder::setOnDecode(FFmpegDecoder::onDec cb) {
    _cb = std::move(cb);
}

void FFmpegDecoder::onDecode(const FFmpegFrame::Ptr &frame) {
    if (_cb) {
        _cb(frame);
    }
}

////////////////////////////////////////////////////////////////////////

void TaskManager::pushExit(){
    {
        lock_guard<mutex> lck(_task_mtx);
        _exit = true;
        _task.clear();
        _task.emplace_back([](){
            throw ThreadExitException();
        });
    }
    _sem.post(10);
}

void TaskManager::addEncodeTask(function<void()> task) {
    {
        lock_guard<mutex> lck(_task_mtx);
        _task.emplace_back(std::move(task));
        if (_task.size() > 30) {
            WarnL << "encoder thread task is too more, now drop frame!";
            _task.pop_front();
        }
    }
    _sem.post();
}

void TaskManager::addDecodeTask(bool key_frame, function<void()> task) {
    {
        lock_guard<mutex> lck(_task_mtx);
        if (_decode_drop_start) {
            if (!key_frame) {
                TraceL << "decode thread drop frame";
                return;
            }
            _decode_drop_start = false;
            InfoL << "decode thread stop drop frame";
        }

        _task.emplace_back(std::move(task));
        if (_task.size() > 30) {
            _decode_drop_start = true;
            WarnL << "decode thread start drop frame";
        }
    }
    _sem.post();
}

void TaskManager::startThread(const string &name) {
    _thread.reset(new thread([this, name]() {
        onThreadRun(name);
    }), [this](thread *ptr) {
        pushExit();
        ptr->join();
        delete ptr;
    });
}

void TaskManager::stopThread() {
    _thread = nullptr;
}

TaskManager::~TaskManager() {
    stopThread();
}

bool TaskManager::isEnabled() const {
    return _thread.operator bool();
}

void TaskManager::onThreadRun(const string &name) {
    setThreadName(name.data());
    function<void()> task;
    _exit = false;
    while (!_exit) {
        _sem.wait();
        {
            unique_lock<mutex> lck(_task_mtx);
            if (_task.empty()) {
                continue;
            }
            task = _task.front();
            _task.pop_front();
        }

        try {
            TimeTicker2(50, TraceL);
            task();
            task = nullptr;
        } catch (ThreadExitException &ex) {
            break;
        } catch (std::exception &ex) {
            WarnL << ex.what();
            continue;
        }
    }
    InfoL << name << " exited!";
}
