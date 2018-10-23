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
#include "RtmpParser.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Util/util.h"
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
namespace Rtmp {

class RtmpMediaSource: public MediaSource {
public:
	typedef std::shared_ptr<RtmpMediaSource> Ptr;
	typedef RingBuffer<RtmpPacket::Ptr> RingType;

	RtmpMediaSource(const string &vhost,const string &strApp, const string &strId) :
			MediaSource(RTMP_SCHEMA,vhost,strApp,strId),
			m_pRing(new RingBuffer<RtmpPacket::Ptr>()) {
	}
	virtual ~RtmpMediaSource() {}

	const RingType::Ptr &getRing() const {
		//获取媒体源的rtp环形缓冲
		return m_pRing;
	}

	const AMFValue &getMetaData() const {
		lock_guard<recursive_mutex> lock(m_mtxMap);
		return m_metadata;
	}
	template<typename FUN>
	void getConfigFrame(const FUN &f) {
		lock_guard<recursive_mutex> lock(m_mtxMap);
		for (auto &pr : m_mapCfgFrame) {
			f(pr.second);
		}
	}

	virtual void onGetMetaData(const AMFValue &_metadata) {
		lock_guard<recursive_mutex> lock(m_mtxMap);
		m_metadata = _metadata;
		RtmpParser parser(_metadata);
		m_iCfgFrameSize = parser.getTrackCount();
		if(ready()){
			MediaSource::regist();
			m_bRegisted = true;
		} else{
			m_bAsyncRegist = true;
		}
	}
	virtual void onGetMedia(const RtmpPacket::Ptr &pkt) {
		lock_guard<recursive_mutex> lock(m_mtxMap);
		if (pkt->isCfgFrame()) {
			m_mapCfgFrame.emplace(pkt->typeId, pkt);

			if(m_bAsyncRegist && !m_bRegisted &&  m_mapCfgFrame.size() == m_iCfgFrameSize){
				m_bAsyncRegist = false;
				MediaSource::regist();
				m_bRegisted = true;
			}
		}

		m_pRing->write(pkt,pkt->isVideoKeyFrame());
	}
private:
	bool ready(){
		lock_guard<recursive_mutex> lock(m_mtxMap);
		return m_iCfgFrameSize != -1 && m_iCfgFrameSize == m_mapCfgFrame.size();
	}
protected:
	AMFValue m_metadata;
	unordered_map<int, RtmpPacket::Ptr> m_mapCfgFrame;
	mutable recursive_mutex m_mtxMap;
	RingBuffer<RtmpPacket::Ptr>::Ptr m_pRing; //rtp环形缓冲
	int m_iCfgFrameSize = -1;
	bool m_bAsyncRegist = false;
	bool m_bRegisted = false;
};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPMEDIASOURCE_H_ */
