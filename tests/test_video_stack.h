#pragma once

#include "Common/config.h"
#include "Player/PlayerProxy.h"
#include "Rtsp/UDPServer.h"
#include "Thread/WorkThreadPool.h"
#include "Util/CMD.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include <atomic>
#include <iostream>
#include <signal.h>
#include "Codec/Transcode.h"
#include "Common/Device.h"
#include "api/include/mk_transcode.h"
#include "json/json.h"
#include <bitset>
#include <shared_mutex>

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



static std::string testJson = R"({"msg":"set_combine_source","gapv":0.002,"gaph":0.001,"width":1920,"urls":[["rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test"],["rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test"],["rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test"],["rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test"]],"id":"89","rows":4,"cols":4,"height":1080,"span":[[[0,0],[1,1]],[[2,3],[3,3]]]})";


class VideoStack :public std::enable_shared_from_this<VideoStack>
{
public:
    static constexpr int MAX_CACHE_SIZE = 100;

    using Ptr = std::shared_ptr<VideoStack>;

    //全对象共享，使用时拷贝一份，叠加上osd信息。
    static mediakit::FFmpegFrame::Ptr noVideoPic;       //没有url时，显示的图片
    static mediakit::FFmpegFrame::Ptr disConnPic;       //有url，但该流断线时显示的图片

    struct Param
    {
        int posX = 0;
        int posY = 0;
        int width = 0;
        int height = 0;
        std::string url{};

        //运行时参数
        std::shared_ptr<AVFrame> tmp;	//用于存储缩放的临时空间

        uint64_t head = 0;
        uint64_t tail = 0;
        std::vector<mediakit::FFmpegFrame::Ptr> write = std::vector<mediakit::FFmpegFrame::Ptr>(MAX_CACHE_SIZE);
        std::vector<mediakit::FFmpegFrame::Ptr> read = std::vector<mediakit::FFmpegFrame::Ptr>(MAX_CACHE_SIZE);

        int count = 0;

        bool isConnected = false;

    };



    /*-------给StackPlayer用到的回调-------*/
    void onPlaySucess(Param& p)
    {
        p.tail = getMinTail(false);
        p.head = p.tail;
        p.write.clear();
        p.isConnected = true;
    }

    void onShutdown(Param& p)
    {
        p.isConnected = false;
        p.head = p.tail;
        p.write.clear();
    }

    void onFrame(Param& p, const mediakit::FFmpegFrame::Ptr& frame)
    {
        //TODO:syncFrameByFps  syncFrameByPts

        if (p.tail - p.head > MAX_CACHE_SIZE) {
            p.tail -= MAX_CACHE_SIZE;
        }
        p.write[p.tail % MAX_CACHE_SIZE] = frame;
        p.tail++;


        if (p.head < tail) {
            p.read.clear();

            int start = p.head % MAX_CACHE_SIZE;
            int end = tail % MAX_CACHE_SIZE;

            if (end <= start) {
                // 复制 start 到 MAX_CACHE_SIZE 之间的元素
                std::copy(p.write.begin() + start, p.write.begin() + MAX_CACHE_SIZE, std::back_inserter(p.read));
                // 复制 0 到 end 之间的元素
                std::copy(p.write.begin(), p.write.begin() + end, std::back_inserter(p.read));
            }
            else {
                // 复制 start 到 end 之间的元素
                std::copy(p.write.begin() + start, p.write.begin() + end, std::back_inserter(p.read));
            }

            p.head += (tail - p.head);

            std::unique_lock<std::mutex> lock(_mx);
            readyCount++;
            cv.notify_one();
        }
    }


public:

