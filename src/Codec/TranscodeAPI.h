/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TRANSCODE_API_H
#define ZLMEDIAKIT_TRANSCODE_API_H

#if defined(ENABLE_FFMPEG)

#include <string>
#include <vector>
#include <functional>
#include <memory>

#include "Codec/TranscodeManager.h"
#include "Common/MediaSource.h"

namespace mediakit {

/**
 * 转码API - 提供简洁的音视频转码接口
 * 
 * 支持的编解码器:
 * 视频: H264, H265, VP8, VP9, AV1, SVAC1, SVAC2, JPEG
 * 音频: AAC, Opus, G711A, G711U, G722, G722.1, MP3, L16
 */
class TranscodeAPI : public std::enable_shared_from_this<TranscodeAPI> {
public:
    using Ptr = std::shared_ptr<TranscodeAPI>;
    using onFrame = std::function<void(const Frame::Ptr &)>;
    using onTrack = std::function<void(const Track::Ptr &)>;
    using onError = std::function<void(const std::string &)>;
    
    /**
     * 创建转码器实例
     * @param config 转码配置
     */
    static Ptr create(const TranscodeConfig &config = TranscodeConfig());
    
    virtual ~TranscodeAPI() = default;
    
    /**
     * 设置源流（通过URL拉流）
     * @param url 源流URL（支持rtsp/rtmp/http-flv等）
     */
    virtual bool setSource(const std::string &url) = 0;
    
    /**
     * 设置源Track（直接输入Track）
     * @param track 源Track
     */
    virtual bool addSourceTrack(const Track::Ptr &track) = 0;
    
    /**
     * 输入帧
     * @param frame 编码帧
     */
    virtual bool inputFrame(const Frame::Ptr &frame) = 0;
    
    /**
     * 设置目标编码器
     * @param video_codec 目标视频编码（如 "h264", "h265", "vp8", "vp9", "av1"）
     * @param audio_codec 目标音频编码（如 "aac", "opus", "g711a", "g711u", "mp3"）
     */
    virtual void setTargetCodec(const std::string &video_codec, const std::string &audio_codec) = 0;
    
    /**
     * 设置视频参数
     */
    virtual void setVideoParams(int width, int height, int fps, int bitrate) = 0;
    
    /**
     * 设置音频参数
     */
    virtual void setAudioParams(int sample_rate, int channels, int bitrate) = 0;
    
    /**
     * 设置回调
     */
    virtual void setOnFrame(onFrame cb) = 0;
    virtual void setOnTrack(onTrack cb) = 0;
    virtual void setOnError(onError cb) = 0;
    
    /**
     * 开始转码
     */
    virtual bool start() = 0;
    
    /**
     * 停止转码
     */
    virtual void stop() = 0;
    
    /**
     * 获取输出Track
     */
    virtual std::vector<Track::Ptr> getOutputTracks() const = 0;
    
    /**
     * 推送到目标地址
     * @param url 目标URL（支持rtsp/rtmp等）
     */
    virtual bool pushTo(const std::string &url) = 0;
    
    /**
     * 注册为MediaSource
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream 流名
     */
    virtual bool regist(const std::string &vhost, const std::string &app, const std::string &stream) = 0;
    
    // ============ 静态工具方法 ============
    
    /**
     * 解析编码器名称到CodecId
     */
    static CodecId parseCodecId(const std::string &codec_name);
    
    /**
     * 获取编码器名称
     */
    static std::string getCodecName(CodecId codec_id);
    
    /**
     * 获取所有支持的视频编码器
     */
    static std::vector<std::string> getSupportedVideoCodecs();
    
    /**
     * 获取所有支持的音频编码器
     */
    static std::vector<std::string> getSupportedAudioCodecs();
    
    /**
     * 检查是否支持某个编码器
     */
    static bool isCodecSupported(const std::string &codec_name);
    
    /**
     * 打印支持的编解码器
     */
    static void printSupportInfo();
};

/**
 * 快捷转码函数
 */

/**
 * 视频转码 - 从一种格式转换到另一种
 * @param src_codec 源编码
 * @param dst_codec 目标编码
 * @param frame 输入帧
 * @param callback 输出回调
 */
void transcodeVideo(CodecId src_codec, CodecId dst_codec,
                   const Frame::Ptr &frame,
                   const std::function<void(const Frame::Ptr &)> &callback);

/**
 * 音频转码 - 从一种格式转换到另一种
 * @param src_codec 源编码
 * @param dst_codec 目标编码
 * @param frame 输入帧
 * @param callback 输出回调
 */
void transcodeAudio(CodecId src_codec, CodecId dst_codec,
                   const Frame::Ptr &frame,
                   const std::function<void(const Frame::Ptr &)> &callback);

/**
 * 流转码 - 完整的流转码处理
 * @param src_url 源流URL
 * @param dst_url 目标流URL
 * @param video_codec 目标视频编码
 * @param audio_codec 目标音频编码
 * @return 是否成功
 */
bool transcodeStream(const std::string &src_url, const std::string &dst_url,
                    const std::string &video_codec = "h264",
                    const std::string &audio_codec = "aac");

} // namespace mediakit

#endif // ENABLE_FFMPEG
#endif // ZLMEDIAKIT_TRANSCODE_API_H
