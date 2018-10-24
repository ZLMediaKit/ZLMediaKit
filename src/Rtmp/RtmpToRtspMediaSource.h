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
#include "amf.h"
#include "Rtmp.h"
#include "RtmpMediaSource.h"
#include "RtspMuxer/RtpMakerH264.h"
#include "RtspMuxer/RtpMakerAAC.h"
#include "RtmpMuxer/RtmpDemuxer.h"
#include "Rtsp/RtspMediaSource.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "MediaFile/MediaRecorder.h"
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
                          bool bEnableMp4 = false);
	virtual ~RtmpToRtspMediaSource();

	void onGetMetaData(const AMFValue &_metadata) override {
		try {
			_pParser.reset(new RtmpDemuxer(_metadata));
			_pRecorder.reset(new MediaRecorder(getVhost(),getApp(),getId(),_pParser,_bEnableHls,_bEnableMp4));
			//todo(xzl) 修复此处

//			_pParser->setOnAudioCB(std::bind(&RtmpToRtspMediaSource::onGetAAC, this, placeholders::_1));
//			_pParser->setOnVideoCB(std::bind(&RtmpToRtspMediaSource::onGetH264, this, placeholders::_1));
		} catch (exception &ex) {
			WarnL << ex.what();
		}
		RtmpMediaSource::onGetMetaData(_metadata);
	}

	void onGetMedia(const RtmpPacket::Ptr &pkt) override {
		if (_pParser) {
			if (!_pRtspSrc && _pParser->isInited()) {
				makeSDP();
			}
			_pParser->inputRtmp(pkt);
		}
		RtmpMediaSource::onGetMedia(pkt);
	}

private:
	RtmpDemuxer::Ptr _pParser;
	RtspMediaSource::Ptr _pRtspSrc;
	RtpMaker_AAC::Ptr _pRtpMaker_aac;
	RtpMaker_H264::Ptr _pRtpMaker_h264;
	MediaRecorder::Ptr _pRecorder;
    bool _bEnableHls;
    bool _bEnableMp4;
	void onGetH264(const H264Frame &frame);
	void onGetAAC(const AACFrame &frame);
	void makeSDP();
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_ */
