/*
 * RtmpToRtspMediaSource.h
 *
 *  Created on: 2016年10月20日
 *      Author: xzl
 */

#ifndef SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_
#define SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <mutex>
#include "Util/logger.h"
#include "amf.h"
#include "Rtmp.h"
#include "Util/util.h"
#include <mutex>
#include "RtmpMediaSource.h"
#include "Rtsp/RtspMediaSource.h"
#include "RTP/RtpMakerH264.h"
#include "RTP/RtpMakerAAC.h"
#include "MedaiFile/MediaRecorder.h"
#include "Rtsp/RtpParser.h"
#include "RtmpParser.h"

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
