/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_FFMPEG)

#include "FFmpegEncoder.h"
#include "Util/logger.h"
#include "Common/config.h"

#if !defined(_WIN32)
#include <dlfcn.h>
#endif

extern "C" {
#include "libavutil/opt.h"
}

using namespace std;
using namespace toolkit;

namespace mediakit {

static string ffmpeg_err(int errnum) {
    char errbuf[AV_ERROR_MAX_STRING_SIZE];
    av_strerror(errnum, errbuf, AV_ERROR_MAX_STRING_SIZE);
    return errbuf;
}

static std::unique_ptr<AVPacket, void (*)(AVPacket *)> alloc_av_packet() {
    return std::unique_ptr<AVPacket, void (*)(AVPacket *)>(av_packet_alloc(), [](AVPacket *pkt) { av_packet_free(&pkt); });
}

AVCodecID getAVCodecID(CodecId codec_id) {
    switch (codec_id) {
        case CodecH264: return AV_CODEC_ID_H264;
        case CodecH265: return AV_CODEC_ID_HEVC;
        case CodecAAC: return AV_CODEC_ID_AAC;
        case CodecG711A: return AV_CODEC_ID_PCM_ALAW;
        case CodecG711U: return AV_CODEC_ID_PCM_MULAW;
        case CodecOpus: return AV_CODEC_ID_OPUS;
        case CodecVP8: return AV_CODEC_ID_VP8;
        case CodecVP9: return AV_CODEC_ID_VP9;
        case CodecAV1: return AV_CODEC_ID_AV1;
        case CodecJPEG: return AV_CODEC_ID_MJPEG;
        case CodecL16: return AV_CODEC_ID_PCM_S16BE;
        default: return AV_CODEC_ID_NONE;
    }
}

CodecId getCodecId(AVCodecID av_codec_id) {
    switch (av_codec_id) {
        case AV_CODEC_ID_H264: return CodecH264;
        case AV_CODEC_ID_HEVC: return CodecH265;
        case AV_CODEC_ID_AAC: return CodecAAC;
        case AV_CODEC_ID_PCM_ALAW: return CodecG711A;
        case AV_CODEC_ID_PCM_MULAW: return CodecG711U;
        case AV_CODEC_ID_OPUS: return CodecOpus;
        case AV_CODEC_ID_VP8: return CodecVP8;
        case AV_CODEC_ID_VP9: return CodecVP9;
        case AV_CODEC_ID_AV1: return CodecAV1;
        case AV_CODEC_ID_MJPEG: return CodecJPEG;
        case AV_CODEC_ID_PCM_S16BE: return CodecL16;
        default: return CodecInvalid;
    }
}

// 获取编码器
template<bool is_encoder = true>
static const AVCodec *getCodec_l(const char *name) {
    auto codec = is_encoder ? avcodec_find_encoder_by_name(name) : avcodec_find_decoder_by_name(name);
    if (codec) {
        InfoL << (is_encoder ? "got encoder:" : "got decoder:") << name;
    } else {
        TraceL << (is_encoder ? "encoder:" : "decoder:") << name << " not found";
    }
    return codec;
}

template<bool is_encoder = true>
static const AVCodec *getCodec_l(enum AVCodecID id) {
    auto codec = is_encoder ? avcodec_find_encoder(id) : avcodec_find_decoder(id);
    if (codec) {
        InfoL << (is_encoder ? "got encoder:" : "got decoder:") << avcodec_get_name(id);
    } else {
        TraceL << (is_encoder ? "encoder:" : "decoder:") << avcodec_get_name(id) << " not found";
    }
    return codec;
}

// 检查是否支持NVIDIA硬件加速
static bool checkNvidiaSupport() {
#if !defined(_WIN32)
    static bool checked = false;
    static bool supported = false;
    if (!checked) {
        checked = true;
        auto so = dlopen("libnvcuvid.so.1", RTLD_LAZY);
        if (so) {
            dlclose(so);
            supported = true;
        }
    }
    return supported;
#else
    return false;
#endif
}

FFmpegEncoder::FFmpegEncoder(const Track::Ptr &track, int thread_num, const std::vector<std::string> &encoder_name) {
    _codec_id = track->getCodecId();
    _target_track = track;
    _frame_pool.setSize(AV_NUM_DATA_POINTERS);

    const AVCodec *codec = nullptr;
    const AVCodec *codec_default = nullptr;
    
    // 如果指定了编码器名称
    if (!encoder_name.empty()) {
        for (const auto &name : encoder_name) {
            codec = getCodec_l<true>(name.c_str());
            if (codec) break;
        }
    }

    AVCodecID av_codec_id = getAVCodecID(_codec_id);
    
    // 根据编码类型选择编码器
    switch (_codec_id) {
        case CodecH264:
            codec_default = getCodec_l<true>(AV_CODEC_ID_H264);
            if (!codec) {
                if (checkNvidiaSupport()) {
                    codec = getCodec_l<true>("h264_nvenc");
                }
                if (!codec) codec = getCodec_l<true>("h264_videotoolbox");
                if (!codec) codec = getCodec_l<true>("h264_qsv");
                if (!codec) codec = getCodec_l<true>("libx264");
                if (!codec) codec = getCodec_l<true>("libopenh264");
                if (!codec) codec = codec_default;
            }
            break;
            
        case CodecH265:
            codec_default = getCodec_l<true>(AV_CODEC_ID_HEVC);
            if (!codec) {
                if (checkNvidiaSupport()) {
                    codec = getCodec_l<true>("hevc_nvenc");
                }
                if (!codec) codec = getCodec_l<true>("hevc_videotoolbox");
                if (!codec) codec = getCodec_l<true>("hevc_qsv");
                if (!codec) codec = getCodec_l<true>("libx265");
                if (!codec) codec = codec_default;
            }
            break;
            
        case CodecVP8:
            codec_default = getCodec_l<true>(AV_CODEC_ID_VP8);
            if (!codec) {
                codec = getCodec_l<true>("libvpx");
                if (!codec) codec = codec_default;
            }
            break;
            
        case CodecVP9:
            codec_default = getCodec_l<true>(AV_CODEC_ID_VP9);
            if (!codec) {
                codec = getCodec_l<true>("libvpx-vp9");
                if (!codec) codec = codec_default;
            }
            break;
            
        case CodecAV1:
            codec_default = getCodec_l<true>(AV_CODEC_ID_AV1);
            if (!codec) {
                codec = getCodec_l<true>("libaom-av1");
                if (!codec) codec = getCodec_l<true>("libsvtav1");
                if (!codec) codec = getCodec_l<true>("av1_nvenc");
                if (!codec) codec = codec_default;
            }
            break;
            
        case CodecAAC:
            codec_default = getCodec_l<true>(AV_CODEC_ID_AAC);
            if (!codec) {
                codec = getCodec_l<true>("libfdk_aac");
                if (!codec) codec = getCodec_l<true>("aac");
                if (!codec) codec = codec_default;
            }
            break;
            
        case CodecOpus:
            codec_default = getCodec_l<true>(AV_CODEC_ID_OPUS);
            if (!codec) {
                codec = getCodec_l<true>("libopus");
                if (!codec) codec = codec_default;
            }
            break;
            
        case CodecG711A:
            codec = getCodec_l<true>(AV_CODEC_ID_PCM_ALAW);
            break;
            
        case CodecG711U:
            codec = getCodec_l<true>(AV_CODEC_ID_PCM_MULAW);
            break;
            
        case CodecJPEG:
            codec = getCodec_l<true>(AV_CODEC_ID_MJPEG);
            break;
            
        default:
            codec = getCodec_l<true>(av_codec_id);
            break;
    }

    if (!codec) {
        throw std::runtime_error("未找到编码器: " + string(getCodecName(_codec_id)));
    }

    _context.reset(avcodec_alloc_context3(codec), [](AVCodecContext *ctx) {
        avcodec_free_context(&ctx);
    });

    if (!_context) {
        throw std::runtime_error("创建编码器上下文失败");
    }

    // 配置编码参数
    if (track->getTrackType() == TrackVideo) {
        auto video_track = std::static_pointer_cast<VideoTrack>(track);
        _context->width = video_track->getVideoWidth();
        _context->height = video_track->getVideoHeight();
        _context->time_base = {1, 1000};  // 毫秒时间基
        _context->framerate = {(int)video_track->getVideoFps(), 1};
        _context->gop_size = (int)video_track->getVideoFps() * 2;  // 2秒一个GOP
        _context->max_b_frames = 0;  // 实时流不用B帧
        _context->bit_rate = video_track->getBitRate() > 0 ? video_track->getBitRate() : 2000000;
        _context->flags |= AV_CODEC_FLAG_LOW_DELAY;
        
        // 设置像素格式
        if (codec->pix_fmts) {
            _context->pix_fmt = codec->pix_fmts[0];
        } else {
            _context->pix_fmt = AV_PIX_FMT_YUV420P;
        }
        
        // 针对不同编码器的特殊配置
        if (strstr(codec->name, "264") || strstr(codec->name, "x264")) {
            av_opt_set(_context->priv_data, "preset", "ultrafast", 0);
            av_opt_set(_context->priv_data, "tune", "zerolatency", 0);
            av_opt_set(_context->priv_data, "profile", "baseline", 0);
        } else if (strstr(codec->name, "265") || strstr(codec->name, "x265") || strstr(codec->name, "hevc")) {
            av_opt_set(_context->priv_data, "preset", "ultrafast", 0);
            av_opt_set(_context->priv_data, "tune", "zerolatency", 0);
        } else if (strstr(codec->name, "nvenc")) {
            av_opt_set(_context->priv_data, "preset", "p1", 0);  // 最快
            av_opt_set(_context->priv_data, "tune", "ull", 0);   // ultra low latency
            av_opt_set(_context->priv_data, "zerolatency", "1", 0);
        } else if (strstr(codec->name, "vpx")) {
            av_opt_set(_context->priv_data, "deadline", "realtime", 0);
            av_opt_set(_context->priv_data, "cpu-used", "8", 0);
        } else if (strstr(codec->name, "av1") || strstr(codec->name, "aom")) {
            av_opt_set(_context->priv_data, "cpu-used", "8", 0);
            av_opt_set(_context->priv_data, "usage", "realtime", 0);
        }
        
    } else if (track->getTrackType() == TrackAudio) {
        auto audio_track = std::static_pointer_cast<AudioTrack>(track);
        _context->sample_rate = audio_track->getAudioSampleRate();
        _context->time_base = {1, _context->sample_rate};
        _context->bit_rate = audio_track->getBitRate() > 0 ? audio_track->getBitRate() : 128000;
        
#if LIBAVCODEC_VERSION_INT >= FF_CODEC_VER_7_1
        av_channel_layout_default(&_context->ch_layout, audio_track->getAudioChannel());
#else
        _context->channels = audio_track->getAudioChannel();
        _context->channel_layout = av_get_default_channel_layout(_context->channels);
#endif
        
        // 设置采样格式
        if (codec->sample_fmts) {
            _context->sample_fmt = codec->sample_fmts[0];
        } else {
            _context->sample_fmt = AV_SAMPLE_FMT_S16;
        }
        
        // 针对不同音频编码器的配置
        if (_codec_id == CodecOpus) {
            av_opt_set(_context->priv_data, "application", "lowdelay", 0);
        }
    }

    // 打开编码器
    AVDictionary *opts = nullptr;
    if (thread_num > 0) {
        av_dict_set(&opts, "threads", to_string(thread_num).c_str(), 0);
    } else {
        av_dict_set(&opts, "threads", "auto", 0);
    }

    int ret = avcodec_open2(_context.get(), codec, &opts);
    av_dict_free(&opts);
    
    if (ret < 0) {
        // 尝试使用默认编码器
        if (codec_default && codec_default != codec) {
            WarnL << "打开编码器 " << codec->name << " 失败: " << ffmpeg_err(ret) << ", 尝试默认编码器";
            
            // 用默认编码器重新分配上下文
            _context.reset(avcodec_alloc_context3(codec_default), [](AVCodecContext *ctx) {
                avcodec_free_context(&ctx);
            });
            
            if (!_context) {
                throw std::runtime_error("创建默认编码器上下文失败");
            }
            
            // 重新配置编码参数
            if (track->getTrackType() == TrackVideo) {
                auto video_track = std::static_pointer_cast<VideoTrack>(track);
                _context->width = video_track->getVideoWidth();
                _context->height = video_track->getVideoHeight();
                _context->time_base = {1, 1000};
                _context->framerate = {(int)video_track->getVideoFps(), 1};
                _context->gop_size = (int)video_track->getVideoFps() * 2;
                _context->max_b_frames = 0;
                _context->bit_rate = video_track->getBitRate() > 0 ? video_track->getBitRate() : 2000000;
                _context->flags |= AV_CODEC_FLAG_LOW_DELAY;
                _context->pix_fmt = codec_default->pix_fmts ? codec_default->pix_fmts[0] : AV_PIX_FMT_YUV420P;
            } else if (track->getTrackType() == TrackAudio) {
                auto audio_track = std::static_pointer_cast<AudioTrack>(track);
                _context->sample_rate = audio_track->getAudioSampleRate();
                _context->time_base = {1, _context->sample_rate};
                _context->bit_rate = audio_track->getBitRate() > 0 ? audio_track->getBitRate() : 128000;
#if LIBAVCODEC_VERSION_INT >= FF_CODEC_VER_7_1
                av_channel_layout_default(&_context->ch_layout, audio_track->getAudioChannel());
#else
                _context->channels = audio_track->getAudioChannel();
                _context->channel_layout = av_get_default_channel_layout(_context->channels);
#endif
                _context->sample_fmt = codec_default->sample_fmts ? codec_default->sample_fmts[0] : AV_SAMPLE_FMT_S16;
            }
            
            ret = avcodec_open2(_context.get(), codec_default, nullptr);
        }
        
        if (ret < 0) {
            throw std::runtime_error("打开编码器失败: " + ffmpeg_err(ret));
        }
    }

    _opened = true;
    InfoL << "打开编码器成功: " << _context->codec->name 
          << ", codec_id=" << getCodecName(_codec_id);
}

FFmpegEncoder::~FFmpegEncoder() {
    stopThread(true);
    flush();
}

void FFmpegEncoder::flush() {
    if (!_opened) return;
    
    // 发送空包刷新编码器
    avcodec_send_frame(_context.get(), nullptr);
    
    auto pkt = alloc_av_packet();
    while (avcodec_receive_packet(_context.get(), pkt.get()) >= 0) {
        onEncode(pkt.get());
        av_packet_unref(pkt.get());
    }
}

const AVCodecContext *FFmpegEncoder::getContext() const {
    return _context.get();
}

bool FFmpegEncoder::inputFrame_l(const FFmpegFrame::Ptr &frame) {
    if (!_opened) return false;
    
    AVFrame *av_frame = frame->get();
    
    // 视频帧格式转换
    if (_context->codec_type == AVMEDIA_TYPE_VIDEO) {
        if (av_frame->format != _context->pix_fmt ||
            av_frame->width != _context->width ||
            av_frame->height != _context->height) {
            // 需要转换
            if (!_sws) {
                _sws = std::make_shared<FFmpegSws>(_context->pix_fmt, _context->width, _context->height);
            }
            auto new_frame = _sws->inputFrame(frame);
            if (!new_frame) {
                WarnL << "视频格式转换失败";
                return false;
            }
            av_frame = new_frame->get();
        }
    }
    // 音频帧格式转换
    else if (_context->codec_type == AVMEDIA_TYPE_AUDIO) {
#if LIBAVCODEC_VERSION_INT >= FF_CODEC_VER_7_1
        if (av_frame->format != _context->sample_fmt ||
            av_channel_layout_compare(&av_frame->ch_layout, &_context->ch_layout) ||
            av_frame->sample_rate != _context->sample_rate) {
            if (!_swr) {
                _swr = std::make_shared<FFmpegSwr>(_context->sample_fmt, &_context->ch_layout, _context->sample_rate);
            }
            auto new_frame = _swr->inputFrame(frame);
            if (!new_frame) {
                WarnL << "音频格式转换失败";
                return false;
            }
            av_frame = new_frame->get();
        }
#else
        if (av_frame->format != _context->sample_fmt ||
            av_frame->channels != _context->channels ||
            av_frame->sample_rate != _context->sample_rate) {
            if (!_swr) {
                _swr = std::make_shared<FFmpegSwr>(_context->sample_fmt, _context->channels, 
                                                   _context->channel_layout, _context->sample_rate);
            }
            auto new_frame = _swr->inputFrame(frame);
            if (!new_frame) {
                WarnL << "音频格式转换失败";
                return false;
            }
            av_frame = new_frame->get();
        }
#endif
    }

    int ret = avcodec_send_frame(_context.get(), av_frame);
    if (ret < 0) {
        if (ret != AVERROR(EAGAIN)) {
            WarnL << "avcodec_send_frame failed: " << ffmpeg_err(ret);
        }
        return false;
    }

    auto pkt = alloc_av_packet();
    while (true) {
        ret = avcodec_receive_packet(_context.get(), pkt.get());
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        }
        if (ret < 0) {
            WarnL << "avcodec_receive_packet failed: " << ffmpeg_err(ret);
            break;
        }
        onEncode(pkt.get());
        av_packet_unref(pkt.get());
    }
    return true;
}

