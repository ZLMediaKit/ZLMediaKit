/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
#include "RtmpDemuxer.h"
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

/**
 * rtmp媒体源的数据抽象
 * rtmp有关键的三要素，分别是metadata、config帧，普通帧
 * 其中metadata是非必须的，有些编码格式也没有config帧(比如MP3)
 * 只要生成了这三要素，那么要实现rtmp推流、rtmp服务器就很简单了
 * rtmp推拉流协议中，先传递metadata，然后传递config帧，然后一直传递普通帧
 */
class RtmpMediaSource : public MediaSource, public RingDelegate<RtmpPacket::Ptr> {
public:
	typedef std::shared_ptr<RtmpMediaSource> Ptr;
	typedef RingBuffer<RtmpPacket::Ptr> RingType;

	/**
	 * 构造函数
	 * @param vhost 虚拟主机名
	 * @param app 应用名
	 * @param stream_id 流id
	 * @param ring_size 可以设置固定的环形缓冲大小，0则自适应
	 */
	RtmpMediaSource(const string &vhost,
					const string &app,
					const string &stream_id,
					int ring_size = 0) :
			MediaSource(RTMP_SCHEMA, vhost, app, stream_id), _ring_size(ring_size) {
		_metadata = TitleMeta().getMetadata();
	}

	virtual ~RtmpMediaSource() {}

	/**
	 * 	获取媒体源的环形缓冲
	 */
	const RingType::Ptr &getRing() const {
		return _ring;
	}

	/**
	 * 获取播放器个数
	 * @return
	 */
	int readerCount() override {
		return _ring ? _ring->readerCount() : 0;
	}

	/**
	 * 获取metadata
	 */
	const AMFValue &getMetaData() const {
		lock_guard<recursive_mutex> lock(_mtx);
		return _metadata;
	}

	/**
	 * 获取所有的config帧
	 */
	template<typename FUNC>
	void getConfigFrame(const FUNC &f) {
		lock_guard<recursive_mutex> lock(_mtx);
		for (auto &pr : _config_frame_map) {
			f(pr.second);
		}
	}

	/**
	 * 设置metadata
	 */
	virtual void setMetaData(const AMFValue &metadata) {
		lock_guard<recursive_mutex> lock(_mtx);
		_metadata = metadata;
	}

	/**
	 * 输入rtmp包
	 * @param pkt rtmp包
	 * @param key 是否为关键帧
	 */
	void onWrite(const RtmpPacket::Ptr &pkt, bool key = true) override {
		lock_guard<recursive_mutex> lock(_mtx);
		if (pkt->isCfgFrame()) {
			_config_frame_map[pkt->typeId] = pkt;
			return;
		}

		if (!_ring) {
			weak_ptr<RtmpMediaSource> weakSelf = dynamic_pointer_cast<RtmpMediaSource>(shared_from_this());
			auto lam = [weakSelf](const EventPoller::Ptr &, int size, bool) {
				auto strongSelf = weakSelf.lock();
				if (!strongSelf) {
					return;
				}
				strongSelf->onReaderChanged(size);
			};
			_ring = std::make_shared<RingType>(_ring_size, std::move(lam));
			onReaderChanged(0);

			//如果输入了非config帧，
			//那么说明不再可能获取config帧以及metadata,
			//所以我们强制其为已注册
			regist();
		}
		_track_stamps_map[pkt->typeId] = pkt->timeStamp;
		_ring->write(pkt, pkt->isVideoKeyFrame());
		checkNoneReader();
	}

	/**
	 * 获取当前时间戳
	 */
	uint32_t getTimeStamp(TrackType trackType) override {
		lock_guard<recursive_mutex> lock(_mtx);
		switch (trackType) {
			case TrackVideo:
				return _track_stamps_map[MSG_VIDEO];
			case TrackAudio:
				return _track_stamps_map[MSG_AUDIO];
			default:
				return MAX(_track_stamps_map[MSG_VIDEO], _track_stamps_map[MSG_AUDIO]);
		}
	}

private:
	/**
	 * 每次增减消费者都会触发该函数
	 */
	void onReaderChanged(int size) {
		//我们记录最后一次活动时间
		_reader_changed_ticker.resetTime();
		if (size != 0 || totalReaderCount() != 0) {
			//还有消费者正在观看该流
			_async_emit_none_reader = false;
			return;
		}
		_async_emit_none_reader = true;
	}

	/**
	 * 检查是否无人消费该流，
	 * 如果无人消费且超过一定时间会触发onNoneReader事件
	 */
	void checkNoneReader() {
		GET_CONFIG(int, stream_none_reader_delay, General::kStreamNoneReaderDelayMS);
		if (_async_emit_none_reader && _reader_changed_ticker.elapsedTime() > stream_none_reader_delay) {
			_async_emit_none_reader = false;
			onNoneReader();
		}
	}
protected:
	int _ring_size;
	bool _async_emit_none_reader = false;
	mutable recursive_mutex _mtx;
	Ticker _reader_changed_ticker;
	AMFValue _metadata;
	RingBuffer<RtmpPacket::Ptr>::Ptr _ring;
	unordered_map<int, uint32_t> _track_stamps_map;
	unordered_map<int, RtmpPacket::Ptr> _config_frame_map;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPMEDIASOURCE_H_ */
