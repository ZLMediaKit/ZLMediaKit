#pragma once
#if defined(ENABLE_VIDEOSTACK) && defined(ENABLE_X264) && defined(ENABLE_FFMPEG)
#include "Codec/Transcode.h"
#include "Common/Device.h"
#include "Player/MediaPlayer.h"
#include "json/json.h"
#include <mutex>
template <typename T>
class RefWrapper {
 public:
    using Ptr = std::shared_ptr<RefWrapper<T>>;

    template <typename... Args>
    explicit RefWrapper(Args&&... args)
        : _rc(0)
        , _entity(std::forward<Args>(args)...)
    {
    }

    T acquire()
    {
        ++_rc;
        return _entity;
    }

    bool dispose() { return --_rc <= 0; }

 private:
    T _entity;
    std::atomic<int> _rc;
};

class Channel;

struct Param {
    using Ptr = std::shared_ptr<Param>;

    int posX = 0;
    int posY = 0;
    int width = 0;
    int height = 0;
    AVPixelFormat pixfmt = AV_PIX_FMT_YUV420P;
    std::string id {};

    // runtime
    std::weak_ptr<Channel> weak_chn;
    std::weak_ptr<mediakit::FFmpegFrame> weak_buf;

    ~Param();
};

using Params = std::shared_ptr<std::vector<Param::Ptr>>;

class Channel : public std::enable_shared_from_this<Channel> {
 public:
    using Ptr = std::shared_ptr<Channel>;

    Channel(const std::string& id, int width, int height, AVPixelFormat pixfmt);

    void addParam(const std::weak_ptr<Param>& p);

    void onFrame(const mediakit::FFmpegFrame::Ptr& frame);

    void fillBuffer(const Param::Ptr& p);

 protected:
    void forEachParam(const std::function<void(const Param::Ptr&)>& func);

    void copyData(const mediakit::FFmpegFrame::Ptr& buf, const Param::Ptr& p);

 private:
    std::string _id;
    int _width;
    int _height;
    AVPixelFormat _pixfmt;

    mediakit::FFmpegFrame::Ptr _tmp;

    std::recursive_mutex _mx;
    std::vector<std::weak_ptr<Param>> _params;

    mediakit::FFmpegSws::Ptr _sws;
    toolkit::EventPoller::Ptr _poller;
};

class StackPlayer : public std::enable_shared_from_this<StackPlayer> {
 public:
    using Ptr = std::shared_ptr<StackPlayer>;

    StackPlayer(const std::string& url)
        : _url(url)
    {
    }

    void addChannel(const std::weak_ptr<Channel>& chn);

    void play();

    void onFrame(const mediakit::FFmpegFrame::Ptr& frame);

    void onDisconnect();

 protected:
    void rePlay(const std::string& url);

 private:
    std::string _url;
    mediakit::MediaPlayer::Ptr _player;

    //用于断线重连
    toolkit::Timer::Ptr _timer;
    int _failedCount = 0;

    std::recursive_mutex _mx;
    std::vector<std::weak_ptr<Channel>> _channels;
};

class VideoStack {
 public:
    using Ptr = std::shared_ptr<VideoStack>;

    VideoStack(const std::string& url,
        int width = 1920,
        int height = 1080,
        AVPixelFormat pixfmt = AV_PIX_FMT_YUV420P,
        float fps = 25.0,
        int bitRate = 2 * 1024 * 1024);

    ~VideoStack();

    void setParam(const Params& params);

    void start();

 protected:
    void initBgColor();

 public:
    Params _params;

    mediakit::FFmpegFrame::Ptr _buffer;

 private:
    std::string _id;
    int _width;
    int _height;
    AVPixelFormat _pixfmt;
    float _fps;
    int _bitRate;

    mediakit::DevChannel::Ptr _dev;

    bool _isExit;

    std::thread _thread;
};

class VideoStackManager {
 public:
    static VideoStackManager& Instance();

    Channel::Ptr getChannel(const std::string& id,
        int width,
        int height,
        AVPixelFormat pixfmt);

    void unrefChannel(const std::string& id,
        int width,
        int height,
        AVPixelFormat pixfmt);

    int startVideoStack(const Json::Value& json);

    int resetVideoStack(const Json::Value& json);

    int stopVideoStack(const std::string& id);

    bool loadBgImg(const std::string& path);

    mediakit::FFmpegFrame::Ptr getBgImg();

 protected:
    Params parseParams(const Json::Value& json,
        std::string& id,
        int& width,
        int& height);

 protected:
    Channel::Ptr createChannel(const std::string& id,
        int width,
        int height,
        AVPixelFormat pixfmt);

    StackPlayer::Ptr createPlayer(const std::string& id);

 private:
    mediakit::FFmpegFrame::Ptr _bgImg;

 private:
    std::recursive_mutex _mx;

    std::unordered_map<std::string, VideoStack::Ptr> _stackMap;

    std::unordered_map<std::string, RefWrapper<Channel::Ptr>::Ptr> _channelMap;

    std::unordered_map<std::string, RefWrapper<StackPlayer::Ptr>::Ptr> _playerMap;
};
#endif