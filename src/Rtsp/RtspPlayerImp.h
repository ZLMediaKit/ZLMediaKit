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

class RtspPlayerImp: public PlayerImp<RtspPlayer,RtspDemuxer> {
public:
	typedef std::shared_ptr<RtspPlayerImp> Ptr;
	RtspPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<RtspPlayer,RtspDemuxer>(poller){}
	virtual ~RtspPlayerImp(){
        DebugL<<endl;
    };
    float getProgress() const override{
        if(getDuration() > 0){
            return getProgressMilliSecond() / (getDuration() * 1000);
        }
        return PlayerBase::getProgress();
        
    };
    void seekTo(float fProgress) override{
        fProgress = MAX(float(0),MIN(fProgress,float(1.0)));
        seekToMilliSecond(fProgress * getDuration() * 1000);
    };
private:
	//派生类回调函数
	bool onCheckSDP(const string &sdp) override {
		_pRtspMediaSrc = dynamic_pointer_cast<RtspMediaSource>(_pMediaSrc);
		if(_pRtspMediaSrc){
            _pRtspMediaSrc->setSdp(sdp);
		}
        _delegate.reset(new RtspDemuxer);
        _delegate->loadSdp(sdp);
        return true;
	}
	void onRecvRTP(const RtpPacket::Ptr &rtp, const SdpTrack::Ptr &track) override {
        if(_pRtspMediaSrc){
            _pRtspMediaSrc->onWrite(rtp,true);
        }
        _delegate->inputRtp(rtp);

        if(_maxAnalysisMS && _delegate->isInited(_maxAnalysisMS)){
            PlayerImp<RtspPlayer,RtspDemuxer>::onPlayResult(SockException(Err_success,"play rtsp success"));
            _maxAnalysisMS = 0;
        }
    }

    //在RtspPlayer中触发onPlayResult事件只是代表收到play回复了，
    //并不代表所有track初始化成功了(这跟rtmp播放器不一样)
    //如果sdp里面信息不完整，只能尝试延后从rtp中恢复关键信息并初始化track
    //如果超过这个时间还未获取成功，那么会强制触发onPlayResult事件(虽然此时有些track还未初始化成功)
    void onPlayResult(const SockException &ex) override {
        //isInited判断条件：无超时
        if(ex || _delegate->isInited(0)){
            //已经初始化成功，说明sdp里面有完善的信息
            PlayerImp<RtspPlayer,RtspDemuxer>::onPlayResult(ex);
        }else{
            //还没初始化成功，说明sdp里面信息不完善，还有一些track未初始化成功
            _maxAnalysisMS = (*this)[Client::kMaxAnalysisMS];
        }
    }
private:
	RtspMediaSource::Ptr _pRtspMediaSrc;
    int _maxAnalysisMS = 0;
};

} /* namespace mediakit */

#endif /* SRC_RTP_RTPPARSERTESTER_H_ */
