/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include "Util/logger.h"
#include <iostream>
#include "Rtsp/UDPServer.h"
#include "Player/MediaPlayer.h"
#include "Util/onceToken.h"
#include "FFMpegDecoder.h"
#include "YuvDisplayer.h"
#include "AudioSRC.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

#ifdef WIN32
#include <TCHAR.h>

extern int __argc;
extern TCHAR** __targv;

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstanc, LPSTR lpCmdLine, int nShowCmd) {
    int argc = __argc;
    char **argv = __targv;

    //1. 首先调用AllocConsole创建一个控制台窗口
    AllocConsole();

    //2. 但此时调用cout或者printf都不能正常输出文字到窗口（包括输入流cin和scanf）, 所以需要如下重定向输入输出流：
    FILE* stream;
    freopen_s(&stream, "CON", "r", stdin);//重定向输入流
    freopen_s(&stream, "CON", "w", stdout);//重定向输入流

    //3. 如果我们需要用到控制台窗口句柄，可以调用FindWindow取得：
    HWND _consoleHwnd;
    SetConsoleTitleA("test_player");//设置窗口名
#else
#include <unistd.h>
int main(int argc, char *argv[]) {
#endif
    static char *url = argv[1];
    //设置退出信号处理函数
    signal(SIGINT, [](int) { SDLDisplayerHelper::Instance().shutdown(); });
    //设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    if (argc != 3) {
        ErrorL << "\r\n测试方法：./test_player rtxp_url rtp_type\r\n"
               << "例如：./test_player rtsp://admin:123456@127.0.0.1/live/0 0\r\n"
               << endl;
        return 0;
    }

    auto player = std::make_shared<MediaPlayer>();
    //sdl要求在main线程初始化
    auto displayer = std::make_shared<YuvDisplayer>(nullptr, url);
    weak_ptr<MediaPlayer> weakPlayer = player;
    player->setOnPlayResult([weakPlayer, displayer](const SockException &ex) {
        InfoL << "OnPlayResult:" << ex.what();
        auto strongPlayer = weakPlayer.lock();
        if (ex || !strongPlayer) {
            return;
        }

        auto videoTrack = dynamic_pointer_cast<VideoTrack>(strongPlayer->getTrack(TrackVideo, false));
        auto audioTrack = dynamic_pointer_cast<AudioTrack>(strongPlayer->getTrack(TrackAudio,false));

        if (videoTrack) {
            auto decoder = std::make_shared<FFmpegDecoder>(videoTrack);
            decoder->setOnDecode([displayer](const FFmpegFrame::Ptr &yuv) {
                SDLDisplayerHelper::Instance().doTask([yuv, displayer]() {
                    //sdl要求在main线程渲染
                    displayer->displayYUV(yuv->get());
                    return true;
                });
            });
            auto merger = std::make_shared<FrameMerger>(FrameMerger::h264_prefix);
            auto delegate = std::make_shared<FrameWriterInterfaceHelper>([decoder, merger](const Frame::Ptr &frame) {
                merger->inputFrame(frame, [&](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer, bool have_idr) {
                    decoder->inputFrame(buffer->data(), buffer->size(), dts, pts);
                });
            });
            videoTrack->addDelegate(delegate);
        }

        if (audioTrack) {
            auto decoder = std::make_shared<FFmpegDecoder>(audioTrack);
            auto audio_player = std::make_shared<AudioPlayer>();
            //FFmpeg解码时已经统一转换为16位整型pcm
            audio_player->setup(audioTrack->getAudioSampleRate(), audioTrack->getAudioChannel(), AUDIO_S16);
            decoder->setOnDecode([audio_player](const FFmpegFrame::Ptr &pcm) {
                audio_player->playPCM((const char *) (pcm->get()->data[0]), pcm->get()->linesize[0]);
            });
            auto audio_delegate = std::make_shared<FrameWriterInterfaceHelper>( [decoder](const Frame::Ptr &frame) {
                decoder->inputFrame(frame);
            });
            audioTrack->addDelegate(audio_delegate);
        }
    });

    (*player)[kRtpType] = atoi(argv[2]);
    player->play(argv[1]);
    SDLDisplayerHelper::Instance().runLoop();
    return 0;
}

