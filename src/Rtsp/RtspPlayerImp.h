/*
 * RtspParserTester.h
 *
 *  Created on: 2016年9月5日
 *      Author: xzl
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
	RtspPlayerImp();
	virtual ~RtspPlayerImp();
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
