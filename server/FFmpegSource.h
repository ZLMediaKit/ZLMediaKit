//
// Created by xzl on 2018/5/24.
//

#ifndef FFMPEG_SOURCE_H
#define FFMPEG_SOURCE_H

#include <mutex>
#include <memory>
#include <functional>
#include "Process.h"
#include "Util/TimeTicker.h"
#include "Network/Socket.h"
#include "Common/MediaSource.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

class FFmpegSource : public std::enable_shared_from_this<FFmpegSource>{
public:
    typedef shared_ptr<FFmpegSource> Ptr;
    typedef function<void(const SockException &ex)> onPlay;

    FFmpegSource();
    virtual ~FFmpegSource();
    /**
     * 设置主动关闭回调
     * @param cb
     */
    void setOnClose(const function<void()> &cb);
    void play(const string &src_url,const string &dst_url,int timeout_ms,const onPlay &cb);
private:
    void findAsync(int maxWaitMS ,const function<void(const MediaSource::Ptr &src)> &cb);
    void startTimer(int timeout_ms);
private:
    Process _process;
    Timer::Ptr _timer;
    EventPoller::Ptr _poller;
    MediaInfo _media_info;
    string _src_url;
    string _dst_url;
    function<void()> _onClose;
};


#endif //FFMPEG_SOURCE_H
