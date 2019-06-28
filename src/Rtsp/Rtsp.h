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
#ifndef RTSP_RTSP_H_
#define RTSP_RTSP_H_

#include <string.h>
#include <string>
#include <memory>
#include <unordered_map>
#include "Common/config.h"
#include "Util/util.h"
#include "Extension/Frame.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace mediakit {

namespace Rtsp {
typedef enum {
	RTP_Invalid = -1,
	RTP_TCP = 0,
	RTP_UDP = 1,
	RTP_MULTICAST = 2,
} eRtpType;
};

class RtpPacket : public BufferRaw{
public:
	typedef std::shared_ptr<RtpPacket> Ptr;
	uint8_t interleaved;
	uint8_t PT;
	bool mark;
	//时间戳，单位毫秒
	uint32_t timeStamp;
	uint16_t sequence;
	uint32_t ssrc;
	uint8_t offset;
	TrackType type;
};

class SdpTrack {
public:
	typedef std::shared_ptr<SdpTrack> Ptr;

	string _m;
	string _o;
	string _s;
	string _i;
	string _c;
	string _t;
	string _b;

	float _duration = 0;
	float _start = 0;
	float _end = 0;

	map<char, string> _other;
	map<string, string> _attr;
public:
	int _pt;
	string _codec;
	int _samplerate;
	string _fmtp;
	string _control;
	string _control_surffix;
	TrackType _type;
public:
	uint8_t _interleaved = 0;
	bool _inited = false;
	uint32_t _ssrc = 0;
	uint16_t _seq = 0;
	//时间戳，单位毫秒
	uint32_t _time_stamp = 0;
};

class SdpAttr {
public:
	typedef std::shared_ptr<SdpAttr> Ptr;

	SdpAttr() {}
	SdpAttr(const string &sdp) { load(sdp); }
	~SdpAttr() {}
	void load(const string &sdp);
	bool available() const;
	SdpTrack::Ptr getTrack(TrackType type) const;
	vector<SdpTrack::Ptr> getAvailableTrack() const;
private:
	map<string, SdpTrack::Ptr> _track_map;
};


class RtcpCounter {
public:
	uint32_t pktCnt = 0;
	uint32_t octCount = 0;
	//网络字节序
	uint32_t timeStamp = 0;
    uint32_t lastTimeStamp = 0;
};

string FindField(const char *buf, const char *start, const char *end, int bufSize = 0);

struct StrCaseCompare {
	bool operator()(const string &__x, const string &__y) const { return strcasecmp(__x.data(), __y.data()) < 0; }
};


class StrCaseMap : public multimap<string, string, StrCaseCompare>{
public:
    typedef multimap<string, string, StrCaseCompare> Super ;
    StrCaseMap() = default;
    ~StrCaseMap() = default;
    string &operator[](const string &key){
        auto it = find(key);
        if(it == end()){
            it = Super::emplace(key,"");
        }
        return it->second;
    }

    template <class K,class V>
    void emplace(K &&k , V &&v) {
        auto it = find(k);
        if(it != end()){
            return;
        }
        Super::emplace(std::forward<K>(k),std::forward<V>(v));
    }

