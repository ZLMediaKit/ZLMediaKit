/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "test_video_stack.h"

#include "Network/TcpServer.h"
#include "Network/TcpServer.h"
#include <memory>
#include <stdexcept>
#include "Common/Device.h"
#include "Util/MD5.h"
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/onceToken.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"

#include "Common/config.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Shell/ShellSession.h"
#include "Rtmp/FlvMuxer.h"
#include "Player/PlayerProxy.h"
#include "Http/WebSocketSession.h"

#include "Pusher/MediaPusher.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;



void VideoStack::parseParam(const std::string &param) {
    //auto json = nlohmann::json::parse(testJson);
    Json::Value json;
    Json::Reader reader;
    reader.parse(testJson, json);

    _width = json["width"].asInt();     //输出宽度
    _height = json["height"].asInt();   //输出高度
    _stack_id = json["id"].asString();  

    int rows = json["rows"].asInt(); // 堆叠行数
    int cols = json["cols"].asInt(); // 堆叠列数
    float gapv = json["gapv"].asFloat(); // 垂直间距
    float gaph = json["gaph"].asFloat(); // 水平间距

    // int gapvPix = (int)std::round(gapv * _width) % 2 ? std::round(gapv * _width)+ 1 : std::round(gapv * _width);
    // int gaphPix = (int)std::round(gaph * _height) % 2 ? std::round(gaph * _height) + 1 : std::round(gaph * _height);
    // int gridWidth = _width - ((cols-1) * gapvPix);         //1920*(1-0.002*3) / 4 = 477
    // int gridHeight = _height - ((rows - 1) * gaphPix);       //1080*(1-0.001*3) / 4 = 269

    //间隔先默认都是0
    auto gridWidth = _width / cols;
    auto gridHeight = _height / rows;
    int gapvPix = 0;
    int gaphPix = 0;

    _params = std::vector<std::vector<VideoStack::Param>>(rows, std::vector<VideoStack::Param>(cols));

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            std::string videoID = json["urls"][row][col].asString();

            VideoStack::Param param;
            param.posX = gridWidth * col + col * gaphPix;
            param.posY = gridHeight * row + row * gapvPix;

            param.width = gridWidth;
            param.height = gridHeight;

            param.stream_id = videoID;
            _params[row][col] = param;
        }
    }

    // 判断是否需要合并格子 （焦点屏）
    if (!json["span"].empty()) {

        for (const auto &subArray : json["span"]) {
            std::array<int, 4> mergePos;
            int index = 0;

            // 获取要合并的起始格子和终止格子下标
            for (const auto &innerArray : subArray) {
                for (const auto &number : innerArray) {
                    if (index < mergePos.size()) {
                        mergePos[index++] = number.asInt();
                    }
                }
            }

            for (int i = mergePos[0]; i <= mergePos[2]; i++) {
                for (int j = mergePos[1]; j <= mergePos[3]; j++) {
                    if (i == mergePos[0] && j == mergePos[1]) // 重新计算合并后格子的宽高
                    {
                        _params[i][j].width = (mergePos[3] - mergePos[1] + 1) * gridWidth + (mergePos[3] - mergePos[1]) * gapvPix;
                        _params[i][j].height = (mergePos[2] - mergePos[0] + 1) * gridHeight + (mergePos[2] - mergePos[0]) * gaphPix;
                    } else {
                        _params[i][j] = {}; // 置空被合并的格子
                    }
                }
            }
        }
    }
}

void VideoStack::copyToBuf(const FFmpegFrame::Ptr &frame, const Param &p) {

    auto &buf = _buffer;

    auto sws = std::make_shared<FFmpegSws>(AV_PIX_FMT_YUV420P, p.width, p.height);

    auto tmp = sws->inputFrame(frame);

    /*libyuv::I420Scale(frame->get()->data[0], frame->get()->linesize[0],
        frame->get()->data[1], frame->get()->linesize[1],
        frame->get()->data[2], frame->get()->linesize[2],
        frame->get()->width, frame->get()->height,
        tmp->data[0], tmp->linesize[0],
        tmp->data[1], tmp->linesize[1],
        tmp->data[2], tmp->linesize[2],
        tmp->width, tmp->height,
        libyuv::kFilterNone);*/

    //TODO: NV12的copy

    //Y平面
    for (int i = 0; i < p.height; i++) {
        memcpy(buf->data[0] + buf->linesize[0] * (i + p.posY) + p.posX, tmp->get()->data[0] + tmp->get()->linesize[0] * i, tmp->get()->width);
    }
    for (int i = 0; i < p.height / 2; i++) {
        // U平面
        memcpy(buf->data[1] + buf->linesize[1] * (i + p.posY / 2) + p.posX / 2, tmp->get()->data[1] + tmp->get()->linesize[1] * i, tmp->get()->width / 2);

        // V平面
        memcpy(buf->data[2] + buf->linesize[2] * (i + p.posY / 2) + p.posX / 2, tmp->get()->data[2] + tmp->get()->linesize[2] * i, tmp->get()->width / 2);
    }

}

