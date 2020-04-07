#ifndef STREAMPROXYPUSHER_H
#define STREAMPROXYPUSHER_H

#include <string>
#include <map>
#include <mutex>
#include "Player/PlayerProxy.h"
#include "Pusher/MediaPusher.h"

class StreamProxyPusher : public std::enable_shared_from_this<StreamProxyPusher> {
public:
    typedef shared_ptr<StreamProxyPusher> Ptr;

    StreamProxyPusher();

    ~StreamProxyPusher();

    void createPusher(const string schema, const string vhost, const string app, const string stream, const string url);

    void
    rePushDelay(const string &schema, const string &vhost, const string &app, const string &stream, const string &url);

    void play(const string &vhost, const string &app, const string &stream, const string &url, bool enable_rtsp,
              bool enable_rtmp, bool enable_hls, bool enable_mp4,
              int rtp_type, const function<void(const SockException &ex)> &cb,
              const string &dst_url);

    void setOnClose(const function<void()> &cb);

private:
    EventPoller::Ptr _poller;

    PlayerProxy::Ptr _player = 0;

    MediaPusher::Ptr _pusher = 0;

    std::mutex _mutex;

    function<void()> _onClose;

    Timer::Ptr _timer;

    std::string _schema = "";
    std::string _vhost = "";
    std::string _app = "";
    std::string _stream = "";
    std::string _url = "";
    std::string _dstUrl = "";
};

#endif // STREAMPROXYPUSHER_H
