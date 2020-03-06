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

#include "RtmpPlayer.h"
#include "Rtmp/utils.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"
using namespace toolkit;
using namespace mediakit::Client;

namespace mediakit {

RtmpPlayer::RtmpPlayer(const EventPoller::Ptr &poller) : TcpClient(poller) {
}

RtmpPlayer::~RtmpPlayer() {
	DebugL << endl;
}

void RtmpPlayer::teardown() {
	if (alive()) {
		shutdown(SockException(Err_shutdown,"teardown"));
	}
	_strApp.clear();
	_strStream.clear();
	_strTcUrl.clear();
	_pBeatTimer.reset();
	_pPlayTimer.reset();
	_pMediaTimer.reset();
	_iSeekTo = 0;
	RtmpProtocol::reset();

	CLEAR_ARR(_aiFistStamp);
	CLEAR_ARR(_aiNowStamp);

	lock_guard<recursive_mutex> lck(_mtxOnResultCB);
	_mapOnResultCB.clear();
	lock_guard<recursive_mutex> lck2(_mtxOnStatusCB);
	_dqOnStatusCB.clear();
}

void RtmpPlayer::play(const string &strUrl)  {
	teardown();
	string strHost = FindField(strUrl.data(), "://", "/");
	_strApp = 	FindField(strUrl.data(), (strHost + "/").data(), "/");
    _strStream = FindField(strUrl.data(), (strHost + "/" + _strApp + "/").data(), NULL);
    _strTcUrl = string("rtmp://") + strHost + "/" + _strApp;

    if (!_strApp.size() || !_strStream.size()) {
        onPlayResult_l(SockException(Err_other,"rtmp url非法"),false);
        return;
    }
	DebugL << strHost << " " << _strApp << " " << _strStream;

	auto iPort = atoi(FindField(strHost.data(), ":", NULL).data());
	if (iPort <= 0) {
        //rtmp 默认端口1935
		iPort = 1935;
	} else {
        //服务器域名
		strHost = FindField(strHost.data(), NULL, ":");
	}
	if(!(*this)[kNetAdapter].empty()){
		setNetAdapter((*this)[kNetAdapter]);
	}

	weak_ptr<RtmpPlayer> weakSelf= dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
	float playTimeOutSec = (*this)[kTimeoutMS].as<int>() / 1000.0;
	_pPlayTimer.reset( new Timer(playTimeOutSec, [weakSelf]() {
		auto strongSelf=weakSelf.lock();
		if(!strongSelf) {
			return false;
		}
		strongSelf->onPlayResult_l(SockException(Err_timeout,"play rtmp timeout"),false);
		return false;
	},getPoller()));

	_metadata_got = false;
	startConnect(strHost, iPort , playTimeOutSec);
}
void RtmpPlayer::onErr(const SockException &ex){
    //定时器_pPlayTimer为空后表明握手结束了
	onPlayResult_l(ex, !_pPlayTimer);
}

void RtmpPlayer::onPlayResult_l(const SockException &ex , bool handshakeCompleted) {
	WarnL << ex.getErrCode() << " " << ex.what();

    if(!ex){
        //播放成功，恢复rtmp接收超时定时器
        _mediaTicker.resetTime();
        weak_ptr<RtmpPlayer> weakSelf = dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
        int timeoutMS = (*this)[kMediaTimeoutMS].as<int>();
		//创建rtmp数据接收超时检测定时器
        _pMediaTimer.reset( new Timer(timeoutMS / 2000.0, [weakSelf,timeoutMS]() {
            auto strongSelf=weakSelf.lock();
            if(!strongSelf) {
                return false;
            }
            if(strongSelf->_mediaTicker.elapsedTime()> timeoutMS) {
                //接收rtmp媒体数据超时
                strongSelf->onPlayResult_l(SockException(Err_timeout,"receive rtmp timeout"),true);
                return false;
            }
            return true;
        },getPoller()));
    }

	if (!handshakeCompleted) {
	    //开始播放阶段
		_pPlayTimer.reset();
		onPlayResult(ex);
	} else if (ex) {
		//播放成功后异常断开回调
		onShutdown(ex);
	} else {
		//恢复播放
		onResume();
	}

	if(ex){
		teardown();
	}
}
void RtmpPlayer::onConnect(const SockException &err){
	if(err.getErrCode() != Err_success) {
		onPlayResult_l(err, false);
		return;
	}
	weak_ptr<RtmpPlayer> weakSelf= dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
	startClientSession([weakSelf](){
        auto strongSelf=weakSelf.lock();
		if(!strongSelf) {
            return;
        }
		strongSelf->send_connect();
	});
}
void RtmpPlayer::onRecv(const Buffer::Ptr &pBuf){
	try {
		onParseRtmp(pBuf->data(), pBuf->size());
	} catch (exception &e) {
		SockException ex(Err_other, e.what());
        //定时器_pPlayTimer为空后表明握手结束了
		onPlayResult_l(ex, !_pPlayTimer);
	}
}

void RtmpPlayer::pause(bool bPause) {
	send_pause(bPause);
}

inline void RtmpPlayer::send_connect() {
	AMFValue obj(AMF_OBJECT);
	obj.set("app", _strApp);
	obj.set("tcUrl", _strTcUrl);
	//未使用代理
	obj.set("fpad", false);
	//参考librtmp,什么作用?
	obj.set("capabilities", 15);
	//SUPPORT_VID_CLIENT_SEEK 支持seek
	obj.set("videoFunction", 1);
    //只支持aac
    obj.set("audioCodecs", (double)(0x0400));
    //只支持H264
    obj.set("videoCodecs", (double)(0x0080));
	sendInvoke("connect", obj);
	addOnResultCB([this](AMFDecoder &dec){
		//TraceL << "connect result";
		dec.load<AMFValue>();
		auto val = dec.load<AMFValue>();
		auto level = val["level"].as_string();
		auto code = val["code"].as_string();
		if(level != "status"){
			throw std::runtime_error(StrPrinter <<"connect 失败:" << level << " " << code << endl);
		}
		send_createStream();
	});
}

inline void RtmpPlayer::send_createStream() {
	AMFValue obj(AMF_NULL);
	sendInvoke("createStream", obj);
	addOnResultCB([this](AMFDecoder &dec){
		//TraceL << "createStream result";
		dec.load<AMFValue>();
		_ui32StreamId = dec.load<int>();
		send_play();
	});
}

inline void RtmpPlayer::send_play() {
	AMFEncoder enc;
	enc << "play" << ++_iReqID  << nullptr << _strStream << (double)_ui32StreamId;
	sendRequest(MSG_CMD, enc.data());
	auto fun = [this](AMFValue &val){
		//TraceL << "play onStatus";
		auto level = val["level"].as_string();
		auto code = val["code"].as_string();
		if(level != "status"){
			throw std::runtime_error(StrPrinter <<"play 失败:" << level << " " << code << endl);
		}
	};
	addOnStatusCB(fun);
	addOnStatusCB(fun);
}

inline void RtmpPlayer::send_pause(bool bPause) {
	AMFEncoder enc;
	enc << "pause" << ++_iReqID  << nullptr << bPause;
	sendRequest(MSG_CMD, enc.data());
	auto fun = [this,bPause](AMFValue &val){
        //TraceL << "pause onStatus";
        auto level = val["level"].as_string();
        auto code = val["code"].as_string();
        if(level != "status") {
            if(!bPause){
                throw std::runtime_error(StrPrinter <<"pause 恢复播放失败:" << level << " " << code << endl);
            }
        }else{
            _bPaused = bPause;
            if(!bPause){
                onPlayResult_l(SockException(Err_success, "resum rtmp success"), true);
            }else{
                //暂停播放
                _pMediaTimer.reset();
            }
        }
	};
	addOnStatusCB(fun);

	_pBeatTimer.reset();
	if(bPause){
		weak_ptr<RtmpPlayer> weakSelf = dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
		_pBeatTimer.reset(new Timer((*this)[kBeatIntervalMS].as<int>() / 1000.0,[weakSelf](){
			auto strongSelf = weakSelf.lock();
			if (!strongSelf){
				return false;
			}
			uint32_t timeStamp = ::time(NULL);
			strongSelf->sendUserControl(CONTROL_PING_REQUEST, timeStamp);
			return true;
		},getPoller()));
	}
}

void RtmpPlayer::onCmd_result(AMFDecoder &dec){
	auto iReqId = dec.load<int>();
	lock_guard<recursive_mutex> lck(_mtxOnResultCB);
	auto it = _mapOnResultCB.find(iReqId);
	if(it != _mapOnResultCB.end()){
		it->second(dec);
		_mapOnResultCB.erase(it);
	}else{
		WarnL << "unhandled _result";
	}
}
void RtmpPlayer::onCmd_onStatus(AMFDecoder &dec) {
	AMFValue val;
	while(true){
		val = dec.load<AMFValue>();
		if(val.type() == AMF_OBJECT){
			break;
		}
	}
	if(val.type() != AMF_OBJECT){
		throw std::runtime_error("onStatus:the result object was not found");
	}
    
    lock_guard<recursive_mutex> lck(_mtxOnStatusCB);
	if(_dqOnStatusCB.size()){
		_dqOnStatusCB.front()(val);
		_dqOnStatusCB.pop_front();
	}else{
		auto level = val["level"];
		auto code = val["code"].as_string();
		if(level.type() == AMF_STRING){
			if(level.as_string() != "status"){
				throw std::runtime_error(StrPrinter <<"onStatus 失败:" << level.as_string() << " " << code << endl);
			}
		}
		//WarnL << "unhandled onStatus:" << code;
    }
}

void RtmpPlayer::onCmd_onMetaData(AMFDecoder &dec) {
	//TraceL;
	auto val = dec.load<AMFValue>();
	if(!onCheckMeta(val)){
		throw std::runtime_error("onCheckMeta failed");
	}
	_metadata_got = true;
}

void RtmpPlayer::onStreamDry(uint32_t ui32StreamId) {
	//TraceL << ui32StreamId;
	onPlayResult_l(SockException(Err_other,"rtmp stream dry"), true);
}

void RtmpPlayer::onMediaData_l(const RtmpPacket::Ptr &packet) {
	_mediaTicker.resetTime();
	if(!_pPlayTimer){
		//已经触发了onPlayResult事件，直接触发onMediaData事件
		onMediaData(packet);
		return;
	}

	if(packet->isCfgFrame()){
		//输入配置帧以便初始化完成各个track
		onMediaData(packet);
	}else{
		//先触发onPlayResult事件，这个时候解码器才能初始化完毕
		onPlayResult_l(SockException(Err_success,"play rtmp success"), false);
		//触发onPlayResult事件后，再把帧数据输入到解码器
		onMediaData(packet);
	}
}


void RtmpPlayer::onRtmpChunk(RtmpPacket &chunkData) {
	typedef void (RtmpPlayer::*rtmp_func_ptr)(AMFDecoder &dec);
	static unordered_map<string, rtmp_func_ptr> s_func_map;
	static onceToken token([]() {
		s_func_map.emplace("_error",&RtmpPlayer::onCmd_result);
		s_func_map.emplace("_result",&RtmpPlayer::onCmd_result);
		s_func_map.emplace("onStatus",&RtmpPlayer::onCmd_onStatus);
		s_func_map.emplace("onMetaData",&RtmpPlayer::onCmd_onMetaData);
	}, []() {});

	switch (chunkData.typeId) {
		case MSG_CMD:
		case MSG_CMD3:
		case MSG_DATA:
		case MSG_DATA3: {
			AMFDecoder dec(chunkData.strBuf, 0);
			std::string type = dec.load<std::string>();
			auto it = s_func_map.find(type);
			if(it != s_func_map.end()){
				auto fun = it->second;
				(this->*fun)(dec);
			}else{
				WarnL << "can not support cmd:" << type;
			}
		}
			break;
		case MSG_AUDIO:
		case MSG_VIDEO: {
            auto idx = chunkData.typeId%2;
            if (_aNowStampTicker[idx].elapsedTime() > 500) {
				//计算播放进度时间轴用
                _aiNowStamp[idx] = chunkData.timeStamp;
            }
			if(!_metadata_got){
				if(!onCheckMeta(TitleMeta().getMetadata())){
					throw std::runtime_error("onCheckMeta failed");
				}
				_metadata_got = true;
			}
			onMediaData_l(std::make_shared<RtmpPacket>(std::move(chunkData)));
		}
			break;
		default:
			//WarnL << "unhandled message:" << (int) chunkData.typeId << hexdump(chunkData.strBuf.data(), chunkData.strBuf.size());
			break;
		}
}

uint32_t RtmpPlayer::getProgressMilliSecond() const{
	uint32_t iTime[2] = {0,0};
    for(auto i = 0 ;i < 2 ;i++){
        iTime[i] = _aiNowStamp[i] - _aiFistStamp[i];
    }
    return _iSeekTo + MAX(iTime[0],iTime[1]);
}
void RtmpPlayer::seekToMilliSecond(uint32_t seekMS){
    if (_bPaused) {
        pause(false);
    }
    AMFEncoder enc;
    enc << "seek" << ++_iReqID << nullptr << seekMS * 1.0;
    sendRequest(MSG_CMD, enc.data());
    addOnStatusCB([this,seekMS](AMFValue &val) {
        //TraceL << "seek result";
        _aNowStampTicker[0].resetTime();
        _aNowStampTicker[1].resetTime();
		int iTimeInc = seekMS - getProgressMilliSecond();
        for(auto i = 0 ;i < 2 ;i++){
            _aiFistStamp[i] = _aiNowStamp[i] + iTimeInc;
            _aiNowStamp[i] = _aiFistStamp[i];
        }
        _iSeekTo = seekMS;
    });

}

} /* namespace mediakit */

