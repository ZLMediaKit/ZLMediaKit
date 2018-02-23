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
#include "RtmpPusher.h"
#include "Rtmp/utils.h"
#include "Rtsp/Rtsp.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"

using namespace ZL::Util;

namespace ZL {
namespace Rtmp {

unordered_map<string, RtmpPusher::rtmpCMDHandle> RtmpPusher::g_mapCmd;
RtmpPusher::RtmpPusher(const char *strVhost,const char *strApp,const char *strStream) {
    auto src = dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA,strVhost,strApp,strStream));
    if (!src) {
        auto strErr = StrPrinter << "media source:"  << strVhost << "/" << strApp << "/" << strStream << "not found!" << endl;
        throw std::runtime_error(strErr);
    }
    init(src);
}
RtmpPusher::RtmpPusher(const RtmpMediaSource::Ptr &src){
    init(src);
}

void RtmpPusher::init(const RtmpMediaSource::Ptr  &src){
    static onceToken token([]() {
        g_mapCmd.emplace("_error",&RtmpPusher::onCmd_result);
        g_mapCmd.emplace("_result",&RtmpPusher::onCmd_result);
        g_mapCmd.emplace("onStatus",&RtmpPusher::onCmd_onStatus);
    }, []() {});
    m_pMediaSrc=src;
}

RtmpPusher::~RtmpPusher() {
	teardown();
	DebugL << endl;
}
void RtmpPusher::teardown() {
	if (alive()) {
		m_strApp.clear();
		m_strStream.clear();
		m_strTcUrl.clear();
		{
			lock_guard<recursive_mutex> lck(m_mtxOnResultCB);
			m_mapOnResultCB.clear();
		}
        {
            lock_guard<recursive_mutex> lck(m_mtxOnStatusCB);
            m_dqOnStatusCB.clear();
        }
		m_pPublishTimer.reset();
        reset();
        shutdown();
	}
}

void RtmpPusher::publish(const char* strUrl)  {
	teardown();
	string strHost = FindField(strUrl, "://", "/");
	m_strApp = 	FindField(strUrl, (strHost + "/").data(), "/");
    m_strStream = FindField(strUrl, (strHost + "/" + m_strApp + "/").data(), NULL);
    m_strTcUrl = string("rtmp://") + strHost + "/" + m_strApp;

    if (!m_strApp.size() || !m_strStream.size()) {
        onPublishResult(SockException(Err_other,"rtmp url非法"));
        return;
    }
	DebugL << strHost << " " << m_strApp << " " << m_strStream;

	auto iPort = atoi(FindField(strHost.c_str(), ":", NULL).c_str());
	if (iPort <= 0) {
        //rtmp 默认端口1935
		iPort = 1935;
	} else {
        //服务器域名
		strHost = FindField(strHost.c_str(), NULL, ":");
	}
	startConnect(strHost, iPort);
}

void RtmpPusher::onErr(const SockException &ex){
	onShutdown(ex);
}
void RtmpPusher::onConnect(const SockException &err){
	if(err.getErrCode()!=Err_success) {
		onPublishResult(err);
		return;
	}
	weak_ptr<RtmpPusher> weakSelf = dynamic_pointer_cast<RtmpPusher>(shared_from_this());
	m_pPublishTimer.reset( new Timer(10,  [weakSelf]() {
		auto strongSelf=weakSelf.lock();
		if(!strongSelf) {
			return false;
		}
		strongSelf->onPublishResult(SockException(Err_timeout,"publish rtmp timeout"));
		strongSelf->teardown();
		return false;
	}));
	startClientSession([weakSelf](){
        auto strongSelf=weakSelf.lock();
        if(!strongSelf) {
            return;
        }
        //strongSelf->sendChunkSize(60000);
        strongSelf->send_connect();
	});
}
void RtmpPusher::onRecv(const Buffer::Ptr &pBuf){
	try {
		onParseRtmp(pBuf->data(), pBuf->size());
	} catch (exception &e) {
		SockException ex(Err_other, e.what());
		onPublishResult(ex);
		onShutdown(ex);
		teardown();
	}
}