    VideoStack(const std::string& id, std::vector<Param>& param, int width = 1920, int height = 1080,
        AVPixelFormat pixfmt = AV_PIX_FMT_YUV420P, float fps = 25.0, int bitRate = 2 * 1024 * 1024, uint8_t r = 20, uint8_t g = 20, uint8_t b = 20)
        :_id(id), _params(std::move(param)), _width(width), _height(height), _pixfmt(pixfmt), _fps(fps), _bitRate(bitRate)
    {
        _buffer.reset(av_frame_alloc(), [](AVFrame* frame_) {
            av_frame_free(&frame_);
            });

        _buffer->width = _width;
        _buffer->height = _height;
        _buffer->format = _pixfmt;

        av_frame_get_buffer(_buffer.get(), 32);

        int i = 0;
        for (auto& p : _params) {
            if (p.width == 0 || p.height == 0) continue;

            p.tmp.reset(av_frame_alloc(), [](AVFrame* frame_) {
                av_frame_free(&frame_);
                });
            p.tmp->width = p.width;
            p.tmp->height = p.height;
            p.tmp->format = _pixfmt;

            av_frame_get_buffer(p.tmp.get(), 32);

        }


        //TODO:
        //setBackground(r, g, b); 


        _dev = std::make_shared<mediakit::DevChannel>(std::move(mediakit::MediaTuple{ DEFAULT_VHOST, "stack", _id }));

        mediakit::VideoInfo info;
        info.codecId = mediakit::CodecH264;
        info.iWidth = _width;
        info.iHeight = _height;
        info.iFrameRate = _fps;
        info.iBitRate = _bitRate;

        _dev->initVideo(std::move(info));
        //dev->initAudio();         //TODO:音频
        _dev->addTrackCompleted();

        _isExit = false;

    }


    ~VideoStack()
    {
        _isExit = true;
    }


    uint64_t getMinTail(bool isUpdateTotalCount = true)
    {
        uint64_t minTail = std::numeric_limits<uint64_t>::max();
        if (isUpdateTotalCount) {
            totalCount = 0;
        }
        for (const auto& p : _params) {
            
            if (!p.url.empty() && p.isConnected) {
                if (isUpdateTotalCount) {
                    totalCount++;
                }
                
                if (p.tail < minTail) {
                    minTail = p.tail;
                }
            }
        }
        
        return minTail == std::numeric_limits<uint64_t>::max() ? 0 : minTail; // 如果没有找到有效的最小值，返回 0

    }

    void copyToBuf(const mediakit::FFmpegFrame::Ptr& frame, const Param& p)
    {
         auto sws = std::make_shared<mediakit::FFmpegSws>(AV_PIX_FMT_YUV420P, p.width, p.height);

        auto tmp = sws->inputFrame(frame);

        auto& buf = _buffer;
        //auto& tmp = p.tmp;
        auto&& rawFrame = frame->get();

        // libyuv::I420Scale(rawFrame->data[0], rawFrame->linesize[0],
        //     rawFrame->data[1], rawFrame->linesize[1],
        //     rawFrame->data[2], rawFrame->linesize[2],
        //     rawFrame->width, rawFrame->height,
        //     tmp->data[0], tmp->linesize[0],
        //     tmp->data[1], tmp->linesize[1],
        //     tmp->data[2], tmp->linesize[2],
        //     tmp->width, tmp->height,
        //     libyuv::kFilterNone);


        for (int i = 0; i < p.height; i++) {
            memcpy(buf->data[0] + buf->linesize[0] * (i + p.posY) + p.posX,
                tmp->get()->data[0] + tmp->get()->linesize[0] * i,
                tmp->get()->width);
        }
        for (int i = 0; i < p.height / 2; i++) {
            // U平面
            memcpy(buf->data[1] + buf->linesize[1] * (i + p.posY / 2) + p.posX / 2,
                tmp->get()->data[1] + tmp->get()->linesize[1] * i,
                tmp->get()->width / 2);

            // V平面
            memcpy(buf->data[2] + buf->linesize[2] * (i + p.posY / 2) + p.posX / 2,
                tmp->get()->data[2] + tmp->get()->linesize[2] * i,
                tmp->get()->width / 2);
        }
    }


