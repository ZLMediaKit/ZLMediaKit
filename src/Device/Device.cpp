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
#include "Device.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/base64.h"
#include "Util/TimeTicker.h"

using namespace toolkit;

namespace mediakit {

DevChannel::DevChannel(const char *strVhost,
                       const char *strApp,
                       const char *strId,
                       float fDuration,
                       bool bEanbleHls,
                       bool bEnableMp4 ) :
        RtspToRtmpMediaSource(strVhost,strApp,strId,bEanbleHls,bEnableMp4) {

	_strSdp = "v=0\r\n";
	_strSdp += "o=- 1383190487994921 1 IN IP4 0.0.0.0\r\n";
	_strSdp += "s=RTSP Session, streamed by the ZL\r\n";
	_strSdp += "i=ZL Live Stream\r\n";
	_strSdp += "c=IN IP4 0.0.0.0\r\n";
	_strSdp += "t=0 0\r\n";
	//直播，时间长度永远
	if(fDuration <= 0){
		_strSdp += "a=range:npt=0-\r\n";
	}else{
		_strSdp += StrPrinter <<"a=range:npt=0-" << fDuration  << "\r\n" << endl;
	}
	_strSdp += "a=control:*\r\n";
}
DevChannel::~DevChannel() {
}

#ifdef ENABLE_X264
void DevChannel::inputYUV(char* apcYuv[3], int aiYuvLen[3], uint32_t uiStamp) {
	//TimeTicker1(50);
	if (!_pH264Enc) {
		_pH264Enc.reset(new H264Encoder());
		if (!_pH264Enc->init(_video->iWidth, _video->iHeight, _video->iFrameRate)) {
			_pH264Enc.reset();
			WarnL << "H264Encoder init failed!";
		}
	}
	if (_pH264Enc) {
		H264Encoder::H264Frame *pOut;
		int iFrames = _pH264Enc->inputData(apcYuv, aiYuvLen, uiStamp, &pOut);
		for (int i = 0; i < iFrames; i++) {
			inputH264((char *) pOut[i].pucData, pOut[i].iLength, uiStamp);
		}
	}
}
#endif //ENABLE_X264

#ifdef ENABLE_FAAC
void DevChannel::inputPCM(char* pcData, int iDataLen, uint32_t uiStamp) {
	if (!_pAacEnc) {
		_pAacEnc.reset(new AACEncoder());
		if (!_pAacEnc->init(_audio->iSampleRate, _audio->iChannel, _audio->iSampleBit)) {
			_pAacEnc.reset();
			WarnL << "AACEncoder init failed!";
		}
	}
	if (_pAacEnc) {
		unsigned char *pucOut;
		int iRet = _pAacEnc->inputData(pcData, iDataLen, &pucOut);
		if (iRet > 0) {
			inputAAC((char *) pucOut, iRet, uiStamp);
		}
	}
}
#endif //ENABLE_FAAC

void DevChannel::inputH264(const char* pcData, int iDataLen, uint32_t uiStamp) {
	if (!_pRtpMaker_h264) {
		uint32_t ui32Ssrc;
		memcpy(&ui32Ssrc, makeRandStr(4, false).data(), 4);
		auto lam = [this](const RtpPacket::Ptr &pkt, bool bKeyPos) {
			onGetRTP(pkt,bKeyPos);
		};
        GET_CONFIG_AND_REGISTER(uint32_t,videoMtu,Rtp::kVideoMtuSize);
		_pRtpMaker_h264.reset(new RtpMaker_H264(lam, ui32Ssrc,videoMtu));
	}
	if (!_bSdp_gotH264 && _video) {
		makeSDP_264((unsigned char*) pcData, iDataLen);
	}
	int iOffset = 4;
	if (memcmp("\x00\x00\x01", pcData, 3) == 0) {
		iOffset = 3;
	}
    if(uiStamp == 0){
        uiStamp = (uint32_t)_aTicker[0].elapsedTime();
    }
	_pRtpMaker_h264->makeRtp(pcData + iOffset, iDataLen - iOffset, uiStamp);
}

void DevChannel::inputAAC(const char* pcData, int iDataLen, uint32_t uiStamp,bool withAdtsHeader) {
	if(withAdtsHeader){
		inputAAC(pcData+7,iDataLen-7,uiStamp,pcData);
	} else if(_pAdtsHeader){
		_pAdtsHeader->aac_frame_length = iDataLen;
		writeAdtsHeader(*_pAdtsHeader,(uint8_t *)_pAdtsHeader->buffer);
		inputAAC(pcData,iDataLen,uiStamp,(const char *)_pAdtsHeader->buffer);
	}
}
void DevChannel::inputAAC(const char *pcDataWithoutAdts,int iDataLen, uint32_t uiStamp,const char *pcAdtsHeader){
	if (!_pRtpMaker_aac) {
		uint32_t ssrc;
		memcpy(&ssrc, makeRandStr(8, false).data() + 4, 4);
		auto lam = [this](const RtpPacket::Ptr &pkt, bool keyPos) {
			onGetRTP(pkt,keyPos);
		};
        GET_CONFIG_AND_REGISTER(uint32_t,audioMtu,Rtp::kAudioMtuSize);
        _pRtpMaker_aac.reset(new RtpMaker_AAC(lam, ssrc, audioMtu,_audio->iSampleRate));
	}
	if (!_bSdp_gotAAC && _audio && pcAdtsHeader) {
		makeSDP_AAC((unsigned char*) pcAdtsHeader);
	}
    if(uiStamp == 0){
        uiStamp = (uint32_t)_aTicker[1].elapsedTime();
    }
    if(pcDataWithoutAdts && iDataLen){
        _pRtpMaker_aac->makeRtp(pcDataWithoutAdts, iDataLen, uiStamp);
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
		if (_uiSPSLen != 0) {
			break;
		}
		memcpy(_aucSPS, pcData + offset, iDataLen - offset);
		_uiSPSLen = iDataLen - offset;
	}
		break;
	case 8:/*PPS frame*/
	{
		if (_uiPPSLen != 0) {
			break;
		}
		memcpy(_aucPPS, pcData + offset, iDataLen - offset);
		_uiPPSLen = iDataLen - offset;
	}
		break;
	default:
		break;
	}
	if (!_uiSPSLen || !_uiPPSLen) {
		return;
	}

