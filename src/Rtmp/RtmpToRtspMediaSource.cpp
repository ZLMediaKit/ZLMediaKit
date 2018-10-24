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

#include "Common/config.h"
#include "RtmpToRtspMediaSource.h"
#include "Util/util.h"
#include "Util/base64.h"
#include "Network/sockutil.h"
using namespace toolkit;

namespace mediakit {

RtmpToRtspMediaSource::RtmpToRtspMediaSource(const string &vhost,
                                             const string &app,
                                             const string &id,
                                             bool bEnableHls,
                                             bool bEnableMp4) :
		RtmpMediaSource(vhost,app,id),_bEnableHls(bEnableHls),_bEnableMp4(bEnableMp4) {
}
RtmpToRtspMediaSource::~RtmpToRtspMediaSource() {}


void RtmpToRtspMediaSource::onGetH264(const H264Frame &frame) {
    if(_pRecorder){
        _pRecorder->inputH264((char *) frame.data(), frame.size(), frame.timeStamp, frame.type);
    }

	if(_pRtpMaker_h264){
		_pRtpMaker_h264->makeRtp(frame.data() + 4, frame.size() - 4, frame.timeStamp);
	}
}
inline void RtmpToRtspMediaSource::onGetAAC(const AACFrame &frame) {
	if(_pRecorder){
        _pRecorder->inputAAC((char *) frame.buffer, frame.aac_frame_length, frame.timeStamp);
    }

	if (_pRtpMaker_aac) {
		_pRtpMaker_aac->makeRtp((char *) frame.buffer + 7, frame.aac_frame_length - 7, frame.timeStamp);
	}
}

void RtmpToRtspMediaSource::makeSDP() {
	string strSDP;
	strSDP = "v=0\r\n";
	strSDP += "o=- 1383190487994921 1 IN IP4 0.0.0.0\r\n";
	strSDP += "s=RTSP Session, streamed by the ZL\r\n";
	strSDP += "i=ZL Live Stream\r\n";
	strSDP += "c=IN IP4 0.0.0.0\r\n";
	strSDP += "t=0 0\r\n";
	if(_pParser->getDuration() <= 0){
		strSDP += "a=range:npt=0-\r\n";
	}else{
		strSDP += StrPrinter << "0-"<<  _pParser->getDuration()<< "\r\n" << endl;
	}
	strSDP += "a=control:*\r\n";

	//todo(xzl) 修复此处

//	if (_pParser->containVideo()) {
//		uint32_t ssrc0;
//		memcpy(&ssrc0, makeRandStr(4, false).data(), 4);
//		auto lam = [this](const RtpPacket::Ptr &pkt, bool bKeyPos) {
//			_pRtspSrc->onGetRTP(pkt,bKeyPos);
//		};
//
//        GET_CONFIG_AND_REGISTER(uint32_t,videoMtu,Rtp::kVideoMtuSize);
//        _pRtpMaker_h264.reset(new RtpMaker_H264(lam, ssrc0,videoMtu));
//
//		char strTemp[100];
//		int profile_level_id = 0;
//		string strSPS =_pParser->getSps().substr(4);
//		string strPPS =_pParser->getPps().substr(4);
//		if (strSPS.length() >= 4) { // sanity check
//			profile_level_id = (strSPS[1] << 16) | (strSPS[2] << 8) | strSPS[3]; // profile_idc|constraint_setN_flag|level_idc
//		}
//
//		//视频通道
//		strSDP += StrPrinter << "m=video 0 RTP/AVP " << _pRtpMaker_h264->getPlayloadType()
//				<< "\r\n" << endl;
//		strSDP += "b=AS:5100\r\n";
//		strSDP += StrPrinter << "a=rtpmap:" << _pRtpMaker_h264->getPlayloadType()
//				<< " H264/" << _pRtpMaker_h264->getSampleRate() << "\r\n" << endl;
//		strSDP += StrPrinter << "a=fmtp:" << _pRtpMaker_h264->getPlayloadType()
//				<< " packetization-mode=1;profile-level-id=" << endl;
//
//		memset(strTemp, 0, 100);
//		sprintf(strTemp, "%06X", profile_level_id);
//		strSDP += strTemp;
//		strSDP += ";sprop-parameter-sets=";
//		memset(strTemp, 0, 100);
//		av_base64_encode(strTemp, 100, (uint8_t *) strSPS.data(), strSPS.size());
//		strSDP += strTemp;
//		strSDP += ",";
//		memset(strTemp, 0, 100);
//		av_base64_encode(strTemp, 100, (uint8_t *) strPPS.data(), strPPS.size());
//		strSDP += strTemp;
//		strSDP += "\r\n";
//		strSDP += StrPrinter << "a=control:trackID=" << _pRtpMaker_h264->getInterleaved() / 2
//				<< "\r\n" << endl;
//    }
//
//	if (_pParser->containAudio()) {
//		uint32_t ssrc1;
//		memcpy(&ssrc1, makeRandStr(8, false).data() + 4, 4);
//		auto lam = [this](const RtpPacket::Ptr &pkt, bool bKeyPos) {
//			_pRtspSrc->onGetRTP(pkt,bKeyPos);
//		};
//        GET_CONFIG_AND_REGISTER(uint32_t,audioMtu,Rtp::kAudioMtuSize);
//        _pRtpMaker_aac.reset(new RtpMaker_AAC(lam, ssrc1, audioMtu,_pParser->getAudioSampleRate()));
//
//		char configStr[32];
//		const string & strAacCfg = _pParser->getAudioCfg();
//		snprintf(configStr, sizeof(configStr), "%02X%02x", strAacCfg[0], strAacCfg[1]);
//		strSDP += StrPrinter << "m=audio 0 RTP/AVP " << _pRtpMaker_aac->getPlayloadType()
//				<< "\r\n" << endl;
//		strSDP += "b=AS:96\r\n";
//		strSDP += StrPrinter << "a=rtpmap:" << _pRtpMaker_aac->getPlayloadType()
//				<< " MPEG4-GENERIC/" << _pRtpMaker_aac->getSampleRate() << "\r\n"
//				<< endl;
//		strSDP += StrPrinter << "a=fmtp:" << _pRtpMaker_aac->getPlayloadType()
//				 << " streamtype=5;profile-level-id=1;mode=AAC-hbr;"
//				 << "sizelength=13;indexlength=3;indexdeltalength=3;config="
//				 << endl;
//		strSDP.append(configStr, 4);
//		strSDP += "\r\n";
//		strSDP += StrPrinter << "a=control:trackID=" << _pRtpMaker_aac->getInterleaved() / 2
//				<< "\r\n" << endl;
//	}

	_pRtspSrc.reset(new RtspMediaSource(getVhost(),getApp(),getId()));
	_pRtspSrc->setListener(_listener);
	_pRtspSrc->onGetSDP(strSDP);
}


} /* namespace mediakit */
