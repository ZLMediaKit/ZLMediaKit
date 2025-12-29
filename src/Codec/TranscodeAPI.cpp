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

#include "TranscodeAPI.h"
#include "Util/logger.h"
#include "Player/PlayerProxy.h"
#include "Pusher/MediaPusher.h"
#include "Common/MultiMediaSourceMuxer.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

// 编码器名称映射表
static const std::map<std::string, CodecId> s_codec_map = {
    // 视频
    {"h264", CodecH264},
    {"h.264", CodecH264},
    {"avc", CodecH264},
    {"h265", CodecH265},
    {"h.265", CodecH265},
    {"hevc", CodecH265},
    {"vp8", CodecVP8},
    {"vp9", CodecVP9},
    {"av1", CodecAV1},
    {"jpeg", CodecJPEG},
    {"mjpeg", CodecJPEG},
    // 音频
    {"aac", CodecAAC},
    {"opus", CodecOpus},
    {"g711a", CodecG711A},
    {"pcma", CodecG711A},
    {"g711u", CodecG711U},
    {"pcmu", CodecG711U},
    {"l16", CodecL16},
    {"pcm", CodecL16},
};

// 转码API实现类
class TranscodeAPIImpl : public TranscodeAPI {
public:
    TranscodeAPIImpl(const TranscodeConfig &config) : _config(config) {
        _manager = make_shared<TranscodeManager>(config);
    }
    
    ~TranscodeAPIImpl() override {
        stop();
    }
    
    bool setSource(const std::string &url) override {
        _src_url = url;
        return true;
    }
    
    bool addSourceTrack(const Track::Ptr &track) override {
        if (!track) return false;
        return _manager->addTrack(track);
    }
    
    bool inputFrame(const Frame::Ptr &frame) override {
        if (!frame) return false;
        return _manager->inputFrame(frame);
    }
    
    void setTargetCodec(const std::string &video_codec, const std::string &audio_codec) override {
        if (!video_codec.empty()) {
            _config.video_codec = parseCodecId(video_codec);
        }
        if (!audio_codec.empty()) {
            _config.audio_codec = parseCodecId(audio_codec);
        }
        _manager->setConfig(_config);
    }
    
    void setVideoParams(int width, int height, int fps, int bitrate) override {
        _config.video_width = width;
        _config.video_height = height;
        _config.video_fps = fps;
        _config.video_bitrate = bitrate;
        _manager->setConfig(_config);
    }
    
    void setAudioParams(int sample_rate, int channels, int bitrate) override {
        _config.audio_sample_rate = sample_rate;
        _config.audio_channels = channels;
        _config.audio_bitrate = bitrate;
        _manager->setConfig(_config);
    }
    
    void setOnFrame(onFrame cb) override {
        _on_frame = std::move(cb);
        _manager->setOnFrame([this](const Frame::Ptr &frame) {
            if (_on_frame) {
                _on_frame(frame);
            }
            // 转发到推流器和MediaSource
            if (_muxer) {
                _muxer->inputFrame(frame);
            }
        });
    }
    
    void setOnTrack(onTrack cb) override {
        _on_track = std::move(cb);
        _manager->setOnTrackReady([this](const Track::Ptr &track) {
            if (_on_track) {
                _on_track(track);
            }
            if (_muxer) {
                _muxer->addTrack(track);
            }
        });
    }
    
    void setOnError(onError cb) override {
        _on_error = std::move(cb);
    }
    
