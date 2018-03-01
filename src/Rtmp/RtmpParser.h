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

#ifndef SRC_RTMP_RTMPPARSER_H_
#define SRC_RTMP_RTMPPARSER_H_

#include <functional>
#include <unordered_map>
#include "amf.h"
#include "Rtmp.h"
#include "Player/Player.h"
#include "Util/TimeTicker.h"
#include "Player/PlayerBase.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Player;

#define H264_CODEC_ID 7
#define AAC_CODEC_ID 10

namespace ZL {
namespace Rtmp {

class RtmpParser : public PlayerBase{
public:
	typedef std::shared_ptr<RtmpParser> Ptr;
	RtmpParser(const AMFValue &val);
	virtual ~RtmpParser();

	bool inputRtmp(const RtmpPacket::Ptr &pkt);

	void setOnVideoCB(const function<void(const H264Frame &frame)> &cb) override{
		lock_guard<recursive_mutex> lck(m_mtxCB);
		onVideo = cb;
	}
	void setOnAudioCB(const function<void(const AdtsFrame &frame)> &cb) override{
		lock_guard<recursive_mutex> lck(m_mtxCB);
		onAudio = cb;
	}

	int getVideoHeight() const override{
		return m_iVideoHeight;
	}

	int getVideoWidth() const override{
		return m_iVideoWidth;
	}

	float getVideoFps() const override{
		return m_fVideoFps;
	}

	int getAudioSampleRate() const override{
		return m_iSampleRate;
	}

	int getAudioSampleBit() const override{
		return m_iSampleBit;
	}

	int getAudioChannel() const override{
		return m_iChannel;
	}

	const string& getPps() const override{
		return m_strPPS;
	}

	const string& getSps() const override{
		return m_strSPS;
	}

	const string& getAudioCfg() const override{
		return m_strAudioCfg;
	}
	bool containAudio() const override{
        //音频只支持aac
		return m_iAudioCodecID == AAC_CODEC_ID;
	}
	bool containVideo () const override{
        //视频只支持264
		return m_iVideoCodecID == H264_CODEC_ID;
	}
	bool isInited() const override{
        if((m_iAudioCodecID | m_iVideoCodecID) == 0){
            //音视频codec_id都未获取到，说明还未初始化成功
            return false;
        }
        if((m_iAudioCodecID & m_iVideoCodecID) == 0 && m_ticker.elapsedTime() < 300){
            //音视频codec_id有其一未获取到,且最少分析300ms才能断定没有音频或视频
            return false;
        }
		if (m_iAudioCodecID && !m_strAudioCfg.size()) {
            //如果音频是aac但是还未获取aac config ，则未初始化成功
			return false;
		}
		if (m_iVideoCodecID && !m_strSPS.size()) {
            //如果视频是h264但是还未获取sps ，则未初始化成功
            return false;
		}
        //初始化成功
		return true;
	}
	float getDuration() const override{
		return m_fDuration;
	}
private:
	inline void onCheckMedia(const AMFValue &obj);

	//返回值：true 代表是i帧第一个rtp包
	inline bool inputVideo(const RtmpPacket::Ptr &pkt);
	inline bool inputAudio(const RtmpPacket::Ptr &pkt);
	inline void _onGetH264(const char *pcData, int iLen, uint32_t ui32TimeStamp);
	inline void onGetH264(const char *pcData, int iLen, uint32_t ui32TimeStamp);
	inline void onGetAAC(const char *pcData, int iLen, uint32_t ui32TimeStamp);
	//video
	H264Frame m_h264frame;
	//aduio
	AdtsFrame m_adts;

	int m_iSampleRate = 44100;
	int m_iSampleBit = 16;
	int m_iChannel = 1;

	string m_strSPS;
	string m_strPPS;
	string m_strAudioCfg;
	int m_iVideoWidth = 0;
	int m_iVideoHeight = 0;
	float m_fVideoFps = 0;
    //音视频codec_id初始为0代表尚未获取到
    int m_iAudioCodecID = 0;
    int m_iVideoCodecID = 0;
	float m_fDuration = 0;
    mutable Ticker m_ticker;
    function<void(const H264Frame &frame)> onVideo;
	function<void(const AdtsFrame &frame)> onAudio;
	recursive_mutex m_mtxCB;


};

} /* namespace Rtmp */
} /* namespace ZL */

#endif /* SRC_RTMP_RTMPPARSER_H_ */
