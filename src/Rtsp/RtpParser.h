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

#ifndef SRC_RTP_RTPPARSER_H_
#define SRC_RTP_RTPPARSER_H_

#include <unordered_map>
#include "Rtsp/Rtsp.h"
#include "Player/Player.h"
#include "Player/PlayerBase.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Player;

namespace ZL {
namespace Rtsp {

class RtpParser : public PlayerBase{
public:
	typedef std::shared_ptr<RtpParser> Ptr;
	RtpParser(const string &sdp);
	virtual ~RtpParser();
	//返回值：true 代表是i帧第一个rtp包
	bool inputRtp(const RtpPacket &rtp);
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
		return m_bHaveAudio;
	}
	bool containVideo() const override{
		return m_bHaveVideo;
	}
    bool isInited() const override{
        if (m_bHaveAudio && !m_strAudioCfg.size()) {
            return false;
        }
        if (m_bHaveVideo && !m_strSPS.size()) {
            return false;
        }
        return true;
    }
	float getDuration() const override {
		return m_fDuration;
	}
private:
	std::unordered_map<uint8_t, RtspTrack> m_mapTracks;

	inline void onGetAudioTrack(const RtspTrack &audio);
	inline void onGetVideoTrack(const RtspTrack &video);

	//返回值：true 代表是i帧第一个rtp包
	inline bool inputVideo(const RtpPacket &rtp, const RtspTrack &track);
	inline bool inputAudio(const RtpPacket &rtp, const RtspTrack &track);
	inline void _onGetH264(H264Frame &frame);
	inline void onGetH264(H264Frame &frame);
	inline void onGetAdts(AdtsFrame &frame);
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
	bool m_bHaveAudio = false;
	bool m_bHaveVideo= false;
	float m_fDuration = 0;

	function<void(const H264Frame &frame)> onVideo;
	function<void(const AdtsFrame &frame)> onAudio;
	recursive_mutex m_mtxCB;
};

} /* namespace Rtsp */
} /* namespace ZL */

#endif /* SRC_RTP_RTPPARSER_H_ */