void StackPlayer::init(const std::string &url) {
    _url = url;
    // 创建拉流 解码对象
    auto player = std::make_shared<mediakit::MediaPlayer>();
    std::weak_ptr<mediakit::MediaPlayer> weakPlayer = player;

    std::weak_ptr<StackPlayer> weakSP = shared_from_this();

    player->setOnPlayResult([weakPlayer, weakSP, url](const toolkit::SockException &ex) mutable {
        InfoL << "Dec channel OnPlayResult:" << ex.what();
        auto strongPlayer = weakPlayer.lock();
        if (!strongPlayer) {
            return;
        }

        if (ex) {
            InfoL << "重试： " << url;
            std::this_thread::sleep_for(std::chrono::seconds(5));
            strongPlayer->play(url);
        }

        auto videoTrack = std::dynamic_pointer_cast<mediakit::VideoTrack>(strongPlayer->getTrack(mediakit::TrackVideo, false));
        // auto audioTrack = std::dynamic_pointer_cast<mediakit::AudioTrack>(strongPlayer->getTrack(mediakit::TrackAudio, false));

        if (videoTrack) {
            // auto decoder = std::make_shared<FFmpegDecoder>(videoTrack, 1, std::vector<std::string>{ "hevc_cuvid", "h264_cuvid"});
            auto decoder = std::make_shared<mediakit::FFmpegDecoder>(videoTrack, 0, std::vector<std::string> { "h264" });
            // auto decoder = std::make_shared<mediakit::FFmpegDecoder>(videoTrack);

            decoder->setOnDecode([weakSP](const mediakit::FFmpegFrame::Ptr &frame) mutable {
                
                auto strongSP = weakSP.lock();
                if (!strongSP) {
                    return;
                }
                strongSP->onFrame(frame);
            });

            videoTrack->addDelegate((std::function<bool(const mediakit::Frame::Ptr &)>)[decoder](const mediakit::Frame::Ptr &frame) {
                return decoder->inputFrame(frame, false, false);
            });
        }
    });

    player->setOnShutdown([](const toolkit::SockException &ex) { InfoL << "Stack play shutdown: " << ex.what(); });

    (*player)[mediakit::Client::kWaitTrackReady] = false; // 不等待TrackReady
    (*player)[mediakit::Client::kRtpType] = Rtsp::RTP_TCP;

    player->play(url);

    _player = player;
}

void StackPlayer::addStackPtr(VideoStack* that) {
    //std::unique_lock<std::shared_timed_mutex> wlock(_mx);
    _stacks.push_back(that);
}

void StackPlayer::delStackPtr(VideoStack *that) {
    //std::unique_lock<std::shared_timed_mutex> wlock(_mx);
    // TODO:
}


void StackPlayer::onFrame(const FFmpegFrame::Ptr &frame) {
    //std::shared_lock<std::shared_timed_mutex> rlock(_mx);
    for (auto &that : _stacks) {

        if (!that) {
            continue;
        }

        for (auto &v : that->_params) {
            for (auto &p : v) {
                if (p.stream_id != _url) {
                    continue;
                }

                // TODO: 待实现帧缓存和帧同步
                if (std::chrono::steady_clock::now() - p.lastInputTime > std::chrono::milliseconds(20)) {

                    that->copyToBuf(frame, p);

                    p.lastInputTime = std::chrono::steady_clock::now();
                }
            }
        }
    }
}

void VideoStack::init() {
    _dev = std::make_shared<mediakit::DevChannel>(mediakit::MediaTuple{ DEFAULT_VHOST, "stack", _stack_id });

    mediakit::VideoInfo info;
    info.codecId = mediakit::CodecH264;
    info.iWidth = _width;
    info.iHeight = _height;
    info.iFrameRate = _fps;
    info.iBitRate = _bitRate;

    _dev->initVideo(std::move(info));
    // dev->initAudio();         //TODO:音频
    _dev->addTrackCompleted();

    _buffer.reset(av_frame_alloc(), [](AVFrame *frame_) { av_frame_free(&frame_); });

    _buffer->width = _width;
    _buffer->height = _height;
    _buffer->format = _pixfmt;

    av_frame_get_buffer(_buffer.get(), 32);

    // setBackground(0, 0, 0);

    _isExit = false;

    for (auto &v : _params) {
        for (auto &p : v) {
            if (p.stream_id.empty()) {
                continue;
            }

            /*p.tmp.reset(av_frame_alloc(), [](AVFrame *frame_) { av_frame_free(&frame_); });

            p.tmp->width = p.width;
            p.tmp->height = p.height;
            p.tmp->format = _pixfmt;

            av_frame_get_buffer(p.tmp.get(), 32);*/

            auto it = playerMap.find(p.stream_id);
            if (it == playerMap.end()) {
                // 创建一个

                auto player = std::make_shared<StackPlayer>();
                player->init(p.stream_id);
                player->addStackPtr(this);

                playerMap[p.stream_id] = player;
            } else {
                it->second->addStackPtr(this);
            }
        }
    }

}

void VideoStack::start() {
    using namespace std::chrono;
    std::thread(
        [this]() {
            int64_t pts = 0, index = 0;
            auto interval = milliseconds(40); // 设置间隔时间为40毫秒
            while (!_isExit) {
                auto start = high_resolution_clock::now();

                _dev->inputYUV((char **)_buffer->data, _buffer->linesize, pts);
                pts += 40;
                index++;

                auto end = high_resolution_clock::now();
                auto duration = duration_cast<milliseconds>(end - start);
                if (duration < interval) {
                    std::this_thread::sleep_for(interval - duration); // 如果迭代花费时间小于间隔时间，等待剩余时间
                }
            }
        }).detach();
}

static std::unordered_map<uint16_t, toolkit::TcpServer::Ptr> _srvMap;

// 播放地址 http://127.0.0.1:7089/stack/89.live.flv
int main(int argc, char *argv[]) {

    Logger::Instance().add(std::make_shared<ConsoleChannel>());

    TcpServer::Ptr httpSrv(new TcpServer());
    httpSrv->start<HttpSession>(7089);
    _srvMap.emplace(7089, httpSrv);


    VideoStack v;
    v.parseParam();
    v.init();
    v.start();

    getchar();
    return 0;
}
