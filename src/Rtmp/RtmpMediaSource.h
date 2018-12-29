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

#ifndef SRC_RTMP_RTMPMEDIASOURCE_H_
#define SRC_RTMP_RTMPMEDIASOURCE_H_

#include <mutex>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include "amf.h"
#include "Rtmp.h"
#include "RtmpMuxer/RtmpDemuxer.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Util/NoticeCenter.h"
#include "Thread/ThreadPool.h"
using namespace toolkit;

namespace mediakit {

class RtmpMediaSource: public MediaSource ,public RingDelegate<RtmpPacket::Ptr> {
public:
	typedef std::shared_ptr<RtmpMediaSource> Ptr;
	typedef RingBuffer<RtmpPacket::Ptr> RingType;

	RtmpMediaSource(const string &vhost,const string &strApp, const string &strId,int ringSize = 0) :
			MediaSource(RTMP_SCHEMA,vhost,strApp,strId),
			_pRing(new RingBuffer<RtmpPacket::Ptr>(ringSize)) {
	}
	virtual ~RtmpMediaSource() {}

	const RingType::Ptr &getRing() const {
		//获取媒体源的rtp环形缓冲
		return _pRing;
	}

	const AMFValue &getMetaData() const {
		lock_guard<recursive_mutex> lock(_mtxMap);
		return _metadata;
	}
	template<typename FUN>
	void getConfigFrame(const FUN &f) {
		lock_guard<recursive_mutex> lock(_mtxMap);
		for (auto &pr : _mapCfgFrame) {
			f(pr.second);
		}
	}


	virtual void onGetMetaData(const AMFValue &metadata) {
		lock_guard<recursive_mutex> lock(_mtxMap);
		_metadata = metadata;
		_iCfgFrameSize = RtmpDemuxer::getTrackCount(metadata);
		if(ready()){
			MediaSource::regist();
			_bRegisted = true;
		} else{
			_bAsyncRegist = true;
		}
	}

    void onWrite(const RtmpPacket::Ptr &pkt,bool isKey = true) override {
		lock_guard<recursive_mutex> lock(_mtxMap);
		if (pkt->isCfgFrame()) {
			_mapCfgFrame[pkt->typeId] = pkt;

			if(_bAsyncRegist && !_bRegisted &&  _mapCfgFrame.size() == _iCfgFrameSize){
				_bAsyncRegist = false;
				MediaSource::regist();
				_bRegisted = true;
			}
		} else{
			_mapStamp[pkt->typeId] = pkt->timeStamp;
			_pRing->write(pkt,pkt->isVideoKeyFrame());
		}
	}

	uint32_t getTimeStamp(TrackType trackType) override {
		lock_guard<recursive_mutex> lock(_mtxMap);
		switch (trackType){
			case TrackVideo:
				return _mapStamp[MSG_VIDEO];
			case TrackAudio:
				return _mapStamp[MSG_AUDIO];
			default:
				return MAX(_mapStamp[MSG_VIDEO],_mapStamp[MSG_AUDIO]);
		}
	}
private:
	bool ready(){
		lock_guard<recursive_mutex> lock(_mtxMap);
		return _iCfgFrameSize != -1 && _iCfgFrameSize == _mapCfgFrame.size();
	}
protected:
	AMFValue _metadata;
	map<int, RtmpPacket::Ptr> _mapCfgFrame;
	map<int,uint32_t> _mapStamp;
	mutable recursive_mutex _mtxMap;
	RingBuffer<RtmpPacket::Ptr>::Ptr _pRing; //rtp环形缓冲
	int _iCfgFrameSize = -1;
	bool _bAsyncRegist = false;
	bool _bRegisted = false;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPMEDIASOURCE_H_ */
