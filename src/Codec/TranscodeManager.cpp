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

#include "TranscodeManager.h"
#include "Util/logger.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

//////////////////////////////////////////////////////////////////
// TrackTranscoder
//////////////////////////////////////////////////////////////////

TrackTranscoder::TrackTranscoder(const Track::Ptr &src_track, const Track::Ptr &dst_track,
                                 int thread_num, const std::vector<std::string> &decoder_names,
                                 const std::vector<std::string> &encoder_names) {
    _src_track = src_track;
    _dst_track = dst_track;
    
    // 检查是否需要转码
    _need_transcode = (src_track->getCodecId() != dst_track->getCodecId());
    
    if (!_need_transcode) {
        // 同一编码，检查是否需要进行参数变换（如分辨率、采样率等）
        if (src_track->getTrackType() == TrackVideo) {
            auto src_video = static_pointer_cast<VideoTrack>(src_track);
            auto dst_video = static_pointer_cast<VideoTrack>(dst_track);
            if ((dst_video->getVideoWidth() > 0 && dst_video->getVideoWidth() != src_video->getVideoWidth()) ||
                (dst_video->getVideoHeight() > 0 && dst_video->getVideoHeight() != src_video->getVideoHeight())) {
                _need_transcode = true;
            }
        } else if (src_track->getTrackType() == TrackAudio) {
            auto src_audio = static_pointer_cast<AudioTrack>(src_track);
            auto dst_audio = static_pointer_cast<AudioTrack>(dst_track);
            if ((dst_audio->getAudioSampleRate() > 0 && dst_audio->getAudioSampleRate() != src_audio->getAudioSampleRate()) ||
                (dst_audio->getAudioChannel() > 0 && dst_audio->getAudioChannel() != src_audio->getAudioChannel())) {
                _need_transcode = true;
            }
        }
    }
    
    if (_need_transcode) {
        InfoL << "创建转码器: " << getCodecName(src_track->getCodecId()) 
              << " -> " << getCodecName(dst_track->getCodecId());
        
        // 创建解码器
        _decoder = make_shared<FFmpegDecoder>(src_track, thread_num, decoder_names);
        _decoder->setOnDecode([this](const FFmpegFrame::Ptr &frame) {
            onDecoded(frame);
        });
        
        // 创建编码器
        _encoder = make_shared<FFmpegEncoder>(dst_track, thread_num, encoder_names);
        _encoder->setOnEncode([this](const Frame::Ptr &frame) {
            onEncoded(frame);
        });
    } else {
        InfoL << "无需转码，直接透传: " << getCodecName(src_track->getCodecId());
    }
}

TrackTranscoder::~TrackTranscoder() {
    InfoL << "销毁转码器: " << getCodecName(_src_track->getCodecId()) 
          << " -> " << getCodecName(_dst_track->getCodecId());
}

bool TrackTranscoder::inputFrame(const Frame::Ptr &frame) {
    if (!_need_transcode) {
        // 不需要转码，直接透传
        if (_on_frame) {
            _on_frame(frame);
        }
        return true;
    }
    
    if (!_decoder) {
        return false;
    }
    
    return _decoder->inputFrame(frame, true, true, true);
}

void TrackTranscoder::setOnFrame(onFrame cb) {
    _on_frame = std::move(cb);
}

void TrackTranscoder::onDecoded(const FFmpegFrame::Ptr &frame) {
    if (_encoder) {
        _encoder->inputFrame(frame, true);
    }
}

void TrackTranscoder::onEncoded(const Frame::Ptr &frame) {
    if (_on_frame) {
        _on_frame(frame);
    }
}

//////////////////////////////////////////////////////////////////
// TranscodeManager
//////////////////////////////////////////////////////////////////

TranscodeManager::TranscodeManager(const TranscodeConfig &config) : _config(config) {
    InfoL << "创建转码管理器";
}

TranscodeManager::~TranscodeManager() {
    resetTracks();
    InfoL << "销毁转码管理器";
}

