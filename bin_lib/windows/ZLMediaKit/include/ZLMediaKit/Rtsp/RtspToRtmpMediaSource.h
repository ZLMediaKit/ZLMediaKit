/*
 * RtspToRtmpMediaSource.h
 *
 *  Created on: 2016年9月7日
 *      Author: xzl
 */

#ifndef SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_
#define SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_

#include "RtpParser.h"
#include "RtspMediaSource.h"
#include "Rtmp/amf.h"
#include "Rtmp/RtmpMediaSource.h"
#include "MediaFile/MediaRecorder.h"

using namespace ZL::Rtmp;
using namespace ZL::MediaFile;

namespace ZL {
namespace Rtsp {
#ifdef ENABLE_RTSP2RTMP
class RtspToRtmpMediaSource: public RtspMediaSource {
public:
	typedef std::shared_ptr<RtspToRtmpMediaSource> Ptr;
	RtspToRtmpMediaSource(const string &_app,const string &_id,bool bEnableFile = true);
	virtual ~RtspToRtmpMediaSource();

	virtual void onGetSDP(const string& strSdp) override{
		try {
			m_pParser.reset(new RtpParser(strSdp));
			if(m_bEnableFile){
				m_pRecorder.reset(new MediaRecorder(getApp(),getId(),m_pParser));
			}
			m_pParser->setOnAudioCB( std::bind(&RtspToRtmpMediaSource::onGetAdts, this, placeholders::_1));
			m_pParser->setOnVideoCB( std::bind(&RtspToRtmpMediaSource::onGetH264, this, placeholders::_1));
			makeMetaData();
		} catch (exception &ex) {
			WarnL << ex.what();
		}
		RtspMediaSource::onGetSDP(strSdp);
	}
	virtual void onGetRTP(const RtpPacket::Ptr &pRtppkt, bool bKeyPos) override{
		if (m_pParser) {
			bKeyPos = m_pParser->inputRtp(*pRtppkt);
		}
		RtspMediaSource::onGetRTP(pRtppkt, bKeyPos);
	}
	virtual void regist() override ;
	virtual void unregist() override;
	int readerCount(){
		return getRing()->readerCount() + (m_pRtmpSrc ? m_pRtmpSrc->getRing()->readerCount() : 0);
	}
	void setOnSeek(const function<bool(uint32_t)> &cb) override{
		RtspMediaSource::setOnSeek(cb);
		if(m_pRtmpSrc){
			m_pRtmpSrc->setOnSeek(cb);
		}
	}
	void setOnStamp(const function<uint32_t()> &cb) override{
		RtspMediaSource::setOnStamp(cb);
		if (m_pRtmpSrc) {
			m_pRtmpSrc->setOnStamp(cb);
		}
	}
	void updateTimeStamp(uint32_t uiStamp) {
		for (auto &pr : m_mapTracks) {
			switch (pr.second.type) {
			case TrackAudio: {
				pr.second.timeStamp = uiStamp * (m_pParser->getAudioSampleRate() / 1000.0);
			}
				break;
			case TrackVideo: {
				pr.second.timeStamp = uiStamp * 90;
			}
				break;
			default:
				break;
			}
		}
	}
private:
	RtpParser::Ptr m_pParser;
	RtmpMediaSource::Ptr m_pRtmpSrc;
	RtmpPacket m_rtmpPkt;
	uint8_t m_ui8AudioFlags = 0;
	MediaRecorder::Ptr m_pRecorder;
	bool m_bEnableFile = true;
	void onGetH264(const H264Frame &frame);
	void onGetAdts(const AdtsFrame &frame);
	void makeVideoConfigPkt();
	void makeAudioConfigPkt();
	void makeMetaData();
};

#else
typedef RtspMediaSource RtspToRtmpMediaSource;
#endif //ENABLE_RTSP2RTMP
} /* namespace Rtsp */
} /* namespace ZL */

#endif /* SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_ */