bool FFmpegEncoder::inputFrame(const FFmpegFrame::Ptr &frame, bool async) {
    if (!_opened) return false;
    
    if (async && !TaskManager::isEnabled() && _context->codec_type == AVMEDIA_TYPE_VIDEO) {
        startThread("encoder thread");
    }

    if (!async || !TaskManager::isEnabled()) {
        return inputFrame_l(frame);
    }

    return addEncodeTask([this, frame]() {
        inputFrame_l(frame);
    });
}

void FFmpegEncoder::setOnEncode(onEnc cb) {
    _cb = std::move(cb);
}

void FFmpegEncoder::onEncode(const AVPacket *pkt) {
    if (_cb) {
        auto frame = makeFrame(pkt);
        if (frame) {
            _cb(frame);
        }
    }
}

Frame::Ptr FFmpegEncoder::makeFrame(const AVPacket *pkt) {
    // 根据编码类型创建对应的Frame
    bool is_video = _context->codec_type == AVMEDIA_TYPE_VIDEO;
    bool is_key = (pkt->flags & AV_PKT_FLAG_KEY) != 0;
    
    auto buffer = BufferRaw::create();
    buffer->assign((const char *)pkt->data, pkt->size);
    
    // 使用通用的Frame
    class EncodedFrame : public Frame {
    public:
        EncodedFrame(CodecId codec_id, bool is_video, bool is_key, 
                     uint64_t dts, uint64_t pts, Buffer::Ptr buffer)
            : _codec_id(codec_id), _is_video(is_video), _is_key(is_key),
              _dts(dts), _pts(pts), _buffer(std::move(buffer)) {}
        
        char *data() const override { return _buffer->data(); }
        size_t size() const override { return _buffer->size(); }
        uint64_t dts() const override { return _dts; }
        uint64_t pts() const override { return _pts; }
        size_t prefixSize() const override { return 0; }
        bool keyFrame() const override { return _is_key; }
        bool configFrame() const override { return false; }
        CodecId getCodecId() const override { return _codec_id; }
        
    private:
        CodecId _codec_id;
        bool _is_video;
        bool _is_key;
        uint64_t _dts;
        uint64_t _pts;
        Buffer::Ptr _buffer;
    };
    
    return std::make_shared<EncodedFrame>(
        _codec_id, is_video, is_key,
        pkt->dts, pkt->pts, buffer
    );
}

