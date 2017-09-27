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
#include "RtmpParser.h"
#include "RtmpMediaSource.h"
#include "RTP/RtpMakerH264.h"
#include "RTP/RtpMakerAAC.h"
#include "Rtsp/RtpParser.h"
#include "Rtsp/RtspMediaSource.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "MediaFile/MediaRecorder.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Rtsp;
using namespace ZL::MediaFile;

namespace ZL {
namespace Rtmp {

#ifdef ENABLE_RTMP2RTSP
class RtmpToRtspMediaSource: public RtmpMediaSource {
public:
	typedef std::shared_ptr<RtmpToRtspMediaSource> Ptr;
	RtmpToRtspMediaSource(const string &_app, const string &_id);
	virtual ~RtmpToRtspMediaSource();
	virtual void regist() override;
	virtual void unregist() override;

	virtual void onGetMetaData(const AMFValue &_metadata) override {
		try {
			m_pParser.reset(new RtmpParser(_metadata));
			m_pRecorder.reset(new MediaRecorder(getApp(),getId(),m_pParser));
			m_pParser->setOnAudioCB(std::bind(&RtmpToRtspMediaSource::onGetAdts, this, placeholders::_1));
			m_pParser->setOnVideoCB(std::bind(&RtmpToRtspMediaSource::onGetH264, this, placeholders::_1));
		} catch (exception &ex) {
			WarnL << ex.what();
		}
		RtmpMediaSource::onGetMetaData(_metadata);
	}

	virtual void onGetMedia(const RtmpPacket &pkt) override {
		if (m_pParser) {
			if (!m_pRtspSrc && m_pParser->isInited()) {
				makeSDP();
			}
			m_pParser->inputRtmp(pkt);
		}
		RtmpMediaSource::onGetMedia(pkt);
	}
	void setOnSeek(const function<bool(uint32_t)> &cb) override {
		RtmpMediaSource::setOnSeek(cb);
		if (m_pRtspSrc) {
			m_pRtspSrc->setOnSeek(cb);
		}
	}
	void setOnStamp(const function<uint32_t()> &cb)  override{
		RtmpMediaSource::setOnStamp(cb);
		if (m_pRtspSrc) {
			m_pRtspSrc->setOnStamp(cb);
		}
	}
private:
	RtmpParser::Ptr m_pParser;
	RtspMediaSource::Ptr m_pRtspSrc;
	RtpMaker_AAC::Ptr m_pRtpMaker_aac;
	RtpMaker_H264::Ptr m_pRtpMaker_h264;
	MediaRecorder::Ptr m_pRecorder;

	void onGetH264(const H264Frame &frame);
	void onGetAdts(const AdtsFrame &frame);
	void makeSDP();
};
#else
	typedef RtmpMediaSource RtmpToRtspMediaSource;
#endif //ENABLE_RTMP2RTSP

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_ */