	char acTmp[256];
	int profile_level_id = 0;
	if (_uiSPSLen >= 4) { // sanity check
		profile_level_id = (_aucSPS[1] << 16) | (_aucSPS[2] << 8) | _aucSPS[3]; // profile_idc|constraint_setN_flag|level_idc
	}

	//视频通道
	_strSdp += StrPrinter << "m=video 0 RTP/AVP "
			<< _pRtpMaker_h264->getPlayloadType() << "\r\n" << endl;
	_strSdp += "b=AS:5100\r\n";
	_strSdp += StrPrinter << "a=rtpmap:" << _pRtpMaker_h264->getPlayloadType()
			<< " H264/" << _pRtpMaker_h264->getSampleRate() << "\r\n" << endl;
	_strSdp += StrPrinter << "a=fmtp:" << _pRtpMaker_h264->getPlayloadType()
			<< " packetization-mode=1;profile-level-id=" << endl;

	memset(acTmp, 0, sizeof(acTmp));
	sprintf(acTmp, "%06X", profile_level_id);
	_strSdp += acTmp;
	_strSdp += ";sprop-parameter-sets=";
	memset(acTmp, 0, sizeof(acTmp));
	av_base64_encode(acTmp, sizeof(acTmp), (uint8_t *) _aucSPS, _uiSPSLen);
	//WarnL<<"SPS base64:"<<strTemp;
	//WarnL<<"SPS hexdump:"<<hexdump(SPS_BUF, SPS_LEN);
	_strSdp += acTmp;
	_strSdp += ",";
	memset(acTmp, 0, sizeof(acTmp));
	av_base64_encode(acTmp, sizeof(acTmp), (uint8_t *) _aucPPS, _uiPPSLen);
	_strSdp += acTmp;
	_strSdp += "\r\n";
	if (_video->iFrameRate > 0 && _video->iHeight > 0 && _video->iWidth > 0) {
		_strSdp += "a=framerate:";
		_strSdp += StrPrinter << _video->iFrameRate << endl;
		_strSdp += StrPrinter << "\r\na=framesize:"
				<< _pRtpMaker_h264->getPlayloadType() << " " << endl;
		_strSdp += StrPrinter << _video->iWidth << endl;
		_strSdp += "-";
		_strSdp += StrPrinter << _video->iHeight << endl;
		_strSdp += "\r\n";
	}
	_strSdp += StrPrinter << "a=control:trackID="
			<< _pRtpMaker_h264->getInterleaved() / 2 << "\r\n" << endl;
	_bSdp_gotH264 = true;
	if (_audio) {
		if (_bSdp_gotAAC) {
			makeSDP(_strSdp);
		}
	} else {
		makeSDP(_strSdp);
	}
}