    template <class K,class V>
    void emplace_force(K &&k , V &&v) {
        Super::emplace(std::forward<K>(k),std::forward<V>(v));
    }
};

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
				_strMethod = FindField(line.data(), NULL, " ");
				_strFullUrl = FindField(line.data(), " ", " ");
				auto args_pos = _strFullUrl.find('?');
				if (args_pos != string::npos) {
					_strUrl = _strFullUrl.substr(0, args_pos);
                    _params = _strFullUrl.substr(args_pos + 1);
					_mapUrlArgs = parseArgs(_params);
				} else {
					_strUrl = _strFullUrl;
				}
				_strTail = FindField(line.data(), (_strFullUrl + " ").data(), NULL);
			} else {
				auto field = FindField(line.data(), NULL, ": ");
				auto value = FindField(line.data(), ": ", NULL);
				if (field.size() != 0) {
					_mapHeaders.emplace_force(field,value);
				}
			}
			start = start + line.size() + 2;
			if (strncmp(start, "\r\n", 2) == 0) { //协议解析完毕
				_strContent = FindField(start, "\r\n", NULL);
				break;
			}
		}
	}

	const string &Method() const {
		//rtsp方法
		return _strMethod;
	}

	const string &Url() const {
		//rtsp url
		return _strUrl;
	}

	const string &FullUrl() const {
		//rtsp url with args
		return _strFullUrl;
	}

	const string &Tail() const {
		//RTSP/1.0
		return _strTail;
	}

	const string &operator[](const char *name) const {
		//rtsp field
		auto it = _mapHeaders.find(name);
		if (it == _mapHeaders.end()) {
			return _strNull;
		}
		return it->second;
	}

	const string &Content() const {
		return _strContent;
	}

	void Clear() {
		_strMethod.clear();
		_strUrl.clear();
		_strFullUrl.clear();
        _params.clear();
		_strTail.clear();
		_strContent.clear();
		_mapHeaders.clear();
		_mapUrlArgs.clear();
	}
	 const string &Params() const {
         return _params;
	}

	void setUrl(const string &url) {
		this->_strUrl = url;
	}

	void setContent(const string &content) {
		this->_strContent = content;
	}

	StrCaseMap &getValues() const {
		return _mapHeaders;
	}

	StrCaseMap &getUrlArgs() const {
		return _mapUrlArgs;
	}

	static StrCaseMap parseArgs(const string &str, const char *pair_delim = "&", const char *key_delim = "=") {
		StrCaseMap ret;
		auto arg_vec = split(str, pair_delim);
		for (string &key_val : arg_vec) {
			auto key = FindField(key_val.data(), NULL, key_delim);
			auto val = FindField(key_val.data(), key_delim, NULL);
			ret.emplace_force(key,val);
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
	string _params;
	mutable StrCaseMap _mapHeaders;
	mutable StrCaseMap _mapUrlArgs;
};


/**
* rtsp sdp基类
*/
class Sdp : public CodecInfo{
public:
	typedef std::shared_ptr<Sdp> Ptr;

	/**
	 * 构造sdp
	 * @param sample_rate 采样率
	 * @param playload_type pt类型
	 */
	Sdp(uint32_t sample_rate, uint8_t playload_type){
		_sample_rate = sample_rate;
		_playload_type = playload_type;
	}

	virtual ~Sdp(){}

	/**
	 * 获取sdp字符串
	 * @return
	 */
	virtual string getSdp() const  = 0;

	/**
	 * 获取pt
	 * @return
	 */
	uint8_t getPlayloadType() const{
		return _playload_type;
	}

	/**
	 * 获取采样率
	 * @return
	 */
	uint32_t getSampleRate() const{
		return _sample_rate;
	}
private:
	uint8_t _playload_type;
	uint32_t _sample_rate;
};

/**
* sdp中除音视频外的其他描述部分
*/
class TitleSdp : public Sdp{
public:

	/**
	 * 构造title类型sdp
	 * @param dur_sec rtsp点播时长，0代表直播，单位秒
	 * @param header 自定义sdp描述
	 * @param version sdp版本
	 */
	TitleSdp(float dur_sec = 0,
			 const map<string,string> &header = map<string,string>(),
			 int version = 0) : Sdp(0,0){
		_printer << "v=" << version << "\r\n";

		if(!header.empty()){
			for (auto &pr : header){
				_printer << pr.first << "=" << pr.second << "\r\n";
			}
		} else {
			_printer << "o=- 1383190487994921 1 IN IP4 0.0.0.0\r\n";
			_printer << "s=RTSP Session, streamed by the ZLMediaKit\r\n";
			_printer << "i=ZLMediaKit Live Stream\r\n";
			_printer << "c=IN IP4 0.0.0.0\r\n";
			_printer << "t=0 0\r\n";
		}

		if(dur_sec <= 0){
			_printer << "a=range:npt=0-\r\n";
		}else{
			_printer << "a=range:npt=0-" << dur_sec  << "\r\n";
		}
		_printer << "a=control:*\r\n";
	}
	string getSdp() const override {
		return _printer;
	}
	/**
	 * 返回音频或视频类型
	 * @return
	 */
	TrackType getTrackType() const override {
		return TrackTitle;
	}

	/**
	 * 返回编码器id
	 * @return
	 */
	CodecId getCodecId() const override{
		return CodecInvalid;
	}
private:
	_StrPrinter _printer;
};

} //namespace mediakit

#endif //RTSP_RTSP_H_