inline void RtmpPusher::send_connect() {
	AMFValue obj(AMF_OBJECT);
	obj.set("app", m_strApp);
	obj.set("type", "nonprivate");
	obj.set("tcUrl", m_strTcUrl);
	obj.set("swfUrl", m_strTcUrl);
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

inline void RtmpPusher::send_createStream() {
	AMFValue obj(AMF_NULL);
	sendInvoke("createStream", obj);
	addOnResultCB([this](AMFDecoder &dec){
		//TraceL << "createStream result";
		dec.load<AMFValue>();
		m_ui32StreamId = dec.load<int>();
		send_publish();
	});
}
inline void RtmpPusher::send_publish() {
	AMFEncoder enc;
	enc << "publish" << ++m_iReqID << nullptr << m_strStream << m_strApp ;
	sendRequest(MSG_CMD, enc.data());

	addOnStatusCB([this](AMFValue &val) {
		auto level = val["level"].as_string();
		auto code = val["code"].as_string();
		if(level != "status") {
			throw std::runtime_error(StrPrinter <<"publish 失败:" << level << " " << code << endl);
		}
		//start send media
		send_metaData();
	});
}

inline void RtmpPusher::send_metaData(){
    auto src = m_pMediaSrc.lock();
    if (!src) {
        throw std::runtime_error("the media source was released");
    }
    if (!src->ready()) {
        throw std::runtime_error("the media source is not ready");
    }
    
    AMFEncoder enc;
    enc << "@setDataFrame" << "onMetaData" <<  src->getMetaData();
    sendRequest(MSG_DATA, enc.data());
    
    src->getConfigFrame([&](const RtmpPacket::Ptr &pkt){
        sendRtmp(pkt->typeId, m_ui32StreamId, pkt->strBuf, pkt->timeStamp, pkt->chunkId , true);
    });
    
    m_pRtmpReader = src->getRing()->attach();
    weak_ptr<RtmpPusher> weakSelf = dynamic_pointer_cast<RtmpPusher>(shared_from_this());
    m_pRtmpReader->setReadCB([weakSelf](const RtmpPacket::Ptr &pkt){
    	auto strongSelf = weakSelf.lock();
    	if(!strongSelf) {
    		return;
    	}
    	strongSelf->sendRtmp(pkt->typeId, strongSelf->m_ui32StreamId, pkt->strBuf, pkt->timeStamp, pkt->chunkId , true);
    });
    m_pRtmpReader->setDetachCB([weakSelf](){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->onShutdown(SockException(Err_other,"媒体源被释放"));
            strongSelf->teardown();
        }
    });
    onPublishResult(SockException(Err_success,"success"));
}
void RtmpPusher::onCmd_result(AMFDecoder &dec){
	auto iReqId = dec.load<int>();
	lock_guard<recursive_mutex> lck(m_mtxOnResultCB);
	auto it = m_mapOnResultCB.find(iReqId);
	if(it != m_mapOnResultCB.end()){
		it->second(dec);
		m_mapOnResultCB.erase(it);
	}else{
		WarnL << "unhandled _result";
	}
}
void RtmpPusher::onCmd_onStatus(AMFDecoder &dec) {
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

    lock_guard<recursive_mutex> lck(m_mtxOnStatusCB);
	if(m_dqOnStatusCB.size()){
		m_dqOnStatusCB.front()(val);
		m_dqOnStatusCB.pop_front();
	}else{
		auto level = val["level"];
		auto code = val["code"].as_string();
		if(level.type() == AMF_STRING){
			if(level.as_string() != "status"){
				throw std::runtime_error(StrPrinter <<"onStatus 失败:" << level.as_string() << " " << code << endl);
			}
		}
    }
}

void RtmpPusher::onRtmpChunk(RtmpPacket &chunkData) {
	switch (chunkData.typeId) {
		case MSG_CMD:
		case MSG_CMD3: {
			AMFDecoder dec(chunkData.strBuf, 0);
			std::string type = dec.load<std::string>();
			auto it = g_mapCmd.find(type);
			if(it != g_mapCmd.end()){
				auto fun = it->second;
				(this->*fun)(dec);
			}else{
				WarnL << "can not support cmd:" << type;
			}
		}
			break;
		default:
			//WarnL << "unhandled message:" << (int) chunkData.typeId << hexdump(chunkData.strBuf.data(), chunkData.strBuf.size());
			break;
		}
}


} /* namespace Rtmp */
} /* namespace ZL */