inline void DevChannel::makeSDP_AAC(unsigned char *fixedHeader) {
	auto audioSpecificConfig = makeAdtsConfig(fixedHeader);
	if (audioSpecificConfig.size() != 2) {
		return;
	}

	char fConfigStr[5] = { 0 };
	sprintf(fConfigStr, "%02X%02x", (uint8_t)audioSpecificConfig[0],(uint8_t)audioSpecificConfig[1]);

	_strSdp += StrPrinter << "m=audio 0 RTP/AVP "
			<< _pRtpMaker_aac->getPlayloadType() << "\r\n" << endl;
	_strSdp += "b=AS:96\r\n";
	_strSdp += StrPrinter << "a=rtpmap:" << _pRtpMaker_aac->getPlayloadType()
			<< " MPEG4-GENERIC/" << _pRtpMaker_aac->getSampleRate() << "\r\n"
			<< endl;
	_strSdp += StrPrinter << "a=fmtp:" << _pRtpMaker_aac->getPlayloadType()
				<< " streamtype=5;profile-level-id=1;mode=AAC-hbr;sizelength=13;indexlength=3;indexdeltalength=3;config="
				<< endl;
	_strSdp += fConfigStr;
	_strSdp += "\r\n";
	_strSdp += StrPrinter << "a=control:trackID="
			<< _pRtpMaker_aac->getInterleaved() / 2 << "\r\n" << endl;

	_bSdp_gotAAC = true;
	if (_video) {
		if (_bSdp_gotH264) {
			makeSDP(_strSdp);
		}
	} else {
		makeSDP(_strSdp);
	}
}

void DevChannel::makeSDP(const string& strSdp) {
	onGetSDP(strSdp);
}

void DevChannel::initVideo(const VideoInfo& info) {
	_video.reset(new VideoInfo(info));
}

void DevChannel::initAudio(const AudioInfo& info) {
	_audio.reset(new AudioInfo(info));
	_pAdtsHeader = std::make_shared<AACFrame>();

	_pAdtsHeader->syncword = 0x0FFF;
	_pAdtsHeader->id = 0;
	_pAdtsHeader->layer = 0;
	_pAdtsHeader->protection_absent = 1;
	_pAdtsHeader->profile =  info.iProfile;//audioObjectType - 1;
	int i = 0;
	for(auto rate : samplingFrequencyTable){
		if(rate == info.iSampleRate){
			_pAdtsHeader->sf_index = i;
		};
		++i;
	}

	_pAdtsHeader->private_bit = 0;
	_pAdtsHeader->channel_configuration = info.iChannel;
	_pAdtsHeader->original = 0;
	_pAdtsHeader->home = 0;
	_pAdtsHeader->copyright_identification_bit = 0;
	_pAdtsHeader->copyright_identification_start = 0;
	_pAdtsHeader->aac_frame_length = 7;
	_pAdtsHeader->adts_buffer_fullness = 2047;
	_pAdtsHeader->no_raw_data_blocks_in_frame = 0;

}
} /* namespace mediakit */