bool TranscodeManager::addTrack(const Track::Ptr &track) {
    lock_guard<mutex> lck(_mtx);
    
    if (!track) {
        return false;
    }
    
    // 检查是否启用该类型的转码
    if (track->getTrackType() == TrackVideo && !_config.enable_video) {
        InfoL << "视频转码未启用，跳过视频Track";
        return false;
    }
    if (track->getTrackType() == TrackAudio && !_config.enable_audio) {
        InfoL << "音频转码未启用，跳过音频Track";
        return false;
    }
    
    // 创建转码器
    auto transcoder = createTranscoder(track);
    if (!transcoder) {
        WarnL << "创建转码器失败: " << getCodecName(track->getCodecId());
        return false;
    }
    
    _transcoders[track->getIndex()] = transcoder;
    _output_tracks.push_back(transcoder->getDstTrack());
    
    if (_on_track_ready) {
        _on_track_ready(transcoder->getDstTrack());
    }
    
    return true;
}

void TranscodeManager::addTrackCompleted() {
    InfoL << "所有Track添加完成，共 " << _transcoders.size() << " 个转码器";
}

void TranscodeManager::resetTracks() {
    lock_guard<mutex> lck(_mtx);
    _transcoders.clear();
    _output_tracks.clear();
    InfoL << "重置所有Track";
}

bool TranscodeManager::inputFrame(const Frame::Ptr &frame) {
    if (!frame) {
        return false;
    }
    
    lock_guard<mutex> lck(_mtx);
    
    auto it = _transcoders.find(frame->getIndex());
    if (it == _transcoders.end()) {
        // 没有找到对应的转码器，尝试用track type查找
        for (auto &pair : _transcoders) {
            if ((frame->getTrackType() == TrackVideo && pair.second->getSrcTrack()->getTrackType() == TrackVideo) ||
                (frame->getTrackType() == TrackAudio && pair.second->getSrcTrack()->getTrackType() == TrackAudio)) {
                if (frame->getCodecId() == pair.second->getSrcTrack()->getCodecId()) {
                    return pair.second->inputFrame(frame);
                }
            }
        }
        return false;
    }
    
    return it->second->inputFrame(frame);
}

void TranscodeManager::setOnFrame(onFrame cb) {
    _on_frame = std::move(cb);
}

void TranscodeManager::setOnTrackReady(onTrackReady cb) {
    _on_track_ready = std::move(cb);
}

std::vector<Track::Ptr> TranscodeManager::getOutputTracks() const {
    lock_guard<mutex> lck(const_cast<mutex&>(_mtx));
    return _output_tracks;
}

void TranscodeManager::setConfig(const TranscodeConfig &config) {
    lock_guard<mutex> lck(_mtx);
    _config = config;
}

Track::Ptr TranscodeManager::createOutputTrack(const Track::Ptr &src_track) {
    if (src_track->getTrackType() == TrackVideo) {
        auto src_video = static_pointer_cast<VideoTrack>(src_track);
        VideoEncoderConfig cfg;
        cfg.codec_id = _config.video_codec;
        cfg.width = _config.video_width > 0 ? _config.video_width : src_video->getVideoWidth();
        cfg.height = _config.video_height > 0 ? _config.video_height : src_video->getVideoHeight();
        cfg.fps = _config.video_fps > 0 ? _config.video_fps : (int)src_video->getVideoFps();
        cfg.bit_rate = _config.video_bitrate > 0 ? _config.video_bitrate : src_video->getBitRate();
        cfg.encoder_name = _config.video_encoder_name;
        
        if (cfg.width <= 0) cfg.width = 1920;
        if (cfg.height <= 0) cfg.height = 1080;
        if (cfg.fps <= 0) cfg.fps = 25;
        if (cfg.bit_rate <= 0) cfg.bit_rate = 2000000;
        
        return createVideoTrack(cfg);
    } else if (src_track->getTrackType() == TrackAudio) {
        auto src_audio = static_pointer_cast<AudioTrack>(src_track);
        AudioEncoderConfig cfg;
        cfg.codec_id = _config.audio_codec;
        cfg.sample_rate = _config.audio_sample_rate > 0 ? _config.audio_sample_rate : src_audio->getAudioSampleRate();
        cfg.channels = _config.audio_channels > 0 ? _config.audio_channels : src_audio->getAudioChannel();
        cfg.bit_rate = _config.audio_bitrate > 0 ? _config.audio_bitrate : src_audio->getBitRate();
        cfg.encoder_name = _config.audio_encoder_name;
        
        if (cfg.sample_rate <= 0) cfg.sample_rate = 44100;
        if (cfg.channels <= 0) cfg.channels = 2;
        if (cfg.bit_rate <= 0) cfg.bit_rate = 128000;
        
        return createAudioTrack(cfg);
    }
    return nullptr;
}

