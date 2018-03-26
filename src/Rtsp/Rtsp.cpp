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
	int track_cnt = 0;
	string::size_type pos_head = 0;
	while ((pos_head = sdp.find("m=",pos_head)) != string::npos ) {
        auto pos_end = sdp.find("m=", pos_head + 2);
		if (pos_end == string::npos) {
			pos_end = sdp.size();
		}
		auto sdp_mid = sdp.substr(pos_head, pos_end - pos_head);
		Track[track_cnt].trackSdp = sdp_mid;
		Track[track_cnt].inited = false;
		Track[track_cnt].PT = atoi(FindField(sdp_mid.c_str(), "a=rtpmap:", " ").c_str());
        auto control = string("/") + trim(FindField(sdp_mid.c_str(), "a=control:", "\n"));
        Track[track_cnt].controlSuffix = control.substr(1 + control.find_last_of('/'));

		if (sdp_mid.find("m=video") != string::npos) {
			//视频通道
			Track[track_cnt].type = TrackVideo;
            Track[track_cnt].trackId = 0;
        } else if (sdp_mid.find("m=audio") != string::npos) {
			//音频通道
			Track[track_cnt].type = TrackAudio;
            Track[track_cnt].trackId = 1;
        } else {
			//不识别的track
			return track_cnt;
		}
		pos_head = pos_end;
        track_cnt++;
	}
	return track_cnt;
}
//static  onceToken s_token([](){
//   string str = "v=0\n"
//           "o=- 1001 1 IN IP4 192.168.0.22\n"
//           "s=VCP IPC Realtime stream\n"
//           "m=video 0 RTP/AVP 105\n"
//           "c=IN IP4 192.168.0.22\n"
//           "a=control:rtsp://192.168.0.22/media/video1/video\n"
//           "a=rtpmap:105 H264/90000\n"
//           "a=fmtp:105 profile-level-id=64001f; packetization-mode=1; sprop-parameter-sets=Z2QAH6wrUCgC3QgAAB9AAAYahCAA,aO4xsg==\n"
//           "a=recvonly\n"
//           "m=application 0 RTP/AVP 107\n"
//           "c=IN IP4 192.168.0.22\n"
//           "a=control:rtsp://192.168.0.22/media/video1/metadata\n"
//           "a=rtpmap:107 vnd.onvif.metadata/90000\n"
//           "a=fmtp:107 DecoderTag=h3c-v3 RTCP=0\n"
//           "a=recvonly";
//    RtspTrack track[2];
//    parserSDP(str,track);
//    track[0].inited=true;
//});
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
