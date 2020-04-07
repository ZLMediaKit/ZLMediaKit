/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

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

class FFmpegSource : public std::enable_shared_from_this<FFmpegSource> , public MediaSourceEvent{
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
    void onGetMediaSource(const MediaSource::Ptr &src);

    //MediaSourceEvent override
    bool close(MediaSource &sender,bool force) override;
    int totalReaderCount(MediaSource &sender) override;
private:
    Process _process;
    Timer::Ptr _timer;
    EventPoller::Ptr _poller;
    MediaInfo _media_info;
    string _src_url;
    string _dst_url;
    function<void()> _onClose;
    std::weak_ptr<MediaSourceEvent> _listener;
    Ticker _replay_ticker;
};


#endif //FFMPEG_SOURCE_H
