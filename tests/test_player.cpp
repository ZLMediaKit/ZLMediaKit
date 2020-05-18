/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <signal.h>
#include "Util/util.h"
#include "Util/logger.h"
#include <iostream>
#include "Rtsp/UDPServer.h"
#include "Player/MediaPlayer.h"
#include "Util/onceToken.h"
#include "FFMpegDecoder.h"
#include "YuvDisplayer.h"
#include "Extension/H265.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;


/**
 * 合并一些时间戳相同的frame
 */
class FrameMerger {
public:
    FrameMerger() = default;
    virtual ~FrameMerger() = default;

    void inputFrame(const Frame::Ptr &frame,const function<void(uint32_t dts,uint32_t pts,const Buffer::Ptr &buffer)> &cb){
        if (!_frameCached.empty() && _frameCached.back()->dts() != frame->dts()) {
            Frame::Ptr back = _frameCached.back();
            Buffer::Ptr merged_frame = back;
            if(_frameCached.size() != 1){
                string merged;
                _frameCached.for_each([&](const Frame::Ptr &frame){
                    if(frame->prefixSize()){
                        merged.append(frame->data(),frame->size());
                    } else{
                        merged.append("\x00\x00\x00\x01",4);
                        merged.append(frame->data(),frame->size());
                    }
                });
                merged_frame = std::make_shared<BufferString>(std::move(merged));
            }
            cb(back->dts(),back->pts(),merged_frame);
            _frameCached.clear();
        }
        _frameCached.emplace_back(Frame::getCacheAbleFrame(frame));
    }
private:
    List<Frame::Ptr> _frameCached;
};


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

    MediaPlayer::Ptr player(new MediaPlayer());
    weak_ptr<MediaPlayer> weakPlayer = player;
    player->setOnPlayResult([weakPlayer](const SockException &ex) {
        InfoL << "OnPlayResult:" << ex.what();
        auto strongPlayer = weakPlayer.lock();
        if (ex || !strongPlayer) {
            return;
        }

        auto viedoTrack = strongPlayer->getTrack(TrackVideo);
        if (!viedoTrack) {
            WarnL << "没有视频!";
            return;
        }

        AnyStorage::Ptr storage(new AnyStorage);
        viedoTrack->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([storage](const Frame::Ptr &frame_in) {
            auto frame = Frame::getCacheAbleFrame(frame_in);
            SDLDisplayerHelper::Instance().doTask([frame,storage]() {
                auto &decoder = (*storage)["decoder"];
                auto &displayer = (*storage)["displayer"];
                auto &merger = (*storage)["merger"];
                if(!decoder){
                    decoder.set<FFMpegDecoder>(frame->getCodecId());
                }
                if(!displayer){
                    displayer.set<YuvDisplayer>(nullptr,url);
                }
                if(!merger){
                    merger.set<FrameMerger>();
                }
                merger.get<FrameMerger>().inputFrame(frame,[&](uint32_t dts,uint32_t pts,const Buffer::Ptr &buffer){
                    AVFrame *pFrame = nullptr;
                    bool flag = decoder.get<FFMpegDecoder>().inputVideo((unsigned char *) buffer->data(), buffer->size(), dts, &pFrame);
                    if (flag) {
                        displayer.get<YuvDisplayer>().displayYUV(pFrame);
                    }
                });
                return true;
            });
        }));
    });


    player->setOnShutdown([](const SockException &ex) {
        ErrorL << "OnShutdown:" << ex.what();
    });
    (*player)[kRtpType] = atoi(argv[2]);
    player->play(argv[1]);

    SDLDisplayerHelper::Instance().runLoop();
    return 0;
}