TrackTranscoder::Ptr TranscodeManager::createTranscoder(const Track::Ptr &src_track) {
    auto dst_track = createOutputTrack(src_track);
    if (!dst_track) {
        return nullptr;
    }
    
    dst_track->setIndex(src_track->getIndex());
    
    vector<string> decoder_names, encoder_names;
    if (!_config.video_encoder_name.empty() && src_track->getTrackType() == TrackVideo) {
        encoder_names.push_back(_config.video_encoder_name);
    }
    if (!_config.audio_encoder_name.empty() && src_track->getTrackType() == TrackAudio) {
        encoder_names.push_back(_config.audio_encoder_name);
    }
    
    auto transcoder = make_shared<TrackTranscoder>(
        src_track, dst_track, _config.thread_num, decoder_names, encoder_names);
    
    transcoder->setOnFrame([this](const Frame::Ptr &frame) {
        if (_on_frame) {
            _on_frame(frame);
        }
    });
    
    return transcoder;
}

//////////////////////////////////////////////////////////////////
// CodecSupport
//////////////////////////////////////////////////////////////////

static std::vector<CodecId> s_video_codecs = {
    CodecH264, CodecH265, CodecVP8, CodecVP9, CodecAV1, CodecJPEG
};

static std::vector<CodecId> s_audio_codecs = {
    CodecAAC, CodecOpus, CodecG711A, CodecG711U, CodecL16
};

bool CodecSupport::isEncoderSupported(CodecId codec) {
    AVCodecID av_id = getAVCodecID(codec);
    if (av_id == AV_CODEC_ID_NONE) {
        return false;
    }
    return avcodec_find_encoder(av_id) != nullptr;
}

bool CodecSupport::isDecoderSupported(CodecId codec) {
    AVCodecID av_id = getAVCodecID(codec);
    if (av_id == AV_CODEC_ID_NONE) {
        return false;
    }
    return avcodec_find_decoder(av_id) != nullptr;
}

std::vector<CodecId> CodecSupport::getSupportedEncoders() {
    std::vector<CodecId> result;
    for (auto codec : s_video_codecs) {
        if (isEncoderSupported(codec)) {
            result.push_back(codec);
        }
    }
    for (auto codec : s_audio_codecs) {
        if (isEncoderSupported(codec)) {
            result.push_back(codec);
        }
    }
    return result;
}

std::vector<CodecId> CodecSupport::getSupportedDecoders() {
    std::vector<CodecId> result;
    for (auto codec : s_video_codecs) {
        if (isDecoderSupported(codec)) {
            result.push_back(codec);
        }
    }
    for (auto codec : s_audio_codecs) {
        if (isDecoderSupported(codec)) {
            result.push_back(codec);
        }
    }
    return result;
}

void CodecSupport::printSupportedCodecs() {
    InfoL << "=== 支持的编码器 ===";
    InfoL << "视频编码器:";
    for (auto codec : s_video_codecs) {
        if (isEncoderSupported(codec)) {
            InfoL << "  - " << getCodecName(codec);
        }
    }
    InfoL << "音频编码器:";
    for (auto codec : s_audio_codecs) {
        if (isEncoderSupported(codec)) {
            InfoL << "  - " << getCodecName(codec);
        }
    }
    
    InfoL << "=== 支持的解码器 ===";
    InfoL << "视频解码器:";
    for (auto codec : s_video_codecs) {
        if (isDecoderSupported(codec)) {
            InfoL << "  - " << getCodecName(codec);
        }
    }
    InfoL << "音频解码器:";
    for (auto codec : s_audio_codecs) {
        if (isDecoderSupported(codec)) {
            InfoL << "  - " << getCodecName(codec);
        }
    }
}

} // namespace mediakit

#endif // ENABLE_FFMPEG
