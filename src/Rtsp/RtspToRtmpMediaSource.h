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

#include "RtspMediaSource.h"
#include "Rtmp/amf.h"
#include "Rtmp/RtmpMediaSource.h"
#include "RtspMuxer/RtspDemuxer.h"
#include "MediaFile/MediaRecorder.h"
using namespace toolkit;

namespace mediakit {

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
		RtspMediaSource::onGetSDP(strSdp);
		try {
			_pParser.reset(new RtspDemuxer(_sdpAttr));
            _pRecorder.reset(new MediaRecorder(getVhost(),getApp(),getId(),_pParser,_bEnableHls,_bEnableMp4));
			//todo(xzl) 修复此处
//			_pParser->setOnAudioCB( std::bind(&RtspToRtmpMediaSource::onGetAAC, this, placeholders::_1));
//			_pParser->setOnVideoCB( std::bind(&RtspToRtmpMediaSource::onGetH264, this, placeholders::_1));
			makeMetaData();
		} catch (exception &ex) {
			WarnL << ex.what();
		}
	}
	virtual void onWrite(const RtpPacket::Ptr &pRtppkt, bool bKeyPos) override{
		if (_pParser) {
			bKeyPos = _pParser->inputRtp(pRtppkt);
		}
		RtspMediaSource::onWrite(pRtppkt, bKeyPos);
	}

	int readerCount(){
		return getRing()->readerCount() + (_pRtmpSrc ? _pRtmpSrc->getRing()->readerCount() : 0);
	}

protected:
	void onGetH264(const H264Frame &frame);
	void onGetAAC(const AACFrame &frame);
private:
	void makeVideoConfigPkt();
	void makeAudioConfigPkt();
	void makeMetaData();
private:
	RtspDemuxer::Ptr _pParser;
	RtmpMediaSource::Ptr _pRtmpSrc;
	uint8_t _ui8AudioFlags = 0;
	MediaRecorder::Ptr _pRecorder;
	bool _bEnableHls;
    bool _bEnableMp4;

};

} /* namespace mediakit */

#endif /* SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_ */
