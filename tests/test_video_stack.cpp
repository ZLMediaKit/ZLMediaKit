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

    _params = std::vector<VideoStack::Param>(rows * cols);

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            std::string url = json["urls"][row][col].asString();

            VideoStack::Param param;
            param.posX = gridWidth * col + col * gaphPix;
            param.posY = gridHeight * row + row * gapvPix;

            param.width = gridWidth;
            param.height = gridHeight;

            param.url = url;
            _params[row * cols + col] = param;
        }
    }

    // 判断是否需要合并格子 （焦点屏）
    if (!json["span"].empty()) {
        for (const auto& subArray : json["span"]) {
            std::array<int, 4> mergePos;
            int index = 0;

            // 获取要合并的起始格子和终止格子下标
            for (const auto& innerArray : subArray) {
                for (const auto& number : innerArray) {
                    if (index < mergePos.size()) {
                        mergePos[index++] = number.asInt();
                    }
                }
            }

            for (int i = mergePos[0]; i <= mergePos[2]; i++) {
                for (int j = mergePos[1]; j <= mergePos[3]; j++) {
                    if (i == mergePos[0] && j == mergePos[1]) {
                        // 重新计算合并后格子的宽高
                        _params[i * cols + j].width = (mergePos[3] - mergePos[1] + 1) * gridWidth + (mergePos[3] - mergePos[1]) * gapvPix;
                        _params[i * cols + j].height = (mergePos[2] - mergePos[0] + 1) * gridHeight + (mergePos[2] - mergePos[0]) * gaphPix;
                    }
                    else {
                        _params[i * cols + j] = {}; // 置空被合并的格子
                    }
                }
            }
        }
    }

}

