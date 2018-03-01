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
#include "RtpParser.h"
#include "RtspPlayer.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Player;

namespace ZL {
namespace Rtsp {

class RtspPlayerImp: public PlayerImp<RtspPlayer,RtpParser> {
public:
	typedef std::shared_ptr<RtspPlayerImp> Ptr;
	RtspPlayerImp(){};
	virtual ~RtspPlayerImp(){
        DebugL<<endl;
        teardown();
    };
    float getProgress() const override{
        if(getDuration() > 0){
            return getProgressTime() / getDuration();
        }
        return PlayerBase::getProgress();
        
    };
    void seekTo(float fProgress) override{
        fProgress = MAX(float(0),MIN(fProgress,float(1.0)));
        seekToTime(fProgress * getDuration());
    };
private:
	//派生类回调函数
	bool onCheckSDP(const string &sdp, const RtspTrack *track, int trackCnt) override {
		try {
			m_parser.reset(new RtpParser(sdp));
			m_parser->setOnVideoCB(m_onGetVideoCB);
			m_parser->setOnAudioCB(m_onGetAudioCB);
			return true;
		} catch (std::exception &ex) {
			WarnL << ex.what();
			return false;
		}
	}
	void onRecvRTP(const RtpPacket::Ptr &rtppt, const RtspTrack &track) override {
		m_parser->inputRtp(*rtppt);
	}
    
};

} /* namespace Rtsp */
} /* namespace ZL */

#endif /* SRC_RTP_RTPPARSERTESTER_H_ */
