/*
 * RtmpPusher.cpp
 *
 *  Created on: 2017年2月13日
 *      Author: xzl
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
RtmpPusher::RtmpPusher(const char *strApp,const char *strStream) {
	static onceToken token([]() {
		g_mapCmd.emplace("_error",&RtmpPusher::onCmd_result);
		g_mapCmd.emplace("_result",&RtmpPusher::onCmd_result);
		g_mapCmd.emplace("onStatus",&RtmpPusher::onCmd_onStatus);
		}, []() {});
    auto src = RtmpMediaSource::find(strApp,strStream);
    if (!src) {
        auto strErr = StrPrinter << "媒体源：" << strApp << "/" << strStream << "不存在" << endl;
        throw std::runtime_error(strErr);
    }
    m_pMediaSrc = src;
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
		m_mapOnResultCB.clear();
        {
            lock_guard<recursive_mutex> lck(m_mtxDeque);
            m_dqOnStatusCB.clear();
        }
		m_pPlayTimer.reset();
        clear();
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
        _onPlayResult(SockException(Err_other,"rtmp url非法"));
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
	_onShutdown(ex);
}
void RtmpPusher::onConnect(const SockException &err){
	if(err.getErrCode()!=Err_success) {
		_onPlayResult(err);
		return;
	}
	weak_ptr<RtmpPusher> weakSelf = dynamic_pointer_cast<RtmpPusher>(shared_from_this());
	m_pPlayTimer.reset( new Timer(10,  [weakSelf]() {
		auto strongSelf=weakSelf.lock();
		if(!strongSelf) {
			return false;
		}
		strongSelf->_onPlayResult(SockException(Err_timeout,"publish rtmp timeout"));
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
void RtmpPusher::onRecv(const Socket::Buffer::Ptr &pBuf){
	try {
		onParseRtmp(pBuf->data(), pBuf->size());
	} catch (exception &e) {
		SockException ex(Err_other, e.what());
		_onPlayResult(ex);
		_onShutdown(ex);
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
			throw std::runtime_error(StrPrinter <<"connect 失败：" << level << " " << code << endl);
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
			throw std::runtime_error(StrPrinter <<"publish 失败：" << level << " " << code << endl);
		}
		//start send media
		send_metaData();
	});
}

inline void RtmpPusher::send_metaData(){
    auto src = m_pMediaSrc.lock();
    if (!src) {
        throw std::runtime_error("媒体源已被释放");
    }
    if (!src->ready()) {
        throw std::runtime_error("媒体源尚未准备就绪");
    }
    
    AMFEncoder enc;
    enc << "@setDataFrame" << "onMetaData" <<  src->getMetaData();
    sendRequest(MSG_DATA, enc.data());
    
    src->getConfigFrame([&](const RtmpPacket &pkt){
        sendRtmp(pkt.typeId, m_ui32StreamId, pkt.strBuf, pkt.timeStamp, pkt.chunkId);
    });
    
    m_pRtmpReader = src->getRing()->attach();
    weak_ptr<RtmpPusher> weakSelf = dynamic_pointer_cast<RtmpPusher>(shared_from_this());
    m_pRtmpReader->setReadCB([weakSelf](const RtmpPacket &pkt){
    	auto strongSelf = weakSelf.lock();
    	if(!strongSelf) {
    		return;
    	}
    	strongSelf->sendRtmp(pkt.typeId, strongSelf->m_ui32StreamId, pkt.strBuf, pkt.timeStamp, pkt.chunkId);
    });
    m_pRtmpReader->setDetachCB([weakSelf](){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->_onShutdown(SockException(Err_other,"媒体源被释放"));
            strongSelf->teardown();
        }
    });
    _onPlayResult(SockException(Err_success,"success"));
}
void RtmpPusher::onCmd_result(AMFDecoder &dec){
	auto iReqId = dec.load<int>();
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
		throw std::runtime_error("onStatus: 未找到结果对象");
	}

    lock_guard<recursive_mutex> lck(m_mtxDeque);
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

