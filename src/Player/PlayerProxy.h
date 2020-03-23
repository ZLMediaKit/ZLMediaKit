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

#ifndef SRC_DEVICE_PLAYERPROXY_H_
#define SRC_DEVICE_PLAYERPROXY_H_

#include <memory>
#include "Common/Device.h"
#include "Player/MediaPlayer.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace toolkit;


namespace mediakit {

class PlayerProxy :public MediaPlayer,
                   public std::enable_shared_from_this<PlayerProxy> ,
                   public MediaSourceEvent{
public:
    typedef std::shared_ptr<PlayerProxy> Ptr;

    //如果iRetryCount<0,则一直重试播放；否则重试iRetryCount次数
    //默认一直重试
    PlayerProxy(const string &strVhost,
                const string &strApp,
                const string &strSrc,
                bool bEnableRtsp = true,
                bool bEnableRtmp = true,
                bool bEnableHls = true,
                bool bEnableMp4 = false,
                int iRetryCount = -1,
                const EventPoller::Ptr &poller = nullptr);

    virtual ~PlayerProxy();

    /**
     * 设置play结果回调，只触发一次；在play执行之前有效
     * @param cb
     */
    void setPlayCallbackOnce(const function<void(const SockException &ex)> &cb);

    /**
     * 设置主动关闭回调
     * @param cb
     */
    void setOnClose(const function<void()> &cb);

    /**
     * 开始拉流播放
     * @param strUrl
     */
    void play(const string &strUrl) override;

    /**
     * 获取观看总人数
     */
    int totalReaderCount() ;
private:
    //MediaSourceEvent override
    bool close(MediaSource &sender,bool force) override;
    int totalReaderCount(MediaSource &sender) override;
    void rePlay(const string &strUrl,int iFailedCnt);
    void onPlaySuccess();
private:
    bool _bEnableRtsp;
    bool _bEnableRtmp;
    bool _bEnableHls;
    bool _bEnableMp4;
    int _iRetryCount;
    MultiMediaSourceMuxer::Ptr _mediaMuxer;
    string _strVhost;
    string _strApp;
    string _strSrc;
    Timer::Ptr _timer;
    function<void(const SockException &ex)> _playCB;
    function<void()> _onClose;
};

} /* namespace mediakit */

#endif /* SRC_DEVICE_PLAYERPROXY_H_ */
