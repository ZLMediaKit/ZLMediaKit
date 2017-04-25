/*
 * RtmpMediaSource.h
 *
 *  Created on: 2016年9月1日
 *      Author: xzl
 */

#ifndef SRC_RTMP_RTMPMEDIASOURCE_H_
#define SRC_RTMP_RTMPMEDIASOURCE_H_

#include <netinet/in.h>
#include <mutex>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include "amf.h"
#include "Rtmp.h"
#include "config.h"
#include "MediaSender.h"
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

namespace ZL {
namespace Rtmp {


class RtmpMediaSource: public enable_shared_from_this<RtmpMediaSource> {
public:
	typedef std::shared_ptr<RtmpMediaSource> Ptr;
	typedef RingBuffer<RtmpPacket> RingType;
	RtmpMediaSource(const string &strApp, const string &strId) :
			m_strApp(strApp),
			m_strId(strId),
			m_pRing( new RingBuffer<RtmpPacket>(1)),
			m_thPool( MediaSender::sendThread()) {
	}
	virtual ~RtmpMediaSource() {
		unregist();
	}
	const RingType::Ptr &getRing() const {
		//获取媒体源的rtp环形缓冲
		return m_pRing;
	}
	virtual void regist() {
		//注册该源，注册后rtmp服务器才能找到该源
		lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
		if (!g_mapMediaSrc[m_strApp].erase(m_strId)) {
			InfoL << "Rtmp src:" << m_strApp << " " << m_strId;
		}
		g_mapMediaSrc[m_strApp].emplace(m_strId, shared_from_this());
		NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastRtmpSrcRegisted,m_strApp.data(),m_strId.data());
	}
	virtual void unregist() {
		//反注册该源
		lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
		auto it = g_mapMediaSrc.find(m_strApp);
		if (it == g_mapMediaSrc.end()) {
			return;
		}
		if (it->second.erase(m_strId)) {
			if (it->second.size() == 0) {
				g_mapMediaSrc.erase(it);
			}
			InfoL << "Rtmp src:" << m_strApp << " " << m_strId;
		}
	}
	static set<string> getMediaSet() {
		set<string> ret;
		lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
		for (auto &pr0 : g_mapMediaSrc) {
			for (auto &pr1 : pr0.second) {
				if (pr1.second.lock()) {
					ret.emplace(pr0.first + "/" + pr1.first);
				}
			}
		}
		return ret;
	}
	static Ptr find(const string &strApp, const string &strId, bool bMake = true) ;
	const string& getApp() const {
		//获取该源的id
		return m_strApp;
	}
	const string& getId() const {
		//获取该源的id
		return m_strId;
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
	virtual void onGetMedia(const RtmpPacket &_pkt) {
		RtmpPacket & pkt = const_cast<RtmpPacket &>(_pkt);
		if (pkt.isCfgFrame()) {
			lock_guard<recursive_mutex> lock(m_mtxMap);
			m_mapCfgFrame.emplace(pkt.typeId, pkt);
		}
		auto _ring = m_pRing;
		m_thPool.async([_ring,pkt]() {
			_ring->write(pkt);
		});
	}
	bool seekTo(uint32_t ui32Stamp) {
		if (!m_onSeek) {
			return false;
		}
		return m_onSeek(ui32Stamp);
	}
	virtual void setOnSeek(const function<bool(uint32_t)> &cb) {
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
private:
	AMFValue m_metadata;
	unordered_map<int, RtmpPacket> m_mapCfgFrame;
	mutable recursive_mutex m_mtxMap;
	string m_strApp; //媒体app
	string m_strId; //媒体id
	RingBuffer<RtmpPacket>::Ptr m_pRing; //rtp环形缓冲
	ThreadPool &m_thPool;
	static unordered_map<string, unordered_map<string,weak_ptr<RtmpMediaSource> > > g_mapMediaSrc; //静态的媒体源表
	static recursive_mutex g_mtxMediaSrc; ///访问静态的媒体源表的互斥锁
};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPMEDIASOURCE_H_ */
