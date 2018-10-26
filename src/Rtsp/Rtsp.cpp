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
			auto surffix = string("/") + track._control;
			track._control_surffix = surffix.substr(1 + surffix.rfind('/'));
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

vector<SdpTrack::Ptr> SdpAttr::getAvailableTrack() const {
	vector<SdpTrack::Ptr> ret;
	auto video = getTrack(TrackVideo);
	if(video){
		ret.emplace_back(video);
	}
	auto audio = getTrack(TrackAudio);
	if(audio){
		ret.emplace_back(audio);
	}
	return ret;
}



