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

#include "TranscodeHttpAPI.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Common/config.h"
#include "json/json.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

TranscodeHttpAPI &TranscodeHttpAPI::Instance() {
    static TranscodeHttpAPI instance;
    return instance;
}

void TranscodeHttpAPI::registerApi() {
    // HTTP API需要在server/WebApi.cpp中使用api_regist函数注册
    // 这里只是输出日志提示
    InfoL << "转码HTTP API模块已加载";
    InfoL << "请在server/WebApi.cpp中添加以下API注册代码:";
    InfoL << "  api_regist(\"/index/api/transcode/start\", ...) - 启动转码";
    InfoL << "  api_regist(\"/index/api/transcode/stop\", ...) - 停止转码";
    InfoL << "  api_regist(\"/index/api/transcode/list\", ...) - 获取任务列表";
    InfoL << "  api_regist(\"/index/api/transcode/info\", ...) - 获取任务详情";
    InfoL << "  api_regist(\"/index/api/transcode/codecs\", ...) - 获取支持的编解码器";
}

string TranscodeHttpAPI::generateTaskId() {
    lock_guard<mutex> lck(_mtx);
    return "transcode_" + to_string(++_task_counter) + "_" + to_string(time(nullptr));
}

string TranscodeHttpAPI::makeResponse(int code, const string &msg, const string &data) {
    Json::Value root;
    root["code"] = code;
    root["msg"] = msg;
    if (!data.empty()) {
        Json::Reader reader;
        Json::Value dataJson;
        if (reader.parse(data, dataJson)) {
            root["data"] = dataJson;
        } else {
            root["data"] = data;
        }
    }
    return root.toStyledString();
}

string TranscodeHttpAPI::startTranscode(const HttpSession::KeyValue &params) {
    // 获取参数
    auto get_param = [&params](const string &key, const string &def = "") -> string {
        auto it = params.find(key);
        return it != params.end() ? it->second : def;
    };
    
    string src_url = get_param("src_url");
    if (src_url.empty()) {
        return makeResponse(-1, "缺少参数: src_url");
    }
    
    string video_codec = get_param("video_codec", "h264");
    string audio_codec = get_param("audio_codec", "aac");
    
    // 检查编码器支持
    if (!TranscodeAPI::isCodecSupported(video_codec)) {
        return makeResponse(-2, "不支持的视频编码: " + video_codec);
    }
    if (!TranscodeAPI::isCodecSupported(audio_codec)) {
        return makeResponse(-2, "不支持的音频编码: " + audio_codec);
    }
    
    // 创建转码配置
    TranscodeConfig config;
    config.video_codec = TranscodeAPI::parseCodecId(video_codec);
    config.audio_codec = TranscodeAPI::parseCodecId(audio_codec);
    
    // 可选参数
    string video_width = get_param("video_width");
    string video_height = get_param("video_height");
    string video_fps = get_param("video_fps");
    string video_bitrate = get_param("video_bitrate");
    string audio_sample_rate = get_param("audio_sample_rate");
    string audio_channels = get_param("audio_channels");
    string audio_bitrate = get_param("audio_bitrate");
    
    if (!video_width.empty()) config.video_width = stoi(video_width);
    if (!video_height.empty()) config.video_height = stoi(video_height);
    if (!video_fps.empty()) config.video_fps = stoi(video_fps);
    if (!video_bitrate.empty()) config.video_bitrate = stoi(video_bitrate);
    if (!audio_sample_rate.empty()) config.audio_sample_rate = stoi(audio_sample_rate);
    if (!audio_channels.empty()) config.audio_channels = stoi(audio_channels);
    if (!audio_bitrate.empty()) config.audio_bitrate = stoi(audio_bitrate);
    
    // 创建转码任务
    auto task = make_shared<TranscodeTask>();
    task->task_id = generateTaskId();
    task->src_url = src_url;
    task->video_codec = video_codec;
    task->audio_codec = audio_codec;
    task->start_time = time(nullptr);
    
    // 创建转码API
    task->api = TranscodeAPI::create(config);
    task->api->setSource(src_url);
    
    // 设置错误回调
    weak_ptr<TranscodeTask> weak_task = task;
    task->api->setOnError([weak_task](const string &err) {
        auto strong_task = weak_task.lock();
        if (strong_task) {
            WarnL << "转码任务 " << strong_task->task_id << " 错误: " << err;
        }
    });
    
    // 目标推流
    string dst_url = get_param("dst_url");
    if (!dst_url.empty()) {
        task->dst_url = dst_url;
        task->api->pushTo(dst_url);
    }
    
    // 注册为MediaSource
    string vhost = get_param("vhost", DEFAULT_VHOST);
    string app = get_param("app", "transcode");
    string stream = get_param("stream", task->task_id);
    
    task->vhost = vhost;
    task->app = app;
    task->stream = stream;
    task->api->regist(vhost, app, stream);
    
    // 启动转码
    if (!task->api->start()) {
        return makeResponse(-3, "启动转码失败");
    }
    
    // 保存任务
    {
        lock_guard<mutex> lck(_mtx);
        _tasks[task->task_id] = task;
    }
    
    // 构造响应
    Json::Value data;
    data["task_id"] = task->task_id;
    data["src_url"] = src_url;
    data["video_codec"] = video_codec;
    data["audio_codec"] = audio_codec;
    data["play_url"]["rtsp"] = "rtsp://127.0.0.1/" + app + "/" + stream;
    data["play_url"]["rtmp"] = "rtmp://127.0.0.1/" + app + "/" + stream;
    data["play_url"]["http_flv"] = "http://127.0.0.1/" + app + "/" + stream + ".flv";
    data["play_url"]["ws_flv"] = "ws://127.0.0.1/" + app + "/" + stream + ".flv";
    data["play_url"]["hls"] = "http://127.0.0.1/" + app + "/" + stream + "/hls.m3u8";
    
    InfoL << "启动转码任务: " << task->task_id << ", " << src_url << " -> " << video_codec << ":" << audio_codec;
    
    return makeResponse(0, "success", data.toStyledString());
}

