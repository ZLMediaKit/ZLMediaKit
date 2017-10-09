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

#include <stdlib.h>
#include "Rtsp.h"

string FindField(const char* buf, const char* start, const char *end ,int bufSize) {
	if(bufSize <=0 ){
		bufSize = strlen(buf);
	}
	const char *msg_start = buf, *msg_end = buf + bufSize;
	int len = 0;
	if (start != NULL) {
		len = strlen(start);
		msg_start = strstr(buf, start);
	}
	if (msg_start == NULL) {
		return "";
	}
	msg_start += len;
	if (end != NULL) {
		msg_end = strstr(msg_start, end);
		if (msg_end == NULL) {
			return "";
		}
	}
	return string(msg_start, msg_end);
}
int parserSDP(const string& sdp, RtspTrack Track[2]) {
	string track_str;
	if (sdp.find("trackID=") != string::npos) {
		track_str = "trackID=";
	}else if (sdp.find("track") != string::npos) {
		track_str = "track";
	}else if (sdp.find("streamid=") != string::npos) {
		track_str = "streamid=";
	}else if (sdp.find("stream") != string::npos) {
		track_str = "stream";
	}
	int track_cnt = 0;
	string::size_type pos_head = sdp.find("m=");
	string::size_type pos_end = 0;
	string IDStr;
	int TrackID;
	string mid;
	while (true) {
		IDStr = FindField(sdp.c_str() + pos_head, track_str.c_str(), "\r\n");
		if (IDStr == "") {
			break;
		}
		TrackID = atoi(IDStr.c_str());
		pos_end = sdp.find("m=", pos_head + 2);
		if (pos_end == string::npos) {
			pos_end = sdp.size();
		}
		mid = sdp.substr(pos_head, pos_end - pos_head);
		Track[track_cnt].trackSdp = mid;
		Track[track_cnt].trackStyle = track_str;
		Track[track_cnt].inited = false;
		Track[track_cnt].trackId = TrackID;
		Track[track_cnt].PT = atoi(
				FindField(mid.c_str(), "rtpmap:", " ").c_str());
		if (mid.find("m=video") != string::npos) {
			//视频通道
			Track[track_cnt].type = TrackVideo;
		} else if (mid.find("m=audio") != string::npos) {
			//音频通道
			Track[track_cnt].type = TrackAudio;
		} else {
			//不识别的track
			return 0;
		}
		pos_head = pos_end;
		if (track_cnt++ > 2) {
			//最大支持2个通道
			return 0;
		}
	}
	return track_cnt;

}

bool MakeNalu(char in, NALU &nal) {
	nal.forbidden_zero_bit = in >> 7;
	if (nal.forbidden_zero_bit) {
		return false;
	}
	nal.nal_ref_idc = (in & 0x60) >> 5;
	nal.type = in & 0x1f;
	return true;
}
bool MakeFU(char in, FU &fu) {
	fu.S = in >> 7;
	fu.E = (in >> 6) & 0x01;
	fu.R = (in >> 5) & 0x01;
	fu.type = in & 0x1f;
	if (fu.R != 0) {
		return false;
	}
	return true;
}
