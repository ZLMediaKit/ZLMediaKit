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
class RtspToRtmpMediaSource: public RtspMediaSource {
public:
	typedef std::shared_ptr<RtspToRtmpMediaSource> Ptr;

	RtspToRtmpMediaSource(const string &vhost,
                          const string &app,
                          const string &id,
                          bool bEnableHls = true,
                          bool bEnableMp4 = true);

	virtual ~RtspToRtmpMediaSource();

	virtual void onGetSDP(const string& strSdp) override{
		try {
			m_pParser.reset(new RtpParser(strSdp));
            m_pRecorder.reset(new MediaRecorder(getVhost(),getApp(),getId(),m_pParser,m_bEnableHls,m_bEnableMp4));
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
	virtual bool regist() override ;
	virtual bool unregist() override;

	int readerCount(){
		return getRing()->readerCount() + (m_pRtmpSrc ? m_pRtmpSrc->getRing()->readerCount() : 0);
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
	uint8_t m_ui8AudioFlags = 0;
	MediaRecorder::Ptr m_pRecorder;
	bool m_bEnableHls;
    bool m_bEnableMp4;
    void onGetH264(const H264Frame &frame);
	void onGetAdts(const AdtsFrame &frame);
	void makeVideoConfigPkt();
	void makeAudioConfigPkt();
	void makeMetaData();
};

} /* namespace Rtsp */
} /* namespace ZL */

#endif /* SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_ */
