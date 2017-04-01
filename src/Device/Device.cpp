/*
 * Device.cpp
 *
 *  Created on: 2016年8月10日
 *      Author: xzl
 */

#include "Device.h"
#include "Util/logger.h"
#include <stdio.h>
#include "Util/util.h"
#include <cstdio>
#include "base64.h"

#include "Util/TimeTicker.h"
using namespace ZL::Util;

namespace ZL {
namespace DEV {


/////////////////////////////////////////////////////////////////////////////////
#ifdef ENABLE_RTSP2RTMP
DevChannel::DevChannel(const char *strApp, const char *strId,float fDuration,bool bLiveStream ) :
		m_mediaSrc(new RtspToRtmpMediaSource(strApp,strId , bLiveStream)) {
#else
DevChannel::DevChannel(const char *strApp, const char *strId,float fDuration,bool bLiveStream ) :
		m_mediaSrc(new RtspToRtmpMediaSource(strApp,strId )) {
#endif //ENABLE_RTSP2RTMP
	m_strSDP = "v=0\r\n";
	m_strSDP += "o=- 1383190487994921 1 IN IP4 0.0.0.0\r\n";
	m_strSDP += "s=RTSP Session, streamed by the ZL\r\n";
	m_strSDP += "i=ZL Live Stream\r\n";
	m_strSDP += "c=IN IP4 0.0.0.0\r\n";
	m_strSDP += "t=0 0\r\n";
	//直播，时间长度永远
	if(fDuration <= 0 || bLiveStream){
		m_strSDP += "a=range:npt=0-\r\n";
	}else{
		m_strSDP += StrPrinter <<"a=range:npt=0-" << fDuration  << "\r\n" << endl;
	}
	m_strSDP += "a=control:*\r\n";
}
DevChannel::~DevChannel() {
}

void DevChannel::inputYUV(char* apcYuv[3], int aiYuvLen[3], uint32_t uiStamp) {
	//TimeTicker1(50);
#ifdef ENABLE_X264
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
#else
	ErrorL << "libx264 was not enabled!";
#endif //ENABLE_X264
}

void DevChannel::inputPCM(char* pcData, int iDataLen, uint32_t uiStamp) {
#ifdef ENABLE_FAAC
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
#else
	ErrorL << "libfaac was not enabled!";
#endif //ENABLE_FAAC
}

void DevChannel::inputH264(char* pcData, int iDataLen, uint32_t uiStamp) {
	if (!m_pRtpMaker_h264) {
		uint32_t ui32Ssrc;
		memcpy(&ui32Ssrc, makeRandStr(4, false).data(), 4);
		auto lam = [this](const RtpPacket::Ptr &pkt, bool bKeyPos) {
			m_mediaSrc->onGetRTP(pkt,bKeyPos);
		};
		static uint32_t videoMtu = mINI::Instance()[Config::Rtp::kVideoMtuSize].as<uint32_t>();
		m_pRtpMaker_h264.reset(new RtpMaker_H264(lam, ui32Ssrc,videoMtu));
	}
	if (!m_bSdp_gotH264 && m_video) {
		makeSDP_264((unsigned char*) pcData, iDataLen);
	}
	int iOffset = 4;
	if (memcmp("\x00\x00\x01", pcData, 3) == 0) {
		iOffset = 3;
	}
	m_pRtpMaker_h264->makeRtp(pcData + iOffset, iDataLen - iOffset, uiStamp);
}

void DevChannel::inputAAC(char* pcData, int iDataLen, uint32_t uiStamp) {
	if (!m_pRtpMaker_aac) {
		uint32_t ssrc;
		memcpy(&ssrc, makeRandStr(8, false).data() + 4, 4);
		auto lam = [this](const RtpPacket::Ptr &pkt, bool keyPos) {
			m_mediaSrc->onGetRTP(pkt,keyPos);
		};
		static uint32_t audioMtu = mINI::Instance()[Config::Rtp::kAudioMtuSize].as<uint32_t>();
		m_pRtpMaker_aac.reset(new RtpMaker_AAC(lam, ssrc, audioMtu,m_audio->iSampleRate));
	}
	if (!m_bSdp_gotAAC && m_audio) {
		makeSDP_AAC((unsigned char*) pcData, iDataLen);
	}
	m_pRtpMaker_aac->makeRtp(pcData + 7, iDataLen - 7, uiStamp);

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
	m_strSDP += StrPrinter << "m=video 0 RTP/AVP "
			<< m_pRtpMaker_h264->getPlayloadType() << "\r\n" << endl;
	m_strSDP += "b=AS:5100\r\n";
	m_strSDP += StrPrinter << "a=rtpmap:" << m_pRtpMaker_h264->getPlayloadType()
			<< " H264/" << m_pRtpMaker_h264->getSampleRate() << "\r\n" << endl;
	m_strSDP += StrPrinter << "a=fmtp:" << m_pRtpMaker_h264->getPlayloadType()
			<< " packetization-mode=1;profile-level-id=" << endl;

	memset(acTmp, 0, sizeof(acTmp));
	sprintf(acTmp, "%06X", profile_level_id);
	m_strSDP += acTmp;
	m_strSDP += ";sprop-parameter-sets=";
	memset(acTmp, 0, sizeof(acTmp));
	av_base64_encode(acTmp, sizeof(acTmp), (uint8_t *) m_aucSPS, m_uiSPSLen);
	//WarnL<<"SPS base64:"<<strTemp;
	//WarnL<<"SPS hexdump:"<<hexdump(SPS_BUF, SPS_LEN);
	m_strSDP += acTmp;
	m_strSDP += ",";
	memset(acTmp, 0, sizeof(acTmp));
	av_base64_encode(acTmp, sizeof(acTmp), (uint8_t *) m_aucPPS, m_uiPPSLen);
	m_strSDP += acTmp;
	m_strSDP += "\r\n";
	if (m_video->iFrameRate > 0 && m_video->iHeight > 0 && m_video->iWidth > 0) {
		m_strSDP += "a=framerate:";
		m_strSDP += StrPrinter << m_video->iFrameRate << endl;
		m_strSDP += StrPrinter << "\r\na=framesize:"
				<< m_pRtpMaker_h264->getPlayloadType() << " " << endl;
		m_strSDP += StrPrinter << m_video->iWidth << endl;
		m_strSDP += "-";
		m_strSDP += StrPrinter << m_video->iHeight << endl;
		m_strSDP += "\r\n";
	}
	m_strSDP += StrPrinter << "a=control:trackID="
			<< m_pRtpMaker_h264->getInterleaved() / 2 << "\r\n" << endl;
	m_bSdp_gotH264 = true;
	if (m_audio) {
		if (m_bSdp_gotAAC) {
			makeSDP(m_strSDP);
		}
	} else {
		makeSDP(m_strSDP);
	}
}

inline void DevChannel::makeSDP_AAC(unsigned char *fixedHeader, int dataLen) {
	auto audioSpecificConfig = makeAdtsConfig(fixedHeader);
	if (audioSpecificConfig.size() != 2) {
		return;
	}

	char fConfigStr[5] = { 0 };
	sprintf(fConfigStr, "%02X%02x", (uint8_t)audioSpecificConfig[0],(uint8_t)audioSpecificConfig[1]);

	m_strSDP += StrPrinter << "m=audio 0 RTP/AVP "
			<< m_pRtpMaker_aac->getPlayloadType() << "\r\n" << endl;
	m_strSDP += "b=AS:96\r\n";
	m_strSDP += StrPrinter << "a=rtpmap:" << m_pRtpMaker_aac->getPlayloadType()
			<< " MPEG4-GENERIC/" << m_pRtpMaker_aac->getSampleRate() << "\r\n"
			<< endl;
	m_strSDP += StrPrinter << "a=fmtp:" << m_pRtpMaker_aac->getPlayloadType()
				<< " streamtype=5;profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config="
				<< endl;
	m_strSDP += fConfigStr;
	m_strSDP += "\r\n";
	m_strSDP += StrPrinter << "a=control:trackID="
			<< m_pRtpMaker_aac->getInterleaved() / 2 << "\r\n" << endl;

	m_bSdp_gotAAC = true;
	if (m_video) {
		if (m_bSdp_gotH264) {
			makeSDP(m_strSDP);
		}
	} else {
		makeSDP(m_strSDP);
	}
}

void DevChannel::makeSDP(const string& strSdp) {
	m_mediaSrc->onGetSDP(strSdp);
	m_mediaSrc->regist();
}

void DevChannel::initVideo(const VideoInfo& info) {
	m_video.reset(new VideoInfo(info));
}

void DevChannel::initAudio(const AudioInfo& info) {
	m_audio.reset(new AudioInfo(info));
}
} /* namespace DEV */
} /* namespace ZL */

