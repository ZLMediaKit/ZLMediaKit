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
#include "RtmpMuxer/RtmpDemuxer.h"
#include "MediaFile/MediaRecorder.h"
#include "RtspMuxer/RtspMediaSourceMuxer.h"
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
                          bool bEnableMp4 = false,
						  int ringSize = 0):RtmpMediaSource(vhost, app, id,ringSize){
		_recorder = std::make_shared<MediaRecorder>(vhost, app, id, bEnableHls, bEnableMp4);
	}
	virtual ~RtmpToRtspMediaSource(){}

	void onGetMetaData(const AMFValue &_metadata) override {
		_rtmpDemuxer = std::make_shared<RtmpDemuxer>(_metadata);
		RtmpMediaSource::onGetMetaData(_metadata);
	}

	void onWrite(const RtmpPacket::Ptr &pkt,bool key_pos) override {
		if(_rtmpDemuxer){
			_rtmpDemuxer->inputRtmp(pkt);
			if(!_rtspMuxer && _rtmpDemuxer->isInited()){
				_rtspMuxer = std::make_shared<RtspMediaSourceMuxer>(getVhost(),
																	getApp(),
																	getId(),
																	std::make_shared<TitleSdp>(
																			_rtmpDemuxer->getDuration()));
				for (auto &track : _rtmpDemuxer->getTracks(false)){
					_rtspMuxer->addTrack(track);
					_recorder->addTrack(track);
					track->addDelegate(_rtspMuxer);
					track->addDelegate(_recorder);
				}
			}
		}
		RtmpMediaSource::onWrite(pkt,key_pos);
	}
private:
	RtmpDemuxer::Ptr _rtmpDemuxer;
	RtspMediaSourceMuxer::Ptr _rtspMuxer;
	MediaRecorder::Ptr _recorder;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_ */
