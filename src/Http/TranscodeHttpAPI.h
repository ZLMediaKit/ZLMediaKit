/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TRANSCODE_HTTP_API_H
#define ZLMEDIAKIT_TRANSCODE_HTTP_API_H

#if defined(ENABLE_FFMPEG)

#include <string>
#include <map>
#include <memory>
#include <mutex>
#include "Codec/TranscodeAPI.h"
#include "Common/Parser.h"
#include "Http/HttpSession.h"

namespace mediakit {

/**
 * 转码HTTP API
 * 提供RESTful接口进行转码控制
 * 
 * API列表:
 * - POST /api/transcode/start   - 启动转码任务
 * - POST /api/transcode/stop    - 停止转码任务
 * - GET  /api/transcode/list    - 获取转码任务列表
 * - GET  /api/transcode/info    - 获取转码任务详情
 * - GET  /api/transcode/codecs  - 获取支持的编解码器
 */
class TranscodeHttpAPI {
public:
    using Ptr = std::shared_ptr<TranscodeHttpAPI>;
    
    /**
     * 获取单例
     */
    static TranscodeHttpAPI &Instance();
    
    /**
     * 注册HTTP API处理器
     */
    void registerApi();
    
    /**
     * 启动转码任务
     * @param params 参数:
     *   - src_url: 源流URL
     *   - video_codec: 目标视频编码 (h264/h265/vp8/vp9/av1)
     *   - audio_codec: 目标音频编码 (aac/opus/g711a/g711u/mp3)
     *   - video_width: 目标视频宽度 (可选)
     *   - video_height: 目标视频高度 (可选)
     *   - video_fps: 目标帧率 (可选)
     *   - video_bitrate: 视频比特率 (可选)
     *   - audio_sample_rate: 音频采样率 (可选)
     *   - audio_channels: 音频通道数 (可选)
     *   - audio_bitrate: 音频比特率 (可选)
     *   - dst_url: 目标推流URL (可选)
     *   - vhost/app/stream: 注册的流名 (可选)
     * @return JSON响应
     */
    std::string startTranscode(const HttpSession::KeyValue &params);
    
    /**
     * 停止转码任务
     * @param params 参数:
     *   - task_id: 任务ID
     */
    std::string stopTranscode(const HttpSession::KeyValue &params);
    
    /**
     * 获取转码任务列表
     */
    std::string listTranscode();
    
    /**
     * 获取转码任务详情
     * @param params 参数:
     *   - task_id: 任务ID
     */
    std::string getTranscodeInfo(const HttpSession::KeyValue &params);
    
    /**
     * 获取支持的编解码器
     */
    std::string getSupportedCodecs();

private:
    TranscodeHttpAPI() = default;
    
    std::string generateTaskId();
    std::string makeResponse(int code, const std::string &msg, const std::string &data = "");

private:
    struct TranscodeTask {
        std::string task_id;
        std::string src_url;
        std::string dst_url;
        std::string video_codec;
        std::string audio_codec;
        std::string vhost;
        std::string app;
        std::string stream;
        uint64_t start_time;
        TranscodeAPI::Ptr api;
    };
    
    std::mutex _mtx;
    std::map<std::string, std::shared_ptr<TranscodeTask>> _tasks;
    uint64_t _task_counter = 0;
};

/**
 * 注册转码HTTP API
 * 在main函数中调用此函数即可启用转码API
 */
void registerTranscodeHttpAPI();

} // namespace mediakit

#endif // ENABLE_FFMPEG
#endif // ZLMEDIAKIT_TRANSCODE_HTTP_API_H
