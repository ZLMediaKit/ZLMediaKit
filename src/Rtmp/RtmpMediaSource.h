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
#include "Common/config.h"
#include "Common/MediaSender.h"
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
			m_pRing(new RingBuffer<RtmpPacket::Ptr>()),
			m_thPool( MediaSender::sendThread()) {
	}
	virtual ~RtmpMediaSource() {}

	const RingType::Ptr &getRing() const {
		//获取媒体源的rtp环形缓冲
		return m_pRing;
	}

	const AMFValue &getMetaData() const {
		return m_metadata;
	}
	template<typename FUN>
	void getConfigFrame(const FUN &f) {
		lock_guard<recursive_mutex> lock(m_mtxMap);
		for (auto &pr : m_mapCfgFrame) {
			f(pr.second);
		}
	}
	bool ready() const {
		lock_guard<recursive_mutex> lock(m_mtxMap);
		return (m_mapCfgFrame.size() != 0);
	}
	virtual void onGetMetaData(const AMFValue &_metadata) {
		m_metadata = _metadata;
	}
	virtual void onGetMedia(const RtmpPacket::Ptr &pkt) {
		if (pkt->isCfgFrame()) {
			lock_guard<recursive_mutex> lock(m_mtxMap);
			m_mapCfgFrame.emplace(pkt->typeId, pkt);
		}
		auto _ring = m_pRing;
		m_thPool.async([_ring,pkt]() {
			_ring->write(pkt,pkt->isVideoKeyFrame());
		});
	}
protected:
	AMFValue m_metadata;
	unordered_map<int, RtmpPacket::Ptr> m_mapCfgFrame;
	mutable recursive_mutex m_mtxMap;
	RingBuffer<RtmpPacket::Ptr>::Ptr m_pRing; //rtp环形缓冲
	ThreadPool &m_thPool;
};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPMEDIASOURCE_H_ */