    void play()
    {
        std::weak_ptr<VideoStack> weakSelf = shared_from_this();

        std::thread([weakSelf]() {
            int64_t pts = 0;

            while (true) {
                auto self = weakSelf.lock();
                if (!self)  break;
                if (self->_isExit)  break;

                uint64_t tail = self->getMinTail();
                if (tail == 0 || tail == std::numeric_limits<int64_t>::max()) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }

                auto count = (tail - self->head) % MAX_CACHE_SIZE;
               // LOGINFO() << "tail: " << tail << "  count: " << count << "  head: " << self->head;
                if (count == 0) {
                    std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    continue;
                }


                self->tail = tail;

                {
                    std::unique_lock<std::mutex> lock(self->_mx);
                    if (!self->cv.wait_for(lock, std::chrono::milliseconds(500), [&]() {
                        return self->readyCount >= self->totalCount;
                        })) {
                        //LOGWARN() << "等待超时！";
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                        continue;
                    }
                }
                self->readyCount = 0;       //重置计数器，用于下次循环

                for (int i = 0; i < count; i++) {
                    for (auto& p : self->_params) {
                        if (p.width == 0 || p.height == 0) {
                            continue;
                        }

                        if (p.url.empty()) {
                            //TODO:填充无视频图片
                            //copyToBuf(novideo,p);
                            continue;
                        }

                        mediakit::FFmpegFrame::Ptr frame;

                        if (p.isConnected) {
                            if (p.read.empty()) {
                                continue;
                            }
                            frame = p.read[i];
                        }
                        else {
                            //frame = self->disconnPic;
                            frame = nullptr;    //TODO:填充断线图片
                        }

                        if (!frame) continue;

                        self->copyToBuf(frame, p);

                    }


                    self->_dev->inputYUV((char**)self->_buffer->data, self->_buffer->linesize, pts);
                    pts += 40;
                }

                self->head += count;

            }
           
           // LOGFATAL() << "退出！！！！！！！！！！！！！！！！！！！！！！！";
            }).detach();
    }


protected:
    //void setBackground(uint8_t r, uint8_t g, uint8_t b);      //设置背景色 （间隔的颜色） RGB->YUV/NV12


public:
    std::vector<Param> _params;   //存储参数
    std::string _id;

private:

    int _width;
    int _height;
    AVPixelFormat _pixfmt = AV_PIX_FMT_YUV420P;
    float _fps = 25.0;
    int _bitRate = 2 * 1024 * 1024;

    mediakit::DevChannel::Ptr _dev;

    bool _isExit;
   
    std::shared_ptr<AVFrame> _buffer;

    //mediakit::FFmpegFrame::Ptr DisconnPic;

    std::atomic<int64_t> head {0};
    std::atomic<int64_t> tail {0};

    int totalCount = 0;
    int readyCount = 0;

    std::condition_variable cv;
    std::mutex _mx;
};

static int rframe = 0;
class StackPlayer : public std::enable_shared_from_this<StackPlayer>
{
public:
    using Ptr = std::shared_ptr<StackPlayer>;

    ~StackPlayer() = default;

