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
#include "RtpCodec/RtpCodec.h"

#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Util/NoticeCenter.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;
using namespace ZL::Media;

namespace ZL {
namespace Rtsp {

class RtspMediaSource: public MediaSource {
public:
	typedef ResourcePool<RtpPacket> PoolType;
	typedef std::shared_ptr<RtspMediaSource> Ptr;
	typedef RingBuffer<RtpPacket::Ptr> RingType;

	RtspMediaSource(const string &strVhost,const string &strApp, const string &strId) :
			MediaSource(RTSP_SCHEMA,strVhost,strApp,strId),
			_pRing(new RingBuffer<RtpPacket::Ptr>()) {
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
		return _mapTracks[trackType].ssrc;
	}
	virtual uint16_t getSeqence(TrackType trackType) {
		return _mapTracks[trackType].seq;
	}
	virtual uint32_t getTimestamp(TrackType trackType) {
		return _mapTracks[trackType].timeStamp;
	}

	virtual void onGetSDP(const string& sdp) {
		//派生类设置该媒体源媒体描述信息
		_strSdp = sdp;
		regist();
	}
	virtual void onGetRTP(const RtpPacket::Ptr &rtppt, bool keyPos) {
		auto &trackRef = _mapTracks[rtppt->type];
		trackRef.seq = rtppt->sequence;
		trackRef.timeStamp = rtppt->timeStamp;
		trackRef.ssrc = rtppt->ssrc;
		trackRef.type = rtppt->type;
		_pRing->write(rtppt,keyPos);
	}
protected:
	unordered_map<int, RtspTrack> _mapTracks;
    string _strSdp; //媒体描述信息
    RingType::Ptr _pRing; //rtp环形缓冲
};

} /* namespace Rtsp */
} /* namespace ZL */

#endif /* SRC_RTSP_RTSPMEDIASOURCE_H_ */