// 创建音频Track的辅助函数
Track::Ptr createAudioTrack(const AudioEncoderConfig &config) {
    // 使用AudioTrackImp或者对应的具体Track类
    // 这里返回一个通用的AudioTrack
    class AudioEncoderTrack : public AudioTrackImp {
    public:
        AudioEncoderTrack(const AudioEncoderConfig &cfg) 
            : AudioTrackImp(cfg.codec_id, cfg.sample_rate, cfg.channels, 16) {
            setBitRate(cfg.bit_rate);
        }
        bool ready() const override { return true; }
        Track::Ptr clone() const override { return std::make_shared<AudioEncoderTrack>(*this); }
    };
    return std::make_shared<AudioEncoderTrack>(config);
}

// 创建视频Track的辅助函数
Track::Ptr createVideoTrack(const VideoEncoderConfig &config) {
    class VideoEncoderTrack : public VideoTrackImp {
    public:
        VideoEncoderTrack(const VideoEncoderConfig &cfg)
            : VideoTrackImp(cfg.codec_id, cfg.width, cfg.height, cfg.fps) {
            setBitRate(cfg.bit_rate);
        }
        bool ready() const override { return true; }
        Track::Ptr clone() const override { return std::make_shared<VideoEncoderTrack>(*this); }
        Sdp::Ptr getSdp(uint8_t payload_type) const override { return nullptr; }
    };
    return std::make_shared<VideoEncoderTrack>(config);
}

} // namespace mediakit

#endif // ENABLE_FFMPEG