    void play(const std::string& url)
    {
        _url = url;

        //创建拉流 解码对象
        auto player = std::make_shared<mediakit::MediaPlayer>();
        std::weak_ptr<mediakit::MediaPlayer> weakPlayer = player;

        std::weak_ptr<StackPlayer> weakSelf = shared_from_this();

        player->setOnPlayResult([weakPlayer, weakSelf, url](const toolkit::SockException& ex) mutable {
            //LOGTRACE() << "StackPlayer OnPlayResult:" << ex.what();
            auto strongPlayer = weakPlayer.lock();
            if (!strongPlayer) {
                return;
            }

            if (ex) {
                //LOGERR() << "StackPlayer play failed, retry： " << url;
                strongPlayer->play(url);
            }
            else {
                auto self = weakSelf.lock();
                if (!self) {
                    return;
                }
                self->dispatch(&VideoStack::onPlaySucess);
            }


            auto videoTrack = std::dynamic_pointer_cast<mediakit::VideoTrack>(strongPlayer->getTrack(mediakit::TrackVideo, false));
            //auto audioTrack = std::dynamic_pointer_cast<mediakit::AudioTrack>(strongPlayer->getTrack(mediakit::TrackAudio, false));

            if (videoTrack) {
                //TODO:添加使用显卡还是cpu解码的判断逻辑
                //auto decoder = std::make_shared<FFmpegDecoder>(videoTrack, 1, std::vector<std::string>{ "hevc_cuvid", "h264_cuvid"});
                auto decoder = std::make_shared<mediakit::FFmpegDecoder>(videoTrack, 0, std::vector<std::string>{"h264", "hevc" });
                //auto decoder = std::make_shared<mediakit::FFmpegDecoder>(videoTrack);

                /*auto self = weakSelf.lock();
                if (!self) {
                    return;
                }
                self->fps = videoTrack->getVideoFps();*/

                decoder->setOnDecode([weakSelf](const mediakit::FFmpegFrame::Ptr& frame) mutable {

                    //TODO: 回调函数（copy frame数据到待编码的buf中）
                    //copy到需要的编码线程的该通道的队列中
                    auto self = weakSelf.lock();
                    if (!self) {
                        return;
                    }

                    //LOGINFO() << "收到frame: " << rframe++;
                    //LOGINFO() << "Frame pts: " << frame->get()->pts;
                    self->dispatch(&VideoStack::onFrame, frame);

                    });


                videoTrack->addDelegate((std::function<bool(const mediakit::Frame::Ptr&)>)[decoder](const mediakit::Frame::Ptr& frame) {
                    return decoder->inputFrame(frame, false, true);
                    });

            }

            });

        player->setOnShutdown([weakPlayer, url, weakSelf](const toolkit::SockException& ex) {
            //LOGTRACE() << "StackPlayer Onshutdown: " << ex.what();
            auto strongPlayer = weakPlayer.lock();
            if (!strongPlayer) {
                return;
            }

            if (ex) {

                auto self = weakSelf.lock();
                if (!self) {
                    return;
                }

                self->dispatch(&VideoStack::onShutdown);

                //LOGTRACE() << "StackPlayer try to reconnect: " << url;
                strongPlayer->play(url);
            }

            });

        (*player)[mediakit::Client::kWaitTrackReady] = false;       //不等待TrackReady
        (*player)[mediakit::Client::kRtpType] = mediakit::Rtsp::RTP_TCP;

        player->play(url);

        _player = player;
    }

    void addDispatcher(const std::weak_ptr<VideoStack>& weakPtr)
    {
        auto ptr = weakPtr.lock();
        if (!ptr) {
            return;
        }
        //wlock_(_mx);
        std::lock_guard<std::mutex> lock(_mx);
        auto it = _dispatchMap.find(ptr->_id);
        if (it != _dispatchMap.end()) {
            return;
        }
        _dispatchMap[ptr->_id] = weakPtr;
    }

    void delDispatcher(const std::string& id)
    {
        //wlock_(_mx);
        std::lock_guard<std::mutex> lock(_mx);
        auto it = _dispatchMap.find(id);
        if (it == _dispatchMap.end()) {
            return;
        }
        _dispatchMap.erase(it);

    }


protected:

    template<typename Func, typename... Args>
    void dispatch(Func func, Args... args) {
        //rlock_(_mx);
        std::lock_guard<std::mutex> lock(_mx);
        for (auto& [_, weakPtr] : _dispatchMap) {
            auto strongPtr = weakPtr.lock();
            if (!strongPtr) continue;
            for (auto& p : strongPtr->_params) {
                if (p.url != _url) continue;
                (strongPtr.get()->*func)(p, args...);
            }

        }
    }
private:
    std::string _url;


    mediakit::MediaPlayer::Ptr _player;

    //RWMutex _mx;
    std::mutex _mx;
    std::unordered_map<std::string, std::weak_ptr<VideoStack>> _dispatchMap;
};




class VideoStackManager
{
public:

