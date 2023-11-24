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


static std::string testJson
    = R"({"msg":"set_combine_source","gapv":0.002,"gaph":0.001,"width":1920,"urls":[["rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test"],["rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test"],["rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test"],["rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test","rtsp://kkem.me:1554/live/test"]],"id":"89","rows":4,"cols":4,"height":1080,"span":[[[0,0],[1,1]],[[2,3],[3,3]]]})";


static constexpr int MAX_FRAME_SIZE = 24;

class VideoStack : public std::enable_shared_from_this<VideoStack> {
public:
    struct Param {
        int posX;
        int posY;
        int width;
        int height;
        std::string stream_id;

        // RuntimeParam
        //std::chrono::steady_clock::time_point lastInputTime;
        //std::shared_ptr<AVFrame> tmp; // 临时存储缩放后的frame
        int order;
        std::list<mediakit::FFmpegFrame::Ptr> cache;
    };

    VideoStack() = default;
    ~VideoStack() { _isExit = true; }


    // 解析参数 存储到_param中
    void parseParam(const std::string &param = testJson);

    // 创建推流对象
    void init();

    void start();

    void copyToBuf(const std::shared_ptr<AVFrame> &buf, const mediakit::FFmpegFrame::Ptr &frame, const Param &p);

public:
    std::string _stack_id;

    int _width;
    int _height;
    AVPixelFormat _pixfmt = AV_PIX_FMT_YUV420P;
    float _fps = 25.0;
    int _bitRate = 2 * 1024 * 1024;

    bool _isExit;

    std::vector<std::vector<VideoStack::Param>> _params; // 存储参数

    mediakit::DevChannel::Ptr _dev;

    std::vector<std::shared_ptr<AVFrame>> _buffers;

    std::bitset<1024> isReady;
    std::bitset<1024> flag;
};


class StackPlayer : public std::enable_shared_from_this<StackPlayer> {
public:
    using Ptr = std::shared_ptr<StackPlayer>;

    void init(const std::string &url);

    void addStackPtr(VideoStack *that);

    void delStackPtr(VideoStack *that);

    void onFrame(const mediakit::FFmpegFrame::Ptr &frame);

private:
    std::string _url;

    //std::shared_timed_mutex _mx;
    //std::vector<VideoStack*> _stacks; // 需要给哪些Stack对象推送帧数据
    std::unordered_map<std::string, VideoStack *> _stacks;

    mediakit::MediaPlayer::Ptr _player;
};



static std::mutex mx;
static std::unordered_map<std::string, StackPlayer::Ptr> playerMap;
