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


void SdpAttr::load(const string &sdp) {
	_track_map.clear();
	string key;
	SdpTrack::Ptr track = std::make_shared<SdpTrack>();

	auto lines = split(sdp,"\n");
	for (auto &line : lines){
		trim(line);
		if(line.size() < 2 || line[1] != '='){
			continue;
		}
		char opt = line[0];
		string opt_val = line.substr(2);
		switch (opt){
			case 'o':
				track->_o = opt_val;
				break;
			case 's':
				track->_s = opt_val;
				break;
			case 'i':
				track->_i = opt_val;
				break;
			case 'c':
				track->_c = opt_val;
				break;
			case 't':
				track->_t = opt_val;
				break;
			case 'b':
				track->_b = opt_val;
				break;
			case 'm':{
				_track_map[key] = track;
				track = std::make_shared<SdpTrack>();
				key = FindField(opt_val.data(), nullptr," ");;
				track->_m = opt_val;
			}
				break;
			case 'a':{
				string attr = FindField(opt_val.data(), nullptr,":");
				if(attr.empty()){
					track->_attr[opt_val] = "";
				}else{
					track->_attr[attr] = FindField(opt_val.data(),":", nullptr);
				}
			}
				break;
			default:
				track->_other[opt] = opt_val;
				break;
		}
	}
	_track_map[key] = track;


	for (auto &pr : _track_map) {
		auto &track = *pr.second;
		if (pr.first == "") {
			track._type = TrackTitle;
		} else if (pr.first == "video") {
			track._type = TrackVideo;
		} else if (pr.first == "audio") {
			track._type = TrackAudio;
		} else {
			track._type = TrackInvalid;
		}

		auto it = track._attr.find("range");
		if (it != track._attr.end()) {
			char name[16] = {0}, start[16] = {0}, end[16] = {0};
			int ret = sscanf(it->second.data(), "%15[^=]=%15[^-]-%15s", name, start, end);
			if (3 == ret || 2 == ret) {
				if (strcmp(start, "now") == 0) {
					strcpy(start, "0");
				}
				track._start = atof(start);
				track._end = atof(end);
				track._duration = track._end - track._start;
			}
		}

		it = track._attr.find("rtpmap");
		if(it != track._attr.end()){
			auto rtpmap = it->second;
			int pt, samplerate;
			char codec[16] = {0};
			if (3 == sscanf(rtpmap.data(), "%d %15[^/]/%d", &pt, codec, &samplerate)) {
				track._pt = pt;
				track._codec = codec;
				track._samplerate = samplerate;
			}
		}

		it = track._attr.find("fmtp");
		if(it != track._attr.end()) {
			track._fmtp = it->second;
		}

		it = track._attr.find("control");
		if(it != track._attr.end()) {
			track._control = it->second;
		}
	}
}

bool SdpAttr::available() const {
    return getTrack(TrackAudio) || getTrack(TrackVideo);
}

SdpTrack::Ptr SdpAttr::getTrack(TrackType type) const {
	for (auto &pr : _track_map){
		if(pr.second->_type == type){
			return pr.second;
		}
	}
	return nullptr;
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
		pos_head = pos_end;
		Track[track_cnt].trackSdp = sdp_mid;
		Track[track_cnt].inited = false;
		Track[track_cnt].PT = atoi(FindField(sdp_mid.c_str(), "a=rtpmap:", " ").c_str());
        auto control = string("/") + trim(FindField(sdp_mid.c_str(), "a=control:", "\n"));
        Track[track_cnt].controlSuffix = control.substr(1 + control.rfind('/'));

		if (sdp_mid.find("m=video") != string::npos) {
			//视频通道
			Track[track_cnt].type = TrackVideo;
        } else if (sdp_mid.find("m=audio") != string::npos) {
			//音频通道
			Track[track_cnt].type = TrackAudio;
        } else {
			//不识别的track
			continue;
		}
        track_cnt++;
	}
	return track_cnt;
}
static  onceToken s_token([](){
   string str = "v=0\n"
           "o=- 1001 1 IN IP4 192.168.0.22\n"
           "s=VCP IPC Realtime stream\n"
		   "a=range:npt=0-\n"
           "m=video 0 RTP/AVP 105\n"
           "c=IN IP4 192.168.0.22\n"
           "a=control:rtsp://192.168.0.22/media/video1/video\n"
           "a=rtpmap:105 H264/90000\n"
           "a=fmtp:105 profile-level-id=64001f; packetization-mode=1; sprop-parameter-sets=Z2QAH6wrUCgC3QgAAB9AAAYahCAA,aO4xsg==\n"
           "a=recvonly\n"
           "m=application 0 RTP/AVP 107\n"
           "c=IN IP4 192.168.0.22\n"
           "a=control:rtsp://192.168.0.22/media/video1/metadata\n"
           "a=rtpmap:107 vnd.onvif.metadata/90000\n"
           "a=fmtp:107 DecoderTag=h3c-v3 RTCP=0\n"
           "a=recvonly";
    RtspTrack track[2];
    parserSDP(str,track);
	SdpAttr attr(str);
    track[0].inited=true;
});