    bool start() override {
        if (_src_url.empty()) {
            // 没有源URL，等待手动输入帧
            _running = true;
            return true;
        }
        
        // 使用PlayerProxy拉取源流
        MediaTuple tuple;
        tuple.vhost = DEFAULT_VHOST;
        tuple.app = "transcode_src";
        tuple.stream = "src_" + to_string(time(nullptr));
        
        ProtocolOption option;
        option.enable_audio = _config.enable_audio;
        option.enable_hls = false;
        option.enable_rtsp = false;
        option.enable_rtmp = false;
        option.enable_fmp4 = false;
        option.enable_ts = false;
        option.enable_mp4 = false;
        
        _player = make_shared<PlayerProxy>(tuple, option, 3);
        
        weak_ptr<TranscodeAPIImpl> weak_self = 
            dynamic_pointer_cast<TranscodeAPIImpl>(shared_from_this());
        
        _player->setPlayCallbackOnce([weak_self](const SockException &ex) {
            auto strong_self = weak_self.lock();
            if (!strong_self) return;
            
            if (ex) {
                if (strong_self->_on_error) {
                    strong_self->_on_error(string("播放失败: ") + ex.what());
                }
                return;
            }
            
            // 获取Track并添加到转码器
            auto tracks = strong_self->_player->getTracks();
            for (auto &track : tracks) {
                strong_self->_manager->addTrack(track);
            }
            strong_self->_manager->addTrackCompleted();
        });
        
        _player->setOnClose([weak_self](const SockException &ex) {
            auto strong_self = weak_self.lock();
            if (!strong_self) return;
            
            if (strong_self->_on_error) {
                strong_self->_on_error(string("播放中断: ") + ex.what());
            }
        });
        
        _player->play(_src_url);
        _running = true;
        return true;
    }
    
    void stop() override {
        _running = false;
        if (_player) {
            _player = nullptr;
        }
        if (_pusher) {
            _pusher = nullptr;
        }
        if (_muxer) {
            _muxer = nullptr;
        }
        _manager->resetTracks();
    }
    
    std::vector<Track::Ptr> getOutputTracks() const override {
        return _manager->getOutputTracks();
    }
    
    bool pushTo(const std::string &url) override {
        _dst_url = url;
        // 推流将在Track准备好后启动
        return true;
    }
    
    bool regist(const std::string &vhost, const std::string &app, const std::string &stream) override {
        ProtocolOption option;
        option.enable_audio = _config.enable_audio;
        option.enable_hls = true;
        option.enable_rtsp = true;
        option.enable_rtmp = true;
        option.enable_fmp4 = true;
        option.enable_ts = true;
        
        MediaTuple tuple;
        tuple.vhost = vhost;
        tuple.app = app;
        tuple.stream = stream;
        
        _muxer = make_shared<MultiMediaSourceMuxer>(tuple, 0, option);
        
        // 添加已有的Track
        for (auto &track : _manager->getOutputTracks()) {
            _muxer->addTrack(track);
        }
        
        return true;
    }

private:
    bool _running = false;
    std::string _src_url;
    std::string _dst_url;
    TranscodeConfig _config;
    TranscodeManager::Ptr _manager;
    PlayerProxy::Ptr _player;
    MediaPusher::Ptr _pusher;
    MultiMediaSourceMuxer::Ptr _muxer;
    onFrame _on_frame;
    onTrack _on_track;
    onError _on_error;
};

// ============ TranscodeAPI 静态方法实现 ============

TranscodeAPI::Ptr TranscodeAPI::create(const TranscodeConfig &config) {
    return make_shared<TranscodeAPIImpl>(config);
}

CodecId TranscodeAPI::parseCodecId(const std::string &codec_name) {
    string lower_name = codec_name;
    transform(lower_name.begin(), lower_name.end(), lower_name.begin(), ::tolower);
    
    auto it = s_codec_map.find(lower_name);
    if (it != s_codec_map.end()) {
        return it->second;
    }
    return CodecInvalid;
}

std::string TranscodeAPI::getCodecName(CodecId codec_id) {
    return mediakit::getCodecName(codec_id);
}

std::vector<std::string> TranscodeAPI::getSupportedVideoCodecs() {
    std::vector<std::string> result;
    std::vector<CodecId> video_codecs = {CodecH264, CodecH265, CodecVP8, CodecVP9, CodecAV1, CodecJPEG};
    
    for (auto codec : video_codecs) {
        if (CodecSupport::isEncoderSupported(codec)) {
            result.push_back(getCodecName(codec));
        }
    }
    return result;
}

std::vector<std::string> TranscodeAPI::getSupportedAudioCodecs() {
    std::vector<std::string> result;
    std::vector<CodecId> audio_codecs = {CodecAAC, CodecOpus, CodecG711A, CodecG711U, CodecL16};
    
    for (auto codec : audio_codecs) {
        if (CodecSupport::isEncoderSupported(codec)) {
            result.push_back(getCodecName(codec));
        }
    }
    return result;
}

bool TranscodeAPI::isCodecSupported(const std::string &codec_name) {
    CodecId codec_id = parseCodecId(codec_name);
    if (codec_id == CodecInvalid) {
        return false;
    }
    return CodecSupport::isEncoderSupported(codec_id) && CodecSupport::isDecoderSupported(codec_id);
}

