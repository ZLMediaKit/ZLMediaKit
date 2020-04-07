/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
