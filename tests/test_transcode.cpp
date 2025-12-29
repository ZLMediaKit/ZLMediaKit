/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

/**
 * 音视频转码测试程序
 * 
 * 支持的编解码器:
 * 视频: H264, H265, VP8, VP9, AV1, SVAC, JPEG
 * 音频: AAC, Opus, G711A, G711U, G722, G722.1, MP3
 * 
 * 用法示例:
 * 1. H265转H264: ./test_transcode rtsp://xxx h265 h264
 * 2. AAC转Opus:  ./test_transcode rtsp://xxx aac opus
 * 3. 完整转码:   ./test_transcode rtsp://xxx h265:aac h264:opus
 */

#include <signal.h>
#include <iostream>
#include "Util/logger.h"
#include "Util/util.h"
#include "Common/config.h"
#include "Poller/EventPoller.h"

#if defined(ENABLE_FFMPEG)
#include "Codec/TranscodeAPI.h"
#include "Codec/TranscodeManager.h"
#endif

using namespace std;
using namespace toolkit;
using namespace mediakit;

static semaphore g_sem;

static void signal_handler(int sig) {
    InfoL << "收到信号: " << sig;
    g_sem.post();
}

void printUsage(const char *exe) {
    cout << "用法: " << exe << " <源URL> [源编码] [目标编码]" << endl;
    cout << endl;
    cout << "参数说明:" << endl;
    cout << "  源URL      - 输入流地址 (rtsp/rtmp/http-flv等)" << endl;
    cout << "  源编码     - 可选，源流编码格式" << endl;
    cout << "  目标编码   - 可选，目标编码格式" << endl;
    cout << endl;
    cout << "支持的编码格式:" << endl;
    cout << "  视频: h264, h265, vp8, vp9, av1, jpeg" << endl;
    cout << "  音频: aac, opus, g711a, g711u, mp3" << endl;
    cout << endl;
    cout << "示例:" << endl;
    cout << "  " << exe << " rtsp://192.168.1.100/stream" << endl;
    cout << "  " << exe << " rtsp://192.168.1.100/stream h265 h264" << endl;
    cout << "  " << exe << " rtmp://192.168.1.100/live/stream h265:aac h264:opus" << endl;
}

int main(int argc, char *argv[]) {
    // 设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    
    // 加载配置
    loadIniConfig();
    
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
#if !defined(ENABLE_FFMPEG)
    ErrorL << "请在编译时启用ENABLE_FFMPEG选项!";
    return -1;
#else
    
    // 打印支持信息
    TranscodeAPI::printSupportInfo();
    
    if (argc < 2) {
        printUsage(argv[0]);
        return 0;
    }
    
    string src_url = argv[1];
    string video_codec = "h264";
    string audio_codec = "aac";
    
    // 解析编码参数
    if (argc >= 4) {
        string src_codec = argv[2];
        string dst_codec = argv[3];
        
        // 检查是否包含音视频组合格式 (如 "h265:aac")
        auto parse_codec = [](const string &codec, string &video, string &audio) {
            auto pos = codec.find(':');
            if (pos != string::npos) {
                video = codec.substr(0, pos);
                audio = codec.substr(pos + 1);
            } else {
                // 根据编码类型自动判断是音频还是视频
                static set<string> video_codecs = {"h264", "h265", "hevc", "avc", "vp8", "vp9", "av1", "jpeg"};
                if (video_codecs.count(codec)) {
                    video = codec;
                } else {
                    audio = codec;
                }
            }
        };
        
        parse_codec(dst_codec, video_codec, audio_codec);
    }
    
    InfoL << "========================================";
    InfoL << "源地址: " << src_url;
    InfoL << "目标视频编码: " << video_codec;
    InfoL << "目标音频编码: " << audio_codec;
    InfoL << "========================================";
    
    // 检查编码器是否支持
    if (!TranscodeAPI::isCodecSupported(video_codec)) {
        WarnL << "不支持的视频编码: " << video_codec;
    }
    if (!TranscodeAPI::isCodecSupported(audio_codec)) {
        WarnL << "不支持的音频编码: " << audio_codec;
    }
    
    // 创建转码配置
    TranscodeConfig config;
    config.video_codec = TranscodeAPI::parseCodecId(video_codec);
    config.audio_codec = TranscodeAPI::parseCodecId(audio_codec);
    config.async = true;
    config.thread_num = 2;
    
    // 创建转码API
    auto transcode_api = TranscodeAPI::create(config);
    
    // 设置回调
    transcode_api->setOnFrame([](const Frame::Ptr &frame) {
        if (frame->getTrackType() == TrackVideo) {
            TraceL << "视频帧: codec=" << getCodecName(frame->getCodecId())
                   << ", size=" << frame->size()
                   << ", pts=" << frame->pts()
                   << ", key=" << frame->keyFrame();
        } else {
            TraceL << "音频帧: codec=" << getCodecName(frame->getCodecId())
                   << ", size=" << frame->size()
                   << ", pts=" << frame->pts();
        }
    });
    
    transcode_api->setOnTrack([](const Track::Ptr &track) {
        InfoL << "输出Track就绪: " << getCodecName(track->getCodecId())
              << ", type=" << (track->getTrackType() == TrackVideo ? "video" : "audio");
    });
    
    transcode_api->setOnError([](const string &err) {
        WarnL << "转码错误: " << err;
    });
    
    // 设置源
    transcode_api->setSource(src_url);
    
    // 注册为MediaSource，方便其他播放器拉取
    string vhost = DEFAULT_VHOST;
    string app = "transcode";
    string stream = "output";
    transcode_api->regist(vhost, app, stream);
    
    InfoL << "转码输出流: rtsp://127.0.0.1/" << app << "/" << stream;
    InfoL << "转码输出流: rtmp://127.0.0.1/" << app << "/" << stream;
    
    // 开始转码
    if (!transcode_api->start()) {
        ErrorL << "启动转码失败";
        return -1;
    }
    
    InfoL << "转码已启动，按Ctrl+C停止...";
    
    // 等待信号
    g_sem.wait();
    
    // 停止转码
    transcode_api->stop();
    
    InfoL << "转码已停止";
    
#endif // ENABLE_FFMPEG
    
    return 0;
}
