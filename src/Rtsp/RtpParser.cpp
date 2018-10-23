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
	if (0 == cnt) {
		throw std::runtime_error("parse sdp failed");
	}

	for (int i = 0; i < cnt; i++) {
		switch (tmp[i].type) {
		case TrackVideo: {
			try {
				onGetVideoTrack(tmp[i]);
				m_bHaveVideo = true;
				m_mapTracks.emplace(tmp[i].PT, tmp[i]);
			} catch (std::exception &ex) {
				WarnL << ex.what();
			}
		}
			break;
		case TrackAudio: {
			try {
				onGetAudioTrack(tmp[i]);
				m_bHaveAudio = true;
				m_mapTracks.emplace(tmp[i].PT, tmp[i]);
			} catch (std::exception &ex) {
				WarnL << ex.what();
			}
		}
			break;
		default:
			break;
		}
	}
	if (!m_bHaveVideo && !m_bHaveAudio) {
		throw std::runtime_error("不支持该RTSP媒体格式");
	}
	m_fDuration = getTimeInSDP(sdp);
}
RtpParser::~RtpParser() {
}

bool RtpParser::inputRtp(const RtpPacket::Ptr & rtp) {
	auto &track = m_mapTracks[rtp->PT];
	switch (track.type) {
	case TrackVideo:
		if (m_bHaveVideo) {
			return inputVideo(rtp, track);
		}
		return false;
	case TrackAudio:
		if (m_bHaveAudio) {
			return inputAudio(rtp, track);
		}
		return false;
	default:
		return false;
	}
}

inline bool RtpParser::inputVideo(const RtpPacket::Ptr & rtp, const RtspTrack& track) {

}

inline void RtpParser::onGetAudioTrack(const RtspTrack& audio) {

}

inline void RtpParser::onGetVideoTrack(const RtspTrack& video) {

}

inline bool RtpParser::inputAudio(const RtpPacket::Ptr &rtppack, const RtspTrack& track) {
}

} /* namespace Rtsp */
} /* namespace ZL */
