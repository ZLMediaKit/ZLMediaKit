/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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
#include "RtspMuxer/RtspDemuxer.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtspPlayerImp: public PlayerImp<RtspPlayer,RtspDemuxer> {
public:
	typedef std::shared_ptr<RtspPlayerImp> Ptr;
	RtspPlayerImp(){};
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
	bool onCheckSDP(const string &sdp, const SdpAttr &sdpAttr) override {
		_pRtspMediaSrc = dynamic_pointer_cast<RtspMediaSource>(_pMediaSrc);
		if(_pRtspMediaSrc){
			_pRtspMediaSrc->onGetSDP(sdp);
		}
        _parser.reset(new RtspDemuxer(sdpAttr));
        return true;
	}
	void onRecvRTP(const RtpPacket::Ptr &rtppt, const SdpTrack::Ptr &track) override {
        if(_pRtspMediaSrc){
            _pRtspMediaSrc->onWrite(rtppt,true);
        }
        _parser->inputRtp(rtppt);
        checkInited();
    }

    bool isInited(int analysisMs = 2000) override{
        return true;
    }
private:
	RtspMediaSource::Ptr _pRtspMediaSrc;
    
};

} /* namespace mediakit */

#endif /* SRC_RTP_RTPPARSERTESTER_H_ */
