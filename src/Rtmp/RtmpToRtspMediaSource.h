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
                          bool bEnableHls = true,
                          //chenxiaolei 修改为int, 录像最大录制天数,0就是不录
                          int bRecordMp4 = 0,
						  int ringSize = 0) : RtmpMediaSource(vhost, app, id,ringSize){
		_bEnableHls = bEnableHls;
		_bRecordMp4 = bRecordMp4;
		_demuxer = std::make_shared<RtmpDemuxer>();
	}
	virtual ~RtmpToRtspMediaSource(){}

	void onGetMetaData(const AMFValue &metadata) override {
		_demuxer = std::make_shared<RtmpDemuxer>(metadata);
		RtmpMediaSource::onGetMetaData(metadata);
	}

	void onWrite(const RtmpPacket::Ptr &pkt,bool key_pos) override {
		_demuxer->inputRtmp(pkt);
		if(!_muxer && _demuxer->isInited(2000)){
			_muxer = std::make_shared<MultiMediaSourceMuxer>(getVhost(),
															 getApp(),
															 getId(),
															 _demuxer->getDuration(),
															 true,//转rtsp
															 false,//不重复生成rtmp
															 _bEnableHls,
															 _bRecordMp4);
			for (auto &track : _demuxer->getTracks(false)){
				_muxer->addTrack(track);
				track->addDelegate(_muxer);
			}
			_muxer->setListener(_listener);
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
private:
	RtmpDemuxer::Ptr _demuxer;
	MultiMediaSourceMuxer::Ptr _muxer;
	bool _bEnableHls;
    //chenxiaolei 修改为int, 录像最大录制天数,0就是不录
	bool _bRecordMp4;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_ */
