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
#include <stdio.h>
#include <stdio.h>
#include "base64.h"
#include "Device.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"

using namespace ZL::Util;

namespace ZL {
namespace DEV {

DevChannel::DevChannel(const char *strVhost,
                       const char *strApp,
                       const char *strId,
                       float fDuration,
                       bool bEanbleHls,
                       bool bEnableMp4 ) :
        RtspToRtmpMediaSource(strVhost,strApp,strId,bEanbleHls,bEnableMp4) {

	m_strSdp = "v=0\r\n";
	m_strSdp += "o=- 1383190487994921 1 IN IP4 0.0.0.0\r\n";
	m_strSdp += "s=RTSP Session, streamed by the ZL\r\n";
	m_strSdp += "i=ZL Live Stream\r\n";
	m_strSdp += "c=IN IP4 0.0.0.0\r\n";
	m_strSdp += "t=0 0\r\n";
	//直播，时间长度永远
	if(fDuration <= 0){
		m_strSdp += "a=range:npt=0-\r\n";
	}else{
		m_strSdp += StrPrinter <<"a=range:npt=0-" << fDuration  << "\r\n" << endl;
	}
	m_strSdp += "a=control:*\r\n";
}
DevChannel::~DevChannel() {
}

#ifdef ENABLE_X264
void DevChannel::inputYUV(char* apcYuv[3], int aiYuvLen[3], uint32_t uiStamp) {
	//TimeTicker1(50);
	if (!m_pH264Enc) {
		m_pH264Enc.reset(new H264Encoder());
		if (!m_pH264Enc->init(m_video->iWidth, m_video->iHeight, m_video->iFrameRate)) {
			m_pH264Enc.reset();
			WarnL << "H264Encoder init failed!";
		}
	}
	if (m_pH264Enc) {
		H264Encoder::H264Frame *pOut;
		int iFrames = m_pH264Enc->inputData(apcYuv, aiYuvLen, uiStamp, &pOut);
		for (int i = 0; i < iFrames; i++) {
			inputH264((char *) pOut[i].pucData, pOut[i].iLength, uiStamp);
		}
	}
}
#endif //ENABLE_X264

#ifdef ENABLE_FAAC
void DevChannel::inputPCM(char* pcData, int iDataLen, uint32_t uiStamp) {
	if (!m_pAacEnc) {
		m_pAacEnc.reset(new AACEncoder());
		if (!m_pAacEnc->init(m_audio->iSampleRate, m_audio->iChannel, m_audio->iSampleBit)) {
			m_pAacEnc.reset();
			WarnL << "AACEncoder init failed!";
		}
	}
	if (m_pAacEnc) {
		unsigned char *pucOut;
		int iRet = m_pAacEnc->inputData(pcData, iDataLen, &pucOut);
		if (iRet > 0) {
			inputAAC((char *) pucOut, iRet, uiStamp);
		}
	}
}
#endif //ENABLE_FAAC

void DevChannel::inputH264(char* pcData, int iDataLen, uint32_t uiStamp) {
	if (!m_pRtpMaker_h264) {
		uint32_t ui32Ssrc;
		memcpy(&ui32Ssrc, makeRandStr(4, false).data(), 4);
		auto lam = [this](const RtpPacket::Ptr &pkt, bool bKeyPos) {
			onGetRTP(pkt,bKeyPos);
		};
        GET_CONFIG_AND_REGISTER(uint32_t,videoMtu,Config::Rtp::kVideoMtuSize);
		m_pRtpMaker_h264.reset(new RtpMaker_H264(lam, ui32Ssrc,videoMtu));
	}
	if (!m_bSdp_gotH264 && m_video) {
		makeSDP_264((unsigned char*) pcData, iDataLen);
	}
	int iOffset = 4;
	if (memcmp("\x00\x00\x01", pcData, 3) == 0) {
		iOffset = 3;
	}
    if(uiStamp == 0){
        uiStamp = (uint32_t)m_aTicker[0].elapsedTime();
    }
	m_pRtpMaker_h264->makeRtp(pcData + iOffset, iDataLen - iOffset, uiStamp);
}

void DevChannel::inputAAC(char* pcData, int iDataLen, uint32_t uiStamp) {
	inputAAC(pcData+7,iDataLen-7,uiStamp,pcData);
}
void DevChannel::inputAAC(char *pcDataWithoutAdts,int iDataLen, uint32_t uiStamp,char *pcAdtsHeader){
	if (!m_pRtpMaker_aac) {
		uint32_t ssrc;
		memcpy(&ssrc, makeRandStr(8, false).data() + 4, 4);
		auto lam = [this](const RtpPacket::Ptr &pkt, bool keyPos) {
			onGetRTP(pkt,keyPos);
		};
        GET_CONFIG_AND_REGISTER(uint32_t,audioMtu,Config::Rtp::kAudioMtuSize);
        m_pRtpMaker_aac.reset(new RtpMaker_AAC(lam, ssrc, audioMtu,m_audio->iSampleRate));
	}
	if (!m_bSdp_gotAAC && m_audio && pcAdtsHeader) {
		makeSDP_AAC((unsigned char*) pcAdtsHeader);
	}
    if(uiStamp == 0){
        uiStamp = (uint32_t)m_aTicker[1].elapsedTime();
    }
    if(pcDataWithoutAdts && iDataLen){
        m_pRtpMaker_aac->makeRtp(pcDataWithoutAdts, iDataLen, uiStamp);
    }
}

inline void DevChannel::makeSDP_264(unsigned char *pcData, int iDataLen) {
	int offset = 4;
	if (memcmp("\x00\x00\x01", pcData, 3) == 0) {
		offset = 3;
	}
	switch (pcData[offset] & 0x1F) {
	case 7:/*SPS frame*/
	{
		if (m_uiSPSLen != 0) {
			break;
		}
		memcpy(m_aucSPS, pcData + offset, iDataLen - offset);
		m_uiSPSLen = iDataLen - offset;
	}
		break;
	case 8:/*PPS frame*/
	{
		if (m_uiPPSLen != 0) {
			break;
		}
		memcpy(m_aucPPS, pcData + offset, iDataLen - offset);
		m_uiPPSLen = iDataLen - offset;
	}
		break;
	default:
		break;
	}
	if (!m_uiSPSLen || !m_uiPPSLen) {
		return;
	}

	char acTmp[256];
	int profile_level_id = 0;
	if (m_uiSPSLen >= 4) { // sanity check
		profile_level_id = (m_aucSPS[1] << 16) | (m_aucSPS[2] << 8) | m_aucSPS[3]; // profile_idc|constraint_setN_flag|level_idc
	}

	//视频通道
	m_strSdp += StrPrinter << "m=video 0 RTP/AVP "
			<< m_pRtpMaker_h264->getPlayloadType() << "\r\n" << endl;
	m_strSdp += "b=AS:5100\r\n";
	m_strSdp += StrPrinter << "a=rtpmap:" << m_pRtpMaker_h264->getPlayloadType()
			<< " H264/" << m_pRtpMaker_h264->getSampleRate() << "\r\n" << endl;
	m_strSdp += StrPrinter << "a=fmtp:" << m_pRtpMaker_h264->getPlayloadType()
			<< " packetization-mode=1;profile-level-id=" << endl;

	memset(acTmp, 0, sizeof(acTmp));
	sprintf(acTmp, "%06X", profile_level_id);
	m_strSdp += acTmp;
	m_strSdp += ";sprop-parameter-sets=";
	memset(acTmp, 0, sizeof(acTmp));
	av_base64_encode(acTmp, sizeof(acTmp), (uint8_t *) m_aucSPS, m_uiSPSLen);
	//WarnL<<"SPS base64:"<<strTemp;
	//WarnL<<"SPS hexdump:"<<hexdump(SPS_BUF, SPS_LEN);
	m_strSdp += acTmp;
	m_strSdp += ",";
	memset(acTmp, 0, sizeof(acTmp));
	av_base64_encode(acTmp, sizeof(acTmp), (uint8_t *) m_aucPPS, m_uiPPSLen);
	m_strSdp += acTmp;
	m_strSdp += "\r\n";
	if (m_video->iFrameRate > 0 && m_video->iHeight > 0 && m_video->iWidth > 0) {
		m_strSdp += "a=framerate:";
		m_strSdp += StrPrinter << m_video->iFrameRate << endl;
		m_strSdp += StrPrinter << "\r\na=framesize:"
				<< m_pRtpMaker_h264->getPlayloadType() << " " << endl;
		m_strSdp += StrPrinter << m_video->iWidth << endl;
		m_strSdp += "-";
		m_strSdp += StrPrinter << m_video->iHeight << endl;
		m_strSdp += "\r\n";
	}
	m_strSdp += StrPrinter << "a=control:trackID="
			<< m_pRtpMaker_h264->getInterleaved() / 2 << "\r\n" << endl;
	m_bSdp_gotH264 = true;
	if (m_audio) {
		if (m_bSdp_gotAAC) {
			makeSDP(m_strSdp);
		}
	} else {
		makeSDP(m_strSdp);
	}
}

inline void DevChannel::makeSDP_AAC(unsigned char *fixedHeader) {
	auto audioSpecificConfig = makeAdtsConfig(fixedHeader);
	if (audioSpecificConfig.size() != 2) {
		return;
	}

	char fConfigStr[5] = { 0 };
	sprintf(fConfigStr, "%02X%02x", (uint8_t)audioSpecificConfig[0],(uint8_t)audioSpecificConfig[1]);

	m_strSdp += StrPrinter << "m=audio 0 RTP/AVP "
			<< m_pRtpMaker_aac->getPlayloadType() << "\r\n" << endl;
	m_strSdp += "b=AS:96\r\n";
	m_strSdp += StrPrinter << "a=rtpmap:" << m_pRtpMaker_aac->getPlayloadType()
			<< " MPEG4-GENERIC/" << m_pRtpMaker_aac->getSampleRate() << "\r\n"
			<< endl;
	m_strSdp += StrPrinter << "a=fmtp:" << m_pRtpMaker_aac->getPlayloadType()
				<< " streamtype=5;profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config="
				<< endl;
	m_strSdp += fConfigStr;
	m_strSdp += "\r\n";
	m_strSdp += StrPrinter << "a=control:trackID="
			<< m_pRtpMaker_aac->getInterleaved() / 2 << "\r\n" << endl;

	m_bSdp_gotAAC = true;
	if (m_video) {
		if (m_bSdp_gotH264) {
			makeSDP(m_strSdp);
		}
	} else {
		makeSDP(m_strSdp);
	}
}

void DevChannel::makeSDP(const string& strSdp) {
	onGetSDP(strSdp);
	regist();
}

void DevChannel::initVideo(const VideoInfo& info) {
	m_video.reset(new VideoInfo(info));
}

void DevChannel::initAudio(const AudioInfo& info) {
	m_audio.reset(new AudioInfo(info));
}
} /* namespace DEV */
} /* namespace ZL */

