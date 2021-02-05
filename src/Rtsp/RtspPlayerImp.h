/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTP_RTPPARSERTESTER_H_
#define SRC_RTP_RTPPARSERTESTER_H_

#include <memory>
#include <algorithm>
#include <functional>
#include "Common/config.h"
#include "RtspPlayer.h"
#include "RtspDemuxer.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtspPlayerImp : public PlayerImp<RtspPlayer,RtspDemuxer> {
public:
    typedef std::shared_ptr<RtspPlayerImp> Ptr;

    RtspPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<RtspPlayer, RtspDemuxer>(poller) {}
    ~RtspPlayerImp() override{
        DebugL << endl;
    }

    float getProgress() const override {
        if (getDuration() > 0) {
            return getProgressMilliSecond() / (getDuration() * 1000);
        }
        return PlayerBase::getProgress();

    }

    void seekTo(float fProgress) override {
        fProgress = MAX(float(0), MIN(fProgress, float(1.0)));
        seekToMilliSecond((uint32_t)(fProgress * getDuration() * 1000));
    }

private:
    //派生类回调函数
    bool onCheckSDP(const string &sdp) override {
        _rtsp_media_src = dynamic_pointer_cast<RtspMediaSource>(_pMediaSrc);
        if (_rtsp_media_src) {
            _rtsp_media_src->setSdp(sdp);
        }
        _delegate.reset(new RtspDemuxer);
        _delegate->loadSdp(sdp);
        return true;
    }

    void onRecvRTP(RtpPacket::Ptr rtp, const SdpTrack::Ptr &track) override {
        _delegate->inputRtp(rtp);
        if (_max_analysis_ms && _delegate->isInited(_max_analysis_ms)) {
            PlayerImp<RtspPlayer, RtspDemuxer>::onPlayResult(SockException(Err_success, "play rtsp success"));
            _max_analysis_ms = 0;
        }

        if (_rtsp_media_src) {
            // rtsp直接代理是无法判断该rtp是否是I帧，所以GOP缓存基本是无效的
            // 为了减少内存使用，那么我们设置为一直关键帧以便清空GOP缓存
            _rtsp_media_src->onWrite(std::move(rtp), true);
        }
    }

    //在RtspPlayer中触发onPlayResult事件只是代表收到play回复了，
    //并不代表所有track初始化成功了(这跟rtmp播放器不一样)
    //如果sdp里面信息不完整，只能尝试延后从rtp中恢复关键信息并初始化track
    //如果超过这个时间还未获取成功，那么会强制触发onPlayResult事件(虽然此时有些track还未初始化成功)
    void onPlayResult(const SockException &ex) override {
        //isInited判断条件：无超时
        if (ex || _delegate->isInited(0)) {
            //已经初始化成功，说明sdp里面有完善的信息
            PlayerImp<RtspPlayer, RtspDemuxer>::onPlayResult(ex);
        } else {
            //还没初始化成功，说明sdp里面信息不完善，还有一些track未初始化成功
            _max_analysis_ms = (*this)[Client::kMaxAnalysisMS];
        }
    }

private:
    int _max_analysis_ms = 0;
    RtspMediaSource::Ptr _rtsp_media_src;
};

} /* namespace mediakit */

#endif /* SRC_RTP_RTPPARSERTESTER_H_ */
