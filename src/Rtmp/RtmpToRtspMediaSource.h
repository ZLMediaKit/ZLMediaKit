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

#ifndef SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_
#define SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "Util/util.h"
#include "Util/logger.h"
#include "amf.h"
#include "Rtmp.h"
#include "RtmpMediaSource.h"
#include "RtmpDemuxer.h"
#include "Common/MultiMediaSourceMuxer.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtmpToRtspMediaSource: public RtmpMediaSource {
public:
	typedef std::shared_ptr<RtmpToRtspMediaSource> Ptr;

	RtmpToRtspMediaSource(const string &vhost,
                          const string &app,
                          const string &id,
						  int ringSize = 0) :
            RtmpMediaSource(vhost, app, id,ringSize){
	}
	virtual ~RtmpToRtspMediaSource(){}

	void onGetMetaData(const AMFValue &metadata) override {
		if(!_demuxer){
			//在未调用onWrite前，设置Metadata能触发生成RtmpDemuxer
			_demuxer = std::make_shared<RtmpDemuxer>(metadata);
		}
		RtmpMediaSource::onGetMetaData(metadata);
	}

	void onWrite(const RtmpPacket::Ptr &pkt,bool key_pos = true) override {
		if(!_demuxer){
			//尚未获取Metadata，那么不管有没有Metadata，都生成RtmpDemuxer
			_demuxer = std::make_shared<RtmpDemuxer>();
		}
		_demuxer->inputRtmp(pkt);
		if(!_muxer && _demuxer->isInited(2000)){
			_muxer = std::make_shared<MultiMediaSourceMuxer>(getVhost(),
															 getApp(),
															 getId(),
															 _demuxer->getDuration(),
															 _enableRtsp,
															 false,//不重复生成rtmp
															 _enableHls,
															 _enableMP4);
			for (auto &track : _demuxer->getTracks(false)){
				_muxer->addTrack(track);
				track->addDelegate(_muxer);
			}
			_muxer->setListener(getListener());
		}
		RtmpMediaSource::onWrite(pkt,key_pos);
	}

	void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override {
        RtmpMediaSource::setListener(listener);
        if(_muxer){
			_muxer->setListener(listener);
        }
    }

    int readerCount() override {
        return RtmpMediaSource::readerCount() + (_muxer ? _muxer->readerCount() : 0);
    }

	/**
     * 获取track
     * @return
     */
	vector<Track::Ptr> getTracks(bool trackReady) const override {
		if(!_demuxer){
			return this->RtmpMediaSource::getTracks(trackReady);
		}
		return _demuxer->getTracks(trackReady);
	}

	/**
	 * 设置协议转换
	 * @param enableRtsp 是否转换成rtsp
	 * @param enableHls  是否转换成hls
	 * @param enableMP4  是否mp4录制
	 */
	void setProtocolTranslation(bool enableRtsp,bool enableHls,bool enableMP4){
//		DebugL << enableRtsp << " " << enableHls << " " << enableMP4;
		_enableRtsp = enableRtsp;
		_enableHls = enableHls;
		_enableMP4 = enableMP4;
	}
private:
	RtmpDemuxer::Ptr _demuxer;
	MultiMediaSourceMuxer::Ptr _muxer;
	bool _enableHls = true;
	bool _enableMP4 = false;
	bool _enableRtsp = true;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_ */