void VideoStack::copyToBuf(const std::shared_ptr<AVFrame> &buf, const FFmpegFrame::Ptr &frame, const Param &p) {

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

void StackPlayer::play(const std::string &url) {
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
             auto strongSP = weakSP.lock();
                if (!strongSP) {
                    return;
                }

            strongSP->fps = videoTrack->getVideoFps();

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

    player->setOnShutdown([](const toolkit::SockException &ex) { 
        InfoL << "Stack play shutdown: " << ex.what(); 
        //TODO:断线 将Param中的isDisconnected置为true,然后编码线程那边对此进行判断，填充断线图片
        });

    (*player)[mediakit::Client::kWaitTrackReady] = false; // 不等待TrackReady
    (*player)[mediakit::Client::kRtpType] = Rtsp::RTP_TCP;

    player->play(url);

    _player = player;
}

void StackPlayer::addStackPtr(VideoStack* that) {
    //std::unique_lock<std::shared_timed_mutex> wlock(_mx);
    if (!that) {
        return;
    }
    auto it = _stacks.find(that->_stack_id);
    if (it != _stacks.end()) {
        return;
    }
    _stacks[that->_stack_id] = that;
}

void StackPlayer::delStackPtr(VideoStack *that) {
    //std::unique_lock<std::shared_timed_mutex> wlock(_mx);
    // TODO:
}


//TODO: 根据相对pts来进行同步 (单位是ms，可能得加一个pts转换时间基的步骤)
/* void StackPlayer::syncFrameByPts(const FFmpegFrame::Ptr& frame, VideoStack::Param& p, float target_fps) {
    static std::shared_ptr<FFmpegFrame> lastFrame = nullptr;
    static int64_t lastPts = 0; // 上一帧的 PTS
    static double totalDiff = 0.0;

    // 检查 frame 是否有效
    if (!frame) return;

    // 首帧时给lastFrame赋值
    if (!lastFrame) {
        lastFrame = frame;
        lastPts = frame->get()->pts;
        p.write.push_back(frame);
        p.tail++;
        return;
    }

    // 计算两帧之间的时间差（假设 PTS 是以秒为单位）
    double diff = static_cast<double>(frame->get()->pts - lastPts);
    double duration = 1000 / target_fps; 

    totalDiff += diff - duration;

    if (totalDiff >= duration) {
        totalDiff -= duration;
        // 当累积误差达到一个完整的帧时，复用上一帧
        p.write.push_back(lastFrame);
        p.tail++;
    }
    else if (totalDiff <= -duration) {
        totalDiff += duration;
        // 累积误差小于负的目标帧持续时间时，跳过当前帧（丢弃）
        // 这里不更新 lastFrame 和 lastPts
    }
    else {
        // 保留当前帧
        p.write.push_back(frame);
        p.tail++;
        lastFrame = frame;
        lastPts = frame->get()->pts;
    }
} */

//直接用fps来计算 进行补帧（复用上一帧）或丢帧
void StackPlayer::syncFrameByFps(const FFmpegFrame::Ptr& frame, VideoStack::Param& p, float target_fps) {

        // 检查 frame 是否有效
        if (!frame) return;

        // 首帧时给lastFrame赋值
        if (!lastFrame) {
            lastFrame = frame;
        }

        diff += fps - target_fps;

        if (diff >= fps) {
            diff -= fps;
            // 当累积误差达到一个完整的帧时，复用上一帧
            p.cache.push_back(lastFrame);
        }
        else if (diff <= -fps) {
            // 累积误差小于负的fps时丢弃当前帧
            diff += fps;
            // 注意这里不更新 lastFrame，因为我们丢弃了当前帧
        }
        else {
            // 保留当前帧
            lastFrame = frame;
            p.cache.push_back(frame);
        }
    }


void StackPlayer::onFrame(const FFmpegFrame::Ptr &frame) {
    //std::shared_lock<std::shared_timed_mutex> rlock(_mx);
    for (auto &vsp : _stacks) {
        auto &that = vsp.second;
        if (!that) {
            continue;
        }

        for (auto &p : that->_params) {
            
            if (p.url != _url) {
                continue;
            }

            //p.cache.push_back(frame);
            syncFrameByFps(frame,p,that->_fps);  //不同帧率的视频，通过复用上一帧或丢帧来实现帧同步

            if (that->isReady.test(p.order)) {
                continue;
            }
            if (p.cache.size() >= MAX_FRAME_SIZE) {
                for (int i = 0; i < MAX_FRAME_SIZE; i++) {
                    auto &front = p.cache.front();
                    that->copyToBuf(that->_buffers[i], front, p);
                    p.cache.pop_front();
                    that->isReady.set(p.order);
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

    for (int i = 0; i < MAX_FRAME_SIZE; i++) {

        std::shared_ptr<AVFrame> frame(av_frame_alloc(), [](AVFrame *frame_) { av_frame_free(&frame_); });

        frame->width = _width;
        frame->height = _height;
        frame->format = _pixfmt;

        av_frame_get_buffer(frame.get(), 32);
        _buffers.push_back(frame);
    }

    // setBackground(0, 0, 0);

    _isExit = false;

    int i = 0;
    for (auto &p : _params) {
        if (p.url.empty()) {
            continue;
        }
        /*p.tmp.reset(av_frame_alloc(), [](AVFrame *frame_) { av_frame_free(&frame_); });

        p.tmp->width = p.width;
        p.tmp->height = p.height;
        p.tmp->format = _pixfmt;

        av_frame_get_buffer(p.tmp.get(), 32);*/
        p.order = i++;

        flag.set(p.order);

        auto it = playerMap.find(p.url);
        if (it == playerMap.end()) {
            // 创建一个

            auto player = std::make_shared<StackPlayer>();
            player->play(p.url);
            player->addStackPtr(this);

            playerMap[p.url] = player;
        } else {
            it->second->addStackPtr(this);
        }
    }


}

void VideoStack::start() {
    std::thread(
        [this]() {
            int64_t pts = 0, index = 0;
            while (!_isExit) {
                if (isReady == flag) {
                    for (auto &buf : _buffers) {
                        _dev->inputYUV((char **)buf->data, buf->linesize, pts);
                        pts += 40;
                        index++;
                    }
                    isReady = 0;
                } else {
                    std::this_thread::sleep_for(std::chrono::milliseconds(5));
                }
            }
        }).detach();
}

static std::unordered_map<uint16_t, toolkit::TcpServer::Ptr> _srvMap;

// 播放地址 http://127.0.0.1:7089/stack/89.live.flv
int main(int argc, char *argv[]) {

    EventPollerPool::enableCpuAffinity(false);   //是否开启cpu亲和性
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