void TranscodeAPI::printSupportInfo() {
    InfoL << "========================================";
    InfoL << "ZLMediaKit Ultra - 音视频转码支持列表";
    InfoL << "========================================";
    
    InfoL << "\n支持的视频编码器:";
    for (const auto &codec : getSupportedVideoCodecs()) {
        InfoL << "  - " << codec;
    }
    
    InfoL << "\n支持的音频编码器:";
    for (const auto &codec : getSupportedAudioCodecs()) {
        InfoL << "  - " << codec;
    }
    
    InfoL << "\n转码能力:";
    InfoL << "  视频: H264 <-> H265 <-> VP8 <-> VP9 <-> AV1";
    InfoL << "  音频: AAC <-> Opus <-> G711A <-> G711U <-> MP3";
    InfoL << "========================================";
}

// ============ 快捷转码函数实现 ============

// 缓存的转码器
static std::map<std::pair<CodecId, CodecId>, std::shared_ptr<TrackTranscoder>> s_video_transcoders;
static std::map<std::pair<CodecId, CodecId>, std::shared_ptr<TrackTranscoder>> s_audio_transcoders;
static std::mutex s_transcoder_mutex;

void transcodeVideo(CodecId src_codec, CodecId dst_codec,
                   const Frame::Ptr &frame,
                   const std::function<void(const Frame::Ptr &)> &callback) {
    if (!frame || !callback) return;
    
    if (src_codec == dst_codec) {
        // 无需转码
        callback(frame);
        return;
    }
    
    std::lock_guard<std::mutex> lck(s_transcoder_mutex);
    
    auto key = std::make_pair(src_codec, dst_codec);
    auto it = s_video_transcoders.find(key);
    
    if (it == s_video_transcoders.end()) {
        // 创建新的转码器
        VideoEncoderConfig cfg;
        cfg.codec_id = dst_codec;
        
        auto src_track = std::make_shared<VideoTrackImp>(src_codec, 1920, 1080, 25);
        auto dst_track = createVideoTrack(cfg);
        
        auto transcoder = std::make_shared<TrackTranscoder>(src_track, dst_track);
        transcoder->setOnFrame(callback);
        
        s_video_transcoders[key] = transcoder;
        it = s_video_transcoders.find(key);
    }
    
    it->second->setOnFrame(callback);
    it->second->inputFrame(frame);
}

void transcodeAudio(CodecId src_codec, CodecId dst_codec,
                   const Frame::Ptr &frame,
                   const std::function<void(const Frame::Ptr &)> &callback) {
    if (!frame || !callback) return;
    
    if (src_codec == dst_codec) {
        // 无需转码
        callback(frame);
        return;
    }
    
    std::lock_guard<std::mutex> lck(s_transcoder_mutex);
    
    auto key = std::make_pair(src_codec, dst_codec);
    auto it = s_audio_transcoders.find(key);
    
    if (it == s_audio_transcoders.end()) {
        // 创建新的转码器
        AudioEncoderConfig cfg;
        cfg.codec_id = dst_codec;
        
        auto src_track = std::make_shared<AudioTrackImp>(src_codec, 44100, 2, 16);
        auto dst_track = createAudioTrack(cfg);
        
        auto transcoder = std::make_shared<TrackTranscoder>(src_track, dst_track);
        transcoder->setOnFrame(callback);
        
        s_audio_transcoders[key] = transcoder;
        it = s_audio_transcoders.find(key);
    }
    
    it->second->setOnFrame(callback);
    it->second->inputFrame(frame);
}

bool transcodeStream(const std::string &src_url, const std::string &dst_url,
                    const std::string &video_codec,
                    const std::string &audio_codec) {
    TranscodeConfig config;
    config.video_codec = TranscodeAPI::parseCodecId(video_codec);
    config.audio_codec = TranscodeAPI::parseCodecId(audio_codec);
    
    auto api = TranscodeAPI::create(config);
    if (!api) {
        return false;
    }
    
    api->setSource(src_url);
    api->pushTo(dst_url);
    
    api->setOnError([](const std::string &err) {
        WarnL << "转码错误: " << err;
    });
    
    return api->start();
}

} // namespace mediakit

#endif // ENABLE_FFMPEG
