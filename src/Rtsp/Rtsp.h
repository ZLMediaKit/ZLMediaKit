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
#ifndef RTSP_RTSP_H_
#define RTSP_RTSP_H_

#include <string.h>
#include <string>
#include <memory>
#include <unordered_map>
#include "Common/config.h"
#include "Util/util.h"
#include "Player/Frame.h"

using namespace std;
using namespace ZL::Util;


class RtspTrack{
public:
	uint8_t PT;
    uint8_t interleaved;
	TrackType type = TrackInvalid;
	string trackSdp;
    string controlSuffix;
	bool inited;
	uint32_t ssrc = 0;
	uint16_t seq;
	uint32_t timeStamp;
};

class RtcpCounter {
public:
    uint32_t pktCnt = 0;
    uint32_t octCount = 0;
    uint32_t timeStamp = 0;
};

string FindField(const char* buf, const char* start, const char *end,int bufSize = 0 );
int parserSDP(const string& sdp, RtspTrack Track[2]);


struct StrCaseCompare
{
    bool operator()(const string& __x, const string& __y) const
    {return strcasecmp(__x.data(), __y.data()) < 0 ;}
};
typedef map<string,string,StrCaseCompare> StrCaseMap;

class Parser {
public:
	Parser() {}
	virtual ~Parser() {}
	void Parse(const char *buf) {
		//解析
		const char *start = buf;
		Clear();
		while (true) {
			auto line = FindField(start, NULL, "\r\n");
			if (line.size() == 0) {
				break;
			}
			if (start == buf) {
				_strMethod = FindField(line.c_str(), NULL, " ");
                _strFullUrl = FindField(line.c_str(), " ", " ");
				auto args_pos =  _strFullUrl.find('?');
				if(args_pos != string::npos){
					_strUrl = _strFullUrl.substr(0,args_pos);
					_mapUrlArgs = parseArgs(_strFullUrl.substr(args_pos + 1 ));
				}else{
					_strUrl = _strFullUrl;
				}
				_strTail = FindField(line.c_str(), (_strFullUrl + " ").c_str(), NULL);
			} else {
				auto field = FindField(line.c_str(), NULL, ": ");
				auto value = FindField(line.c_str(), ": ", NULL);
				if (field.size() != 0) {
					_mapHeaders[field] = value;
				}
			}
			start = start + line.size() + 2;
			if (strncmp(start, "\r\n", 2) == 0) { //协议解析完毕
				_strContent = FindField(start, "\r\n", NULL);
				break;
			}
		}
	}
	const string& Method() const {
		//rtsp方法
		return _strMethod;
	}
	const string& Url() const {
		//rtsp url
		return _strUrl;
	}
    const string& FullUrl() const {
        //rtsp url with args
        return _strFullUrl;
    }
	const string& Tail() const {
		//RTSP/1.0
		return _strTail;
	}
	const string& operator[](const char *name) const {
		//rtsp field
		auto it = _mapHeaders.find(name);
		if (it == _mapHeaders.end()) {
			return _strNull;
		}
		return it->second;
	}
	const string& Content() const {
		return _strContent;
	}
	void Clear() {
		_strMethod.clear();
		_strUrl.clear();
        _strFullUrl.clear();
		_strTail.clear();
		_strContent.clear();
		_mapHeaders.clear();
		_mapUrlArgs.clear();
	}

	void setUrl(const string& url) {
		this->_strUrl = url;
	}
	void setContent(const string& content) {
		this->_strContent = content;
	}

	StrCaseMap& getValues() const {
		return _mapHeaders;
	}
	StrCaseMap& getUrlArgs() const {
		return _mapUrlArgs;
	}

	static StrCaseMap parseArgs(const string &str,const char *pair_delim = "&", const char *key_delim = "="){
		StrCaseMap ret;
		auto arg_vec = split(str, pair_delim);
		for (string &key_val : arg_vec) {
			auto key = FindField(key_val.data(),NULL,key_delim);
			auto val = FindField(key_val.data(),key_delim, NULL);
			ret[key] = val;
		}
		return ret;
	}

private:
	string _strMethod;
	string _strUrl;
	string _strTail;
	string _strContent;
	string _strNull;
    string _strFullUrl;
	mutable StrCaseMap _mapHeaders;
	mutable StrCaseMap _mapUrlArgs;
};

typedef struct {
	unsigned forbidden_zero_bit :1;
	unsigned nal_ref_idc :2;
	unsigned type :5;
} NALU;

typedef struct {
	unsigned S :1;
	unsigned E :1;
	unsigned R :1;
	unsigned type :5;
} FU;

bool MakeNalu(uint8_t in, NALU &nal) ;
bool MakeFU(uint8_t in, FU &fu) ;


#endif //RTSP_RTSP_H_