string TranscodeHttpAPI::stopTranscode(const HttpSession::KeyValue &params) {
    auto it = params.find("task_id");
    if (it == params.end()) {
        return makeResponse(-1, "缺少参数: task_id");
    }
    
    string task_id = it->second;
    
    lock_guard<mutex> lck(_mtx);
    auto task_it = _tasks.find(task_id);
    if (task_it == _tasks.end()) {
        return makeResponse(-2, "任务不存在: " + task_id);
    }
    
    // 停止转码
    task_it->second->api->stop();
    _tasks.erase(task_it);
    
    InfoL << "停止转码任务: " << task_id;
    
    return makeResponse(0, "success");
}

string TranscodeHttpAPI::listTranscode() {
    Json::Value data;
    data["count"] = 0;
    data["tasks"] = Json::arrayValue;
    
    lock_guard<mutex> lck(_mtx);
    data["count"] = (int)_tasks.size();
    
    for (const auto &pair : _tasks) {
        Json::Value task;
        task["task_id"] = pair.second->task_id;
        task["src_url"] = pair.second->src_url;
        task["dst_url"] = pair.second->dst_url;
        task["video_codec"] = pair.second->video_codec;
        task["audio_codec"] = pair.second->audio_codec;
        task["vhost"] = pair.second->vhost;
        task["app"] = pair.second->app;
        task["stream"] = pair.second->stream;
        task["start_time"] = (Json::UInt64)pair.second->start_time;
        task["duration"] = (Json::UInt64)(time(nullptr) - pair.second->start_time);
        data["tasks"].append(task);
    }
    
    return makeResponse(0, "success", data.toStyledString());
}

string TranscodeHttpAPI::getTranscodeInfo(const HttpSession::KeyValue &params) {
    auto it = params.find("task_id");
    if (it == params.end()) {
        return makeResponse(-1, "缺少参数: task_id");
    }
    
    string task_id = it->second;
    
    lock_guard<mutex> lck(_mtx);
    auto task_it = _tasks.find(task_id);
    if (task_it == _tasks.end()) {
        return makeResponse(-2, "任务不存在: " + task_id);
    }
    
    auto &task = task_it->second;
    
    Json::Value data;
    data["task_id"] = task->task_id;
    data["src_url"] = task->src_url;
    data["dst_url"] = task->dst_url;
    data["video_codec"] = task->video_codec;
    data["audio_codec"] = task->audio_codec;
    data["vhost"] = task->vhost;
    data["app"] = task->app;
    data["stream"] = task->stream;
    data["start_time"] = (Json::UInt64)task->start_time;
    data["duration"] = (Json::UInt64)(time(nullptr) - task->start_time);
    
    // 获取输出Track信息
    auto tracks = task->api->getOutputTracks();
    data["tracks"] = Json::arrayValue;
    for (const auto &track : tracks) {
        Json::Value t;
        t["codec"] = getCodecName(track->getCodecId());
        t["type"] = track->getTrackType() == TrackVideo ? "video" : "audio";
        if (track->getTrackType() == TrackVideo) {
            auto video = static_pointer_cast<VideoTrack>(track);
            t["width"] = video->getVideoWidth();
            t["height"] = video->getVideoHeight();
            t["fps"] = video->getVideoFps();
        } else {
            auto audio = static_pointer_cast<AudioTrack>(track);
            t["sample_rate"] = audio->getAudioSampleRate();
            t["channels"] = audio->getAudioChannel();
        }
        data["tracks"].append(t);
    }
    
    return makeResponse(0, "success", data.toStyledString());
}

string TranscodeHttpAPI::getSupportedCodecs() {
    Json::Value data;
    
    // 视频编码器
    data["video_encoders"] = Json::arrayValue;
    for (const auto &codec : TranscodeAPI::getSupportedVideoCodecs()) {
        data["video_encoders"].append(codec);
    }
    
    // 音频编码器
    data["audio_encoders"] = Json::arrayValue;
    for (const auto &codec : TranscodeAPI::getSupportedAudioCodecs()) {
        data["audio_encoders"].append(codec);
    }
    
    // 转码矩阵
    data["transcode_matrix"]["video"] = "H264 <-> H265 <-> VP8 <-> VP9 <-> AV1";
    data["transcode_matrix"]["audio"] = "AAC <-> Opus <-> G711A <-> G711U <-> MP3";
    
    return makeResponse(0, "success", data.toStyledString());
}

void registerTranscodeHttpAPI() {
    TranscodeHttpAPI::Instance().registerApi();
}

} // namespace mediakit

#endif // ENABLE_FFMPEG