    //解析参数，解析成功返回true，解析失败返回false， 解析出来的参数通过tmp返回
    bool parseParam(const std::string& jsonStr, std::string& id, std::vector<VideoStack::Param>& params)
    {
        //auto json = nlohmann::json::parse(testJson);
    Json::Value json;
    Json::Reader reader;
    reader.parse(testJson, json);

    int width = json["width"].asInt();     //输出宽度
    int height = json["height"].asInt();   //输出高度
    id = json["id"].asString();  

    int rows = json["rows"].asInt(); // 堆叠行数
    int cols = json["cols"].asInt(); // 堆叠列数
    float gapv = json["gapv"].asFloat(); // 垂直间距
    float gaph = json["gaph"].asFloat(); // 水平间距

    // int gapvPix = (int)std::round(gapv * _width) % 2 ? std::round(gapv * _width)+ 1 : std::round(gapv * _width);
    // int gaphPix = (int)std::round(gaph * _height) % 2 ? std::round(gaph * _height) + 1 : std::round(gaph * _height);
    // int gridWidth = _width - ((cols-1) * gapvPix);         //1920*(1-0.002*3) / 4 = 477
    // int gridHeight = _height - ((rows - 1) * gaphPix);       //1080*(1-0.001*3) / 4 = 269

    //间隔先默认都是0
    auto gridWidth = width / cols;
    auto gridHeight = height / rows;
    int gapvPix = 0;
    int gaphPix = 0;

    params = std::vector<VideoStack::Param>(rows * cols);

    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols; col++) {
            std::string url = json["urls"][row][col].asString();

            VideoStack::Param param;
            param.posX = gridWidth * col + col * gaphPix;
            param.posY = gridHeight * row + row * gapvPix;

            param.width = gridWidth;
            param.height = gridHeight;

            param.url = url;
            params[row * cols + col] = param;
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
                        params[i * cols + j].width = (mergePos[3] - mergePos[1] + 1) * gridWidth + (mergePos[3] - mergePos[1]) * gapvPix;
                        params[i * cols + j].height = (mergePos[2] - mergePos[0] + 1) * gridHeight + (mergePos[2] - mergePos[0]) * gaphPix;
                    }
                    else {
                        params[i * cols + j] = {}; // 置空被合并的格子
                    }
                }
            }
        }
    }

        return true;
    }


    bool start(const std::string& json)
    {
        std::vector<VideoStack::Param> params;
        std::string id;

        bool ret = parseParam(json, id, params);

        if (!ret)  return false;

        auto stack = std::make_shared<VideoStack>(id, params);

        for (auto& p : stack->_params) {
            if (p.url.empty()) continue;
            if (p.width == 0 || p.height == 0) continue;


            StackPlayer::Ptr player;

            {
                std::lock_guard<std::recursive_mutex> lock(_playerMx);
                auto it = _playerMap.find(p.url);
                if (it != _playerMap.end()) {
                    player = it->second;
                }
                else {
                    player = std::make_shared<StackPlayer>();
                    player->play(p.url);

                    _playerMap[p.url] = player;
                }
            }
            _weakPlayer = player;
            player->addDispatcher(std::weak_ptr<VideoStack>(stack));
        }


        stack->play();
        std::lock_guard<std::recursive_mutex> lock(_stackMx);
        _stackMap[id] = stack;

        _weakPtr = std::weak_ptr<VideoStack>(stack);

        return true;
    }


    void stop(const std::string& id)
    {
        //TODO:先临时全部清空
        {
            std::lock_guard<std::recursive_mutex> lock(_playerMx);
            _playerMap.clear();
        }

        {
            std::lock_guard<std::recursive_mutex> lock(_stackMx);
            auto it = _stackMap.find(id);
            if (it == _stackMap.end()) {
                return;
            }

            _stackMap.erase(it);
        }




    }


    std::weak_ptr<StackPlayer> _weakPlayer;

    std::weak_ptr<VideoStack> _weakPtr;

private:



    std::recursive_mutex _playerMx;
    std::unordered_map<std::string, StackPlayer::Ptr> _playerMap;

    std::recursive_mutex _stackMx;
    std::unordered_map<std::string, VideoStack::Ptr> _stackMap;
};




