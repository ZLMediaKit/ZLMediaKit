/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
