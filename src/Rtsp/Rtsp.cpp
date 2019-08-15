/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
#include "Common/Parser.h"

namespace mediakit{

static void getAttrSdp(const map<string, string> &attr, _StrPrinter &printer){
	const map<string, string>::value_type *ptr = nullptr;
	for(auto &pr : attr){
		if(pr.first == "control"){
			ptr = &pr;
			continue;
		}
		if(pr.second.empty()){
			printer << "a=" << pr.first << "\r\n";
		}else{
			printer << "a=" << pr.first << ":" << pr.second << "\r\n";
		}
	}
	if(ptr){
		printer << "a=" << ptr->first << ":" << ptr->second << "\r\n";
	}
}
string SdpTrack::toString() const {
	_StrPrinter _printer;
	switch (_type){
		case TrackTitle:{
			_printer << "v=" << 0 << "\r\n";
			if(!_o.empty()){
				_printer << "o="<< _o << "\r\n";
			}
			if(!_c.empty()){
				_printer << "c=" << _c << "\r\n";
			}
			if(!_t.empty()){
				_printer << "t=" << _t << "\r\n";
			}

			_printer << "s=RTSP Session, streamed by the ZLMediaKit\r\n";
			_printer << "i=ZLMediaKit Live Stream\r\n";
			getAttrSdp(_attr,_printer);
		}
			break;
		case TrackAudio:
		case TrackVideo:{
			if(_type == TrackAudio){
				_printer << "m=audio 0 RTP/AVP " << _pt << "\r\n";
			}else{
				_printer << "m=video 0 RTP/AVP " << _pt << "\r\n";
			}
			if(!_b.empty()){
				_printer << "b=" <<_b << "\r\n";
			}
			getAttrSdp(_attr,_printer);
		}
			break;
		default:
			break;
	}
	return _printer;
}
void SdpParser::load(const string &sdp) {
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
				key = FindField(opt_val.data(), nullptr," ");
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

bool SdpParser::available() const {
    return getTrack(TrackAudio) || getTrack(TrackVideo);
}

SdpTrack::Ptr SdpParser::getTrack(TrackType type) const {
	for (auto &pr : _track_map){
		if(pr.second->_type == type){
			return pr.second;
		}
	}
	return nullptr;
}

vector<SdpTrack::Ptr> SdpParser::getAvailableTrack() const {
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

string SdpParser::toString() const {
	string title,audio,video;
	for(auto &pr : _track_map){
		switch (pr.second->_type){
			case TrackTitle:{
				title = pr.second->toString();
			}
				break;
			case TrackVideo:{
				video = pr.second->toString();
			}
				break;
			case TrackAudio:{
				audio = pr.second->toString();
			}
				break;
			default:
				break;
		}
	}
	return title + video + audio;
}

}//namespace mediakit

