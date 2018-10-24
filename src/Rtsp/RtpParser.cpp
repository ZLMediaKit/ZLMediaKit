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

#include <cctype>
#include <algorithm>
#include "RtpParser.h"
#include "Util/base64.h"
#include "H264/SPSParser.h"
#include "Common/Factory.h"

using namespace std;

namespace ZL {
namespace Rtsp {

static int getTimeInSDP(const string &sdp) {
	auto strRange = FindField(sdp.data(), "a=range:npt=", "\r\n");
	strRange.append(" ");
	auto iPos = strRange.find('-');
	if (iPos == string::npos) {
		return 0;
	}
	auto strStart = strRange.substr(0, iPos);
	auto strEnd = strRange.substr(iPos + 1);
	strEnd.pop_back();
	if (strStart == "now") {
		strStart = "0";
	}
	return atof(strEnd.data()) - atof(strStart.data());
}
RtpParser::RtpParser(const string& sdp) {
	RtspTrack tmp[2];
	int cnt = parserSDP(sdp, tmp);
	for (int i = 0; i < cnt; i++) {
		switch (tmp[i].type) {
		case TrackVideo: {
            onGetVideoTrack(tmp[i]);
		}
			break;
		case TrackAudio: {
            onGetAudioTrack(tmp[i]);
		}
			break;
		default:
			break;
		}
	}
	m_fDuration = getTimeInSDP(sdp);
}

bool RtpParser::inputRtp(const RtpPacket::Ptr & rtp) {
	switch (rtp->getTrackType()) {
	case TrackVideo:
        return inputVideo(rtp);
	case TrackAudio:
        return inputAudio(rtp);
	default:
		return false;
	}
}

inline bool RtpParser::inputVideo(const RtpPacket::Ptr &rtp) {
    if(_videoRtpDecoder){
		return _videoRtpDecoder->inputRtp(rtp, true);
    }
	return false;
}

inline bool RtpParser::inputAudio(const RtpPacket::Ptr &rtp) {
    if(_audioRtpDecoder){
		return _audioRtpDecoder->inputRtp(rtp, false);
    }
	return false;
}

inline void RtpParser::onGetAudioTrack(const RtspTrack& audio) {
	//生成Track对象
    _audioTrack = dynamic_pointer_cast<AudioTrack>(Factory::getTrackBySdp(audio.trackSdp));
    if(_audioTrack){
    	//生成RtpCodec对象以便解码rtp
		_audioRtpDecoder = Factory::getRtpDecoderById(_audioTrack->getCodecId(),_audioTrack->getAudioSampleRate());
		if(_audioRtpDecoder){
			//设置rtp解码器代理，生成的frame写入该Track
			_audioRtpDecoder->setDelegate(_audioTrack);
		}
    }
}

inline void RtpParser::onGetVideoTrack(const RtspTrack& video) {
	//生成Track对象
	_videoTrack = dynamic_pointer_cast<VideoTrack>(Factory::getTrackBySdp(video.trackSdp));
	if(_videoTrack){
		//生成RtpCodec对象以便解码rtp
		_videoRtpDecoder = Factory::getRtpDecoderById(_videoTrack->getCodecId(),90000);
		if(_videoRtpDecoder){
			//设置rtp解码器代理，生成的frame写入该Track
			_videoRtpDecoder->setDelegate(_videoTrack);
		}
	}
}

vector<Track::Ptr> RtpParser::getTracks() const {
	vector<Track::Ptr> ret;
	if(_videoTrack){
		ret.emplace_back(_videoTrack);
	}
	if(_audioTrack){
		ret.emplace_back(_audioTrack);
	}
	return ret;
}


} /* namespace Rtsp */
} /* namespace ZL */
