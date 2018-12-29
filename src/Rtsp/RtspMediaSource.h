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

#ifndef SRC_RTSP_RTSPMEDIASOURCE_H_
#define SRC_RTSP_RTSPMEDIASOURCE_H_

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "Rtsp.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "RtspMuxer/RtpCodec.h"

#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Util/NoticeCenter.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtspMediaSource: public MediaSource , public RingDelegate<RtpPacket::Ptr> {
public:
	typedef ResourcePool<RtpPacket> PoolType;
	typedef std::shared_ptr<RtspMediaSource> Ptr;
	typedef RingBuffer<RtpPacket::Ptr> RingType;

	RtspMediaSource(const string &strVhost,const string &strApp, const string &strId,int ringSize = 0) :
			MediaSource(RTSP_SCHEMA,strVhost,strApp,strId),
			_pRing(new RingBuffer<RtpPacket::Ptr>(ringSize)) {
	}
	virtual ~RtspMediaSource() {}

	const RingType::Ptr &getRing() const {
		//获取媒体源的rtp环形缓冲
		return _pRing;
	}
	const string& getSdp() const {
		//获取该源的媒体描述信息
		return _strSdp;
	}

	virtual uint32_t getSsrc(TrackType trackType) {
		auto track = _sdpAttr.getTrack(trackType);
		if(!track){
			return 0;
		}
		return track->_ssrc;
	}
	virtual uint16_t getSeqence(TrackType trackType) {
		auto track = _sdpAttr.getTrack(trackType);
		if(!track){
			return 0;
		}
		return track->_seq;
	}

	uint32_t getTimeStamp(TrackType trackType) override {
		auto track = _sdpAttr.getTrack(trackType);
		if(track) {
			return track->_time_stamp;
		}
		auto tracks = _sdpAttr.getAvailableTrack();
		switch (tracks.size()){
			case 0: return 0;
			case 1: return tracks[0]->_time_stamp;
			default:return MAX(tracks[0]->_time_stamp,tracks[1]->_time_stamp);
		}
	}

	virtual void setTimeStamp(uint32_t uiStamp) {
		auto tracks = _sdpAttr.getAvailableTrack();
		for (auto &track : tracks) {
			track->_time_stamp  = uiStamp;
		}
	}

	virtual void onGetSDP(const string& sdp) {
		//派生类设置该媒体源媒体描述信息
		_strSdp = sdp;
		_sdpAttr.load(sdp);
		regist();
	}

	void onWrite(const RtpPacket::Ptr &rtppt, bool keyPos) override {
		auto track = _sdpAttr.getTrack(rtppt->type);
		if(track){
			track->_seq = rtppt->sequence;
			track->_time_stamp = rtppt->timeStamp;
			track->_ssrc = rtppt->ssrc;
		}
		_pRing->write(rtppt,keyPos);
	}
protected:
	SdpAttr _sdpAttr;
    string _strSdp; //媒体描述信息
    RingType::Ptr _pRing; //rtp环形缓冲
};

} /* namespace mediakit */

#endif /* SRC_RTSP_RTSPMEDIASOURCE_H_ */
