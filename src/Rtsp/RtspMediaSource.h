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

#ifndef SRC_RTSP_RTSPMEDIASOURCE_H_
#define SRC_RTSP_RTSPMEDIASOURCE_H_

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "RtpCodec.h"

#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Util/NoticeCenter.h"
#include "Thread/ThreadPool.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

/**
 * rtsp媒体源的数据抽象
 * rtsp有关键的两要素，分别是sdp、rtp包
 * 只要生成了这两要素，那么要实现rtsp推流、rtsp服务器就很简单了
 * rtsp推拉流协议中，先传递sdp，然后再协商传输方式(tcp/udp/组播)，最后一直传递rtp
 */
class RtspMediaSource : public MediaSource, public RingDelegate<RtpPacket::Ptr> {
public:
	typedef ResourcePool<RtpPacket> PoolType;
	typedef std::shared_ptr<RtspMediaSource> Ptr;
	typedef RingBuffer<RtpPacket::Ptr> RingType;

	/**
	 * 构造函数
	 * @param vhost 虚拟主机名
	 * @param app 应用名
	 * @param stream_id 流id
	 * @param ring_size 可以设置固定的环形缓冲大小，0则自适应
	 */
	RtspMediaSource(const string &vhost,
					const string &app,
					const string &stream_id,
					int ring_size = 0) :
			MediaSource(RTSP_SCHEMA, vhost, app, stream_id), _ring_size(ring_size) {}

	virtual ~RtspMediaSource() {}

	/**
	 * 获取媒体源的环形缓冲
	 */
	const RingType::Ptr &getRing() const {
		return _ring;
	}

	/**
	 * 获取播放器个数
	 */
	int readerCount() override {
		return _ring ? _ring->readerCount() : 0;
	}

	/**
	 * 获取该源的sdp
	 */
	const string &getSdp() const {
		return _sdp;
	}

	/**
	 * 获取相应轨道的ssrc
	 */
	virtual uint32_t getSsrc(TrackType trackType) {
		auto track = _sdp_parser.getTrack(trackType);
		if (!track) {
			return 0;
		}
		return track->_ssrc;
	}

	/**
	 * 获取相应轨道的seqence
	 */
	virtual uint16_t getSeqence(TrackType trackType) {
		auto track = _sdp_parser.getTrack(trackType);
		if (!track) {
			return 0;
		}
		return track->_seq;
	}

	/**
	 * 获取相应轨道的时间戳，单位毫秒
	 */
	uint32_t getTimeStamp(TrackType trackType) override {
		auto track = _sdp_parser.getTrack(trackType);
		if (track) {
			return track->_time_stamp;
		}
		auto tracks = _sdp_parser.getAvailableTrack();
		switch (tracks.size()) {
			case 0:
				return 0;
			case 1:
				return tracks[0]->_time_stamp;
			default:
				return MAX(tracks[0]->_time_stamp, tracks[1]->_time_stamp);
		}
	}

	/**
	 * 更新时间戳
	 */
	 void setTimeStamp(uint32_t uiStamp) override {
		auto tracks = _sdp_parser.getAvailableTrack();
		for (auto &track : tracks) {
			track->_time_stamp = uiStamp;
		}
	}

	/**
	 * 设置sdp
	 */
	virtual void setSdp(const string &sdp) {
		_sdp = sdp;
		_sdp_parser.load(sdp);
		if (_ring) {
			regist();
		}
	}

	/**
	 * 输入rtp
	 * @param rtp rtp包
	 * @param keyPos 该包是否为关键帧的第一个包
	 */
	void onWrite(const RtpPacket::Ptr &rtp, bool keyPos) override {
		auto track = _sdp_parser.getTrack(rtp->type);
		if (track) {
			track->_seq = rtp->sequence;
			track->_time_stamp = rtp->timeStamp;
			track->_ssrc = rtp->ssrc;
		}
		if (!_ring) {
			weak_ptr<RtspMediaSource> weakSelf = dynamic_pointer_cast<RtspMediaSource>(shared_from_this());
			auto lam = [weakSelf](const EventPoller::Ptr &, int size, bool) {
				auto strongSelf = weakSelf.lock();
				if (!strongSelf) {
					return;
				}
				strongSelf->onReaderChanged(size);
			};
			_ring = std::make_shared<RingType>(_ring_size, std::move(lam));
			onReaderChanged(0);
			if (!_sdp.empty()) {
				regist();
			}
		}
		_ring->write(rtp, keyPos);
		checkNoneReader();
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
	Ticker _reader_changed_ticker;
	SdpParser _sdp_parser;
	string _sdp;
	RingType::Ptr _ring;
};

} /* namespace mediakit */

#endif /* SRC_RTSP_RTSPMEDIASOURCE_H_ */
