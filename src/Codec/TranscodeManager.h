/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TRANSCODEMANAGER_H
#define ZLMEDIAKIT_TRANSCODEMANAGER_H

#if defined(ENABLE_FFMPEG)

#include <map>
#include <memory>
#include <functional>
#include "Transcode.h"
#include "FFmpegEncoder.h"
#include "Common/MediaSink.h"

namespace mediakit {

/**
 * 转码器配置
 */
struct TranscodeConfig {
    // 视频转码配置
    bool enable_video = true;
    CodecId video_codec = CodecH264;
    int video_width = 0;      // 0表示保持原始
    int video_height = 0;
    int video_fps = 0;
    int video_bitrate = 0;
    std::string video_encoder_name;  // 指定编码器名称
    
    // 音频转码配置
    bool enable_audio = true;
    CodecId audio_codec = CodecAAC;
    int audio_sample_rate = 0;  // 0表示保持原始
    int audio_channels = 0;
    int audio_bitrate = 0;
    std::string audio_encoder_name;
    
    // 是否异步转码
    bool async = true;
    
    // 转码线程数
    int thread_num = 2;
};

/**
 * 单轨转码器 - 负责一个Track的转码
 */
class TrackTranscoder : public std::enable_shared_from_this<TrackTranscoder> {
public:
    using Ptr = std::shared_ptr<TrackTranscoder>;
    using onFrame = std::function<void(const Frame::Ptr &)>;
    
    TrackTranscoder(const Track::Ptr &src_track, const Track::Ptr &dst_track, 
                    int thread_num = 2, const std::vector<std::string> &decoder_names = {},
                    const std::vector<std::string> &encoder_names = {});
    ~TrackTranscoder();
    
    /**
     * 输入原始编码帧
     */
    bool inputFrame(const Frame::Ptr &frame);
    
    /**
     * 设置转码完成回调
     */
    void setOnFrame(onFrame cb);
    
    /**
     * 获取源Track
     */
    Track::Ptr getSrcTrack() const { return _src_track; }
    
    /**
     * 获取目标Track
     */
    Track::Ptr getDstTrack() const { return _dst_track; }
    
    /**
     * 是否需要转码（源和目标codec相同则不需要）
     */
    bool needTranscode() const { return _need_transcode; }

private:
    void onDecoded(const FFmpegFrame::Ptr &frame);
    void onEncoded(const Frame::Ptr &frame);

private:
    bool _need_transcode = false;
    Track::Ptr _src_track;
    Track::Ptr _dst_track;
    FFmpegDecoder::Ptr _decoder;
    FFmpegEncoder::Ptr _encoder;
    onFrame _on_frame;
};

/**
 * 转码管理器 - 管理完整的音视频转码流程
 * 支持: H265/H264/Opus/G711/AAC/G722/MP3/SVAC/VP8/VP9/AV1等
 */
class TranscodeManager : public MediaSinkInterface {
public:
    using Ptr = std::shared_ptr<TranscodeManager>;
    using onFrame = std::function<void(const Frame::Ptr &)>;
    using onTrackReady = std::function<void(const Track::Ptr &)>;
    
    TranscodeManager(const TranscodeConfig &config = TranscodeConfig());
    ~TranscodeManager() override;
    
    /**
     * 添加源Track
     */
    bool addTrack(const Track::Ptr &track) override;
    
    /**
     * Track添加完成
     */
    void addTrackCompleted() override;
    
    /**
     * 重置
     */
    void resetTracks() override;
    
    /**
     * 输入帧
     */
    bool inputFrame(const Frame::Ptr &frame) override;
    
    /**
     * 设置转码后帧回调
     */
    void setOnFrame(onFrame cb);
    
    /**
     * 设置转码后Track准备好回调
     */
    void setOnTrackReady(onTrackReady cb);
    
    /**
     * 获取所有转码后的Track
     */
    std::vector<Track::Ptr> getOutputTracks() const;
    
    /**
     * 获取配置
     */
    const TranscodeConfig &getConfig() const { return _config; }
    
    /**
     * 动态修改转码配置
     */
    void setConfig(const TranscodeConfig &config);

private:
    Track::Ptr createOutputTrack(const Track::Ptr &src_track);
    TrackTranscoder::Ptr createTranscoder(const Track::Ptr &src_track);

private:
    TranscodeConfig _config;
    std::mutex _mtx;
    std::map<int, TrackTranscoder::Ptr> _transcoders;  // track index -> transcoder
    std::vector<Track::Ptr> _output_tracks;
    onFrame _on_frame;
    onTrackReady _on_track_ready;
};

/**
 * 支持的编解码器列表
 */
class CodecSupport {
public:
    /**
     * 检查是否支持某个编解码器
     */
    static bool isEncoderSupported(CodecId codec);
    static bool isDecoderSupported(CodecId codec);
    
    /**
     * 获取所有支持的编码器
     */
    static std::vector<CodecId> getSupportedEncoders();
    
    /**
     * 获取所有支持的解码器
     */
    static std::vector<CodecId> getSupportedDecoders();
    
    /**
     * 打印支持的编解码器信息
     */
    static void printSupportedCodecs();
};

} // namespace mediakit

#endif // ENABLE_FFMPEG
#endif // ZLMEDIAKIT_TRANSCODEMANAGER_H
