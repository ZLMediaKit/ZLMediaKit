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

using namespace std;
using namespace ZL::Util;

typedef enum {
	TrackVideo = 0, TrackAudio
} TrackType;

class RtspTrack{
public:
	uint8_t PT;
	uint8_t trackId;
	uint8_t interleaved;
	TrackType type = (TrackType) -1;
	string trackSdp;
	string trackStyle;
	bool inited;
	uint32_t ssrc = 0;
	uint16_t seq;
	uint32_t timeStamp;
};

class RtpPacket {
public:
    typedef std::shared_ptr<RtpPacket> Ptr;
    uint8_t interleaved;
    uint8_t PT;
    bool mark;
    uint32_t length;
    uint32_t timeStamp;
    uint16_t sequence;
    uint32_t ssrc;
    uint8_t payload[1560];
    TrackType type;
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
	Parser() {
	}
	virtual ~Parser() {
	}
	void Parse(const char *buf) {
		//解析
		const char *start = buf;
		string line;
		string field;
		string value;
		Clear();
		while (true) {
			line = FindField(start, NULL, "\r\n");
			if (line.size() == 0) {
				break;
			}
			if (start == buf) {
				m_strMethod = FindField(line.c_str(), NULL, " ");
				m_strUrl = FindField(line.c_str(), " ", " ");
				m_strTail = FindField(line.c_str(), (m_strUrl + " ").c_str(), NULL);
			} else {
				field = FindField(line.c_str(), NULL, ": ");
				value = FindField(line.c_str(), ": ", NULL);
				if (field.size() != 0) {
					m_mapValues[field] = value;
				}
			}
			start = start + line.size() + 2;
			if (strncmp(start, "\r\n", 2) == 0) { //协议解析完毕
				m_strContent = FindField(start, "\r\n", NULL);
				break;
			}
		}
	}
	const string& Method() const {
		//rtsp方法
		return m_strMethod;
	}
	const string& Url() const {
		//rtsp url
		return m_strUrl;
	}
	const string& Tail() const {
		//RTSP/1.0
		return m_strTail;
	}
	const string& operator[](const char *name) const {
		//rtsp field
		auto it = m_mapValues.find(name);
		if (it == m_mapValues.end()) {
			return m_strNull;
		}
		return it->second;
	}
	const string& Content() const {
		return m_strContent;
	}
	void Clear() {
		m_strMethod.clear();
		m_strUrl.clear();
		m_strTail.clear();
		m_strContent.clear();
		m_mapValues.clear();
	}

	void setUrl(const string& url) {
		this->m_strUrl = url;
	}
	void setContent(const string& content) {
		this->m_strContent = content;
	}

	const StrCaseMap& getValues() const {
		return m_mapValues;
	}

private:
	string m_strMethod;
	string m_strUrl;
	string m_strTail;
	string m_strContent;
	string m_strNull;
	StrCaseMap m_mapValues;

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

bool MakeNalu(char in, NALU &nal) ;
bool MakeFU(char in, FU &fu) ;


#endif //RTSP_RTSP_H_
