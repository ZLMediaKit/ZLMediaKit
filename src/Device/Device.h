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

#ifndef DEVICE_DEVICE_H_
#define DEVICE_DEVICE_H_

#include <memory>
#include <string>
#include <functional>
#include "Util/util.h"
#include "RTP/RtpMakerAAC.h"
#include "RTP/RtpMakerH264.h"
#include "Rtsp/RtspToRtmpMediaSource.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Rtsp;
using namespace ZL::Util;

#ifdef ENABLE_FAAC
#include "Codec/AACEncoder.h"
using namespace ZL::Codec;
#endif //ENABLE_FAAC

#ifdef ENABLE_X264
#include "Codec/H264Encoder.h"
using namespace ZL::Codec;
#endif //ENABLE_X264


namespace ZL {
namespace DEV {

class VideoInfo {
public:
	int iWidth;
	int iHeight;
	float iFrameRate;
};
class AudioInfo {
public:
	int iChannel;
	int iSampleBit;
	int iSampleRate;
};

class DevChannel  : public RtspToRtmpMediaSource{
public:
	typedef std::shared_ptr<DevChannel> Ptr;
    //fDuration<=0为直播，否则为点播
	DevChannel(const char *strVhost,
               const char *strApp,
               const char *strId,
               float fDuration = 0,
               bool bEanbleHls = true,
               bool bEnableMp4 = false);
	virtual ~DevChannel();

	void initVideo(const VideoInfo &info);
	void initAudio(const AudioInfo &info);

	void inputH264(char *pcData, int iDataLen, uint32_t uiStamp);
	void inputAAC(char *pcDataWithAdts, int iDataLen, uint32_t uiStamp);
	void inputAAC(char *pcDataWithoutAdts,int iDataLen, uint32_t uiStamp,char *pcAdtsHeader);

#ifdef ENABLE_X264
        void inputYUV(char *apcYuv[3], int aiYuvLen[3], uint32_t uiStamp);
#endif //ENABLE_X264

#ifdef ENABLE_FAAC
        void inputPCM(char *pcData, int iDataLen, uint32_t uiStamp);
#endif //ENABLE_FAAC

private:
	inline void makeSDP_264(unsigned char *pucData, int iDataLen);
	inline void makeSDP_AAC(unsigned char *pucData);
	inline void makeSDP(const string& strSdp);
#ifdef ENABLE_X264
	std::shared_ptr<H264Encoder> m_pH264Enc;
#endif //ENABLE_X264

#ifdef ENABLE_FAAC
	std::shared_ptr<AACEncoder> m_pAacEnc;
#endif //ENABLE_FAAC
	RtpMaker_AAC::Ptr m_pRtpMaker_aac;
	RtpMaker_H264::Ptr m_pRtpMaker_h264;
	bool m_bSdp_gotH264 = false;
	bool m_bSdp_gotAAC = false;

	unsigned char m_aucSPS[256];
	unsigned int m_uiSPSLen = 0;
	unsigned char m_aucPPS[256];
	unsigned int m_uiPPSLen = 0;
	std::shared_ptr<VideoInfo> m_video;
	std::shared_ptr<AudioInfo> m_audio;
    SmoothTicker m_aTicker[2];
};


} /* namespace DEV */
} /* namespace ZL */

#endif /* DEVICE_DEVICE_H_ */
