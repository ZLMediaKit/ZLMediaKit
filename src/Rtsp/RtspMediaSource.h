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
#include "Common/MediaSender.h"
#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Util/NoticeCenter.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Thread;

namespace ZL {
namespace Rtsp {



class RtspMediaSource: public enable_shared_from_this<RtspMediaSource> {
public:
	typedef ResourcePool<RtpPacket, 64> PoolType;
	typedef std::shared_ptr<RtspMediaSource> Ptr;
	typedef RingBuffer<RtpPacket::Ptr> RingType;
	RtspMediaSource(const string &strApp, const string &strId) :
			m_strApp(strApp),
			m_strId(strId),
			m_pRing(new RingBuffer<RtpPacket::Ptr>()),
			m_thPool(MediaSender::sendThread()) {
	}
	virtual ~RtspMediaSource() {
		unregist();
	}

	const RingType::Ptr &getRing() const {
		//获取媒体源的rtp环形缓冲
		return m_pRing;
	}

	const string& getSdp() const {
		//获取该源的媒体描述信息
		return m_strSdp;
	}
	virtual void regist() {
		//注册该源，注册后rtsp服务器才能找到该源
		lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
		if (!g_mapMediaSrc[m_strApp].erase(m_strId)) {
			InfoL << "Rtsp src:" << m_strApp << " " << m_strId;
		}
		g_mapMediaSrc[m_strApp].emplace(m_strId, shared_from_this());
		NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastRtspSrcRegisted,m_strApp.data(),m_strId.data());
	}
	virtual void unregist() {
		//反注册该源
		lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
		auto it = g_mapMediaSrc.find(m_strApp);
		if (it == g_mapMediaSrc.end()) {
			return;
		}
		if (it->second.erase(m_strId)) {
			if(it->second.size() == 0){
				g_mapMediaSrc.erase(it);
			}
			InfoL << "Rtsp src:" << m_strApp << " " << m_strId;
		}
	}
	static set<string> getMediaSet() {
		set<string> ret;
		lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
		for (auto &pr0 : g_mapMediaSrc) {
			for (auto &pr1 : pr0.second) {
				if(pr1.second.lock()){
					ret.emplace(pr0.first + "/" + pr1.first);
				}
			}
		}
		return ret;
	}
	static Ptr find(const string &_app, const string &_id,bool bMake = true) ;

	const string& getApp() const {
		//获取该源的id
		return m_strApp;
	}
	const string& getId() const {
		return m_strId;
	}
	virtual uint32_t getSsrc(int trackId) {
		return m_mapTracks[trackId].ssrc;
	}
	virtual uint16_t getSeqence(int trackId) {
		return m_mapTracks[trackId].seq;
	}
	virtual uint32_t getTimestamp(int trackId) {
		return m_mapTracks[trackId].timeStamp;
	}

	virtual void onGetSDP(const string& sdp) {
		//派生类设置该媒体源媒体描述信息
		this->m_strSdp = sdp;
	}
	virtual void onGetRTP(const RtpPacket::Ptr &rtppt, bool keyPos) {
		auto &trackRef = m_mapTracks[rtppt->interleaved / 2];
		trackRef.seq = rtppt->sequence;
		trackRef.timeStamp = rtppt->timeStamp;
		trackRef.ssrc = rtppt->ssrc;
		trackRef.type = rtppt->type;
		auto _outRing = m_pRing;
		m_thPool.async([_outRing,rtppt,keyPos]() {
			_outRing->write(rtppt,keyPos);
		});
	}
	bool seekTo(uint32_t ui32Stamp) {
		if (!m_onSeek) {
			return false;
		}
		return m_onSeek(ui32Stamp);
	}
	virtual void setOnSeek(const function<bool(uint32_t)> &cb){
		m_onSeek = cb;
	}
	uint32_t getStamp() {
		if (!m_onStamp) {
			return 0;
		}
		return m_onStamp();
	}
	virtual void setOnStamp(const function<uint32_t()> &cb) {
		m_onStamp = cb;
	}
protected:
	function<bool(uint32_t)> m_onSeek;
	function<uint32_t()> m_onStamp;
	unordered_map<int, RtspTrack> m_mapTracks;
private:
	string m_strSdp; //媒体描述信息
	string m_strApp; //媒体app
	string m_strId; //媒体id
	RingType::Ptr m_pRing; //rtp环形缓冲
	ThreadPool &m_thPool;
	static unordered_map<string, unordered_map<string, weak_ptr<RtspMediaSource> > > g_mapMediaSrc; //静态的媒体源表
	static recursive_mutex g_mtxMediaSrc; ///访问静态的媒体源表的互斥锁
};

} /* namespace Rtsp */
} /* namespace ZL */

#endif /* SRC_RTSP_RTSPMEDIASOURCE_H_ */
