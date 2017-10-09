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
#include "Device/base64.h"
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

bool RtpParser::inputRtp(const RtpPacket& rtp) {
	auto &track = m_mapTracks[rtp.PT];
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

inline bool RtpParser::inputVideo(const RtpPacket& rtppack,
		const RtspTrack& track) {
	const char *frame = (char *) rtppack.payload + 16;
	int length = rtppack.length - 16;
	NALU nal;
	MakeNalu(*frame, nal);
	//Type==1:P frame
	//Type==6:SEI frame
	//Type==7:SPS frame
	//Type==8:PPS frame
	if (nal.type > 0 && nal.type < 24) {
		//a full frame
		m_h264frame.data.assign("\x0\x0\x0\x1", 4);
		m_h264frame.data.append(frame, length);
		m_h264frame.type = nal.type;
		m_h264frame.timeStamp = rtppack.timeStamp / 90;
		m_h264frame.sequence = rtppack.sequence;
		_onGetH264(m_h264frame);
		m_h264frame.data.clear();
		return (m_h264frame.type == 7);
	}
	if (nal.type == 28) {
		//FU-A
		FU fu;
		MakeFU(frame[1], fu);
		if (fu.S == 1) {
			//FU-A start
			char tmp = (nal.forbidden_zero_bit << 7 | nal.nal_ref_idc << 5 | fu.type);
			m_h264frame.data.assign("\x0\x0\x0\x1", 4);
			m_h264frame.data.push_back(tmp);
			m_h264frame.data.append(frame + 2, length - 2);
			m_h264frame.type = fu.type;
			m_h264frame.timeStamp = rtppack.timeStamp / 90;
			m_h264frame.sequence = rtppack.sequence;
			return (m_h264frame.type == 7); //i frame
		}

		if (rtppack.sequence != (uint16_t)(m_h264frame.sequence + 1)) {
			m_h264frame.data.clear();
			WarnL << "丢包,帧废弃:" << rtppack.sequence << "," << m_h264frame.sequence;
			return false;
		}
		m_h264frame.sequence = rtppack.sequence;
		if (fu.E == 1) {
			//FU-A end
			m_h264frame.data.append(frame + 2, length - 2);
			m_h264frame.timeStamp = rtppack.timeStamp / 90;
			_onGetH264(m_h264frame);
			m_h264frame.data.clear();
			return false;
		}
		//FU-A mid
		m_h264frame.data.append(frame + 2, length - 2);
		return false;
	}
	WarnL << nal.type;
	return false;
	// 29 FU-B     单NAL单元B模式
	// 24 STAP-A   单一时间的组合包
	// 25 STAP-B   单一时间的组合包
	// 26 MTAP16   多个时间的组合包
	// 27 MTAP24   多个时间的组合包
	// 0 udef
	// 30 udef
	// 31 udef
}

inline void RtpParser::onGetAudioTrack(const RtspTrack& audio) {
	for (auto &ch : const_cast<string &>(audio.trackSdp)) {
		ch = tolower(ch);
	}
	if (audio.trackSdp.find("mpeg4-generic") == string::npos) {
		throw std::runtime_error("只支持aac格式的音频！");
	}
	string fConfigStr = FindField(audio.trackSdp.c_str(), "config=", "\r\n");
	if (fConfigStr.size() != 4) {
		fConfigStr = FindField(audio.trackSdp.c_str(), "config=", ";");
	}
	if (fConfigStr.size() != 4) {
		throw std::runtime_error("解析aac格式头失败！");
	}
	m_strAudioCfg.clear();
	unsigned int cfg1;
	sscanf(fConfigStr.substr(0, 2).c_str(), "%02X", &cfg1);
	cfg1 &= 0x00FF;
	m_strAudioCfg.push_back(cfg1);
	unsigned int cfg2;
	sscanf(fConfigStr.substr(2, 2).c_str(), "%02X", &cfg2);
	cfg2 &= 0x00FF;
	m_strAudioCfg.push_back(cfg2);
	makeAdtsHeader(m_strAudioCfg,m_adts);
	getAACInfo(m_adts, m_iSampleRate, m_iChannel);
	if(m_adts.profile >= 3){
		throw std::runtime_error("不支持该profile的AAC");
	}
}

inline void RtpParser::onGetVideoTrack(const RtspTrack& video) {
	if (video.trackSdp.find("H264") == string::npos) {
		throw std::runtime_error("只支持264格式的视频！");
	}
	string sps_pps = FindField(video.trackSdp.c_str(), "sprop-parameter-sets=", "\r\n");
	string base64_SPS = FindField(sps_pps.c_str(), NULL, ",");
	string base64_PPS = FindField(sps_pps.c_str(), ",", NULL);
	if(base64_PPS.back() == ';'){
		base64_PPS.pop_back();
	}
	uint8_t SPS_BUF[256], PPS_BUF[256];
	int SPS_LEN = av_base64_decode(SPS_BUF, base64_SPS.c_str(), sizeof(SPS_BUF));
	int PPS_LEN = av_base64_decode(PPS_BUF, base64_PPS.c_str(), sizeof(PPS_BUF));

	m_strSPS.assign("\x00\x00\x00\x01", 4);
	m_strSPS.append((char *) SPS_BUF, SPS_LEN);

	m_strPPS.assign("\x00\x00\x00\x01", 4);
	m_strPPS.append((char *) PPS_BUF, PPS_LEN);

	string strTmp((char *)SPS_BUF, SPS_LEN);
	if (!getAVCInfo(strTmp, m_iVideoWidth, m_iVideoHeight, m_fVideoFps)) {
		throw std::runtime_error("parse sdp failed");
	}
}

inline bool RtpParser::inputAudio(const RtpPacket& rtppack,
		const RtspTrack& track) {
	char *frame = (char *) rtppack.payload + 16;
	int length = rtppack.length - 16;

	if (m_adts.aac_frame_length + length - 4 > sizeof(AdtsFrame::data)) {
		m_adts.aac_frame_length = 7;
		return false;
	}
	memcpy(m_adts.data + m_adts.aac_frame_length, frame + 4, length - 4);
	m_adts.aac_frame_length += (length - 4);
	if (rtppack.mark == true) {
		m_adts.sequence = rtppack.sequence;
		m_adts.timeStamp = rtppack.timeStamp * (1000.0 / m_iSampleRate);
		writeAdtsHeader(m_adts, m_adts.data);
		onGetAdts(m_adts);
		m_adts.aac_frame_length = 7;
	}
	return false;
}

inline void RtpParser::_onGetH264(H264Frame& frame) {
	switch (frame.type) {
	case 5: {	//I
		H264Frame insertedFrame;
		insertedFrame.type = 7; //SPS
		insertedFrame.timeStamp = frame.timeStamp;
		insertedFrame.data = m_strSPS;
		onGetH264(insertedFrame);

		insertedFrame.type = 8; //PPS
		insertedFrame.timeStamp = frame.timeStamp;
		insertedFrame.data = m_strPPS;
		onGetH264(insertedFrame);
	}
	case 1:  //P
		onGetH264(frame);
		break;
	case 7://SPS
		m_strSPS=frame.data;break;
	case 8://PPS
		m_strPPS=frame.data;break;
	default:
		break;
	}
}

inline void RtpParser::onGetH264(H264Frame& frame) {
	//frame.timeStamp=ticker0.elapsedTime();
	lock_guard<recursive_mutex> lck(m_mtxCB);
	if (onVideo) {
		onVideo(frame);
	}
}

inline void RtpParser::onGetAdts(AdtsFrame& frame) {
	//frame.timeStamp=ticker1.elapsedTime();
	lock_guard<recursive_mutex> lck(m_mtxCB);
	if (onAudio) {
		onAudio(frame);
	}
}

} /* namespace Rtsp */
} /* namespace ZL */
