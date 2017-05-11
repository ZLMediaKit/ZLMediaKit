/*
 * RtmpSession.cpp
 *
 *  Created on: 2017年2月10日
 *      Author: xzl
 */


#include "RtmpSession.h"
#include "Util/onceToken.h"

namespace ZL {
namespace Rtmp {

unordered_map<string, RtmpSession::rtmpCMDHandle> RtmpSession::g_mapCmd;
RtmpSession::RtmpSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock) :
		TcpLimitedSession(pTh, pSock) {
	static onceToken token([]() {
		g_mapCmd.emplace("connect",&RtmpSession::onCmd_connect);
		g_mapCmd.emplace("createStream",&RtmpSession::onCmd_createStream);
		g_mapCmd.emplace("publish",&RtmpSession::onCmd_publish);
		g_mapCmd.emplace("deleteStream",&RtmpSession::onCmd_deleteStream);
		g_mapCmd.emplace("play",&RtmpSession::onCmd_play);
		g_mapCmd.emplace("seek",&RtmpSession::onCmd_seek);
		g_mapCmd.emplace("pause",&RtmpSession::onCmd_pause);}, []() {});
	DebugL << getPeerIp();
}

RtmpSession::~RtmpSession() {
	DebugL << getPeerIp();
}

void RtmpSession::onError(const SockException& err) {
	DebugL << err.what();
	if (m_pPublisherSrc) {
		m_pPublisherSrc.reset();
	}
}

void RtmpSession::onManager() {
	if (m_ticker.createdTime() > 10 * 1000) {
		if (!m_pRingReader && !m_pPublisherSrc) {
			WarnL << "非法链接:" << getPeerIp();
			shutdown();
		}
	}
	if (m_pPublisherSrc) {
		//publisher
		if (m_ticker.elapsedTime() > 10 * 1000) {
			WarnL << "数据接收超时:" << getPeerIp();
			shutdown();
		}
	}
}

void RtmpSession::onRecv(const Socket::Buffer::Ptr &pBuf) {
	m_ticker.resetTime();
	try {
		onParseRtmp(pBuf->data(), pBuf->size());
	} catch (exception &e) {
		WarnL << e.what();
		shutdown();
	}
}

void RtmpSession::onCmd_connect(AMFDecoder &dec) {
	auto params = dec.load<AMFValue>();
	m_strApp = params["app"].as_string();
	bool ok = true; //(app == APP_NAME);
	AMFValue version(AMF_OBJECT);
	version.set("fmsVer", "FMS/3,5,3,888");
	version.set("capabilities", 127.0);
	version.set("mode", 1.0);
	AMFValue status(AMF_OBJECT);
	status.set("level", ok ? "status" : "error");
	status.set("code", ok ? "NetConnection.Connect.Success" : "NetConnection.Connect.InvalidApp");
	status.set("description", ok ? "Connection succeeded." : "InvalidApp.");
	status.set("objectEncoding", (double) (dec.getVersion()));
	sendReply(ok ? "_result" : "_error", version, status);
	if (!ok) {
		throw std::runtime_error("Unsupported application: " + m_strApp);
	}

	////////////window Acknowledgement size/////
	sendAcknowledgementSize(2500000);
	///////////set peerBandwidth////////////////
	sendPeerBandwidth(2500000);
	///////////set chunk size////////////////
#ifndef _DEBUG
	sendChunkSize(60000);
#endif
}

void RtmpSession::onCmd_createStream(AMFDecoder &dec) {
	sendReply("_result", nullptr, double(STREAM_MEDIA));
}

void RtmpSession::onCmd_publish(AMFDecoder &dec) {
	dec.load<AMFValue>();/* NULL */
	m_strId = dec.load<std::string>();
	auto iPos = m_strId.find('?');
	if (iPos != string::npos) {
		m_strId.erase(iPos);
	}
	auto src = RtmpMediaSource::find(m_strApp,m_strId,false);
	bool ok = (!src && !m_pPublisherSrc);
	AMFValue status(AMF_OBJECT);
	status.set("level", ok ? "status" : "error");
	status.set("code", ok ? "NetStream.Publish.Start" : "NetStream.Publish.BadName");
	status.set("description", ok ? "Started publishing stream." : "Already publishing.");
	status.set("clientid", "ASAICiss");
	sendReply("onStatus", nullptr, status);
	if (!ok) {
		throw std::runtime_error( StrPrinter << "Already publishing:" << m_strApp << "/" << m_strId << endl);
	}
	m_bPublisherSrcRegisted = false;
	m_pPublisherSrc.reset(new RtmpToRtspMediaSource(m_strApp,m_strId));
}

void RtmpSession::onCmd_deleteStream(AMFDecoder &dec) {
	AMFValue status(AMF_OBJECT);
	status.set("level", "status");
	status.set("code", "NetStream.Unpublish.Success");
	status.set("description", "Stop publishing.");
	sendReply("onStatus", nullptr, status);
	throw std::runtime_error(StrPrinter << "Stop publishing." << endl);
}

void RtmpSession::onCmd_play(AMFDecoder &dec) {
	dec.load<AMFValue>();/* NULL */
	m_strId = dec.load<std::string>();
	auto iPos = m_strId.find('?');
	if (iPos != string::npos) {
		m_strId.erase(iPos);
	}
	auto src = RtmpMediaSource::find(m_strApp,m_strId,true);
	bool ok = (src.operator bool());
	ok = ok && src->ready();
// onStatus(NetStream.Play.Reset)
	AMFValue status(AMF_OBJECT);

	status.set("level", ok ? "status" : "error");
	status.set("code", ok ? "NetStream.Play.Reset" : "NetStream.Play.StreamNotFound");
	status.set("description", ok ? "Resetting and playing stream." : "No such stream.");
	status.set("details", "stream");
	status.set("clientid", "ASAICiss");
	sendReply("onStatus", nullptr, status);
	if (!ok) {
		throw std::runtime_error( StrPrinter << "No such stream:" << m_strApp << " " << m_strId << endl);
	}
//stream begin
	sendUserControl(CONTROL_STREAM_BEGIN, STREAM_MEDIA);
// onStatus(NetStream.Play.Start)
	status.clear();
	status.set("level", "status");
	status.set("code", "NetStream.Play.Start");
	status.set("description", "Started playing stream.");
	status.set("details", "stream");
	status.set("clientid", "ASAICiss");
	sendReply("onStatus", AMFValue(), status);

// |RtmpSampleAccess(true, true)
	AMFEncoder invoke;
	invoke << "|RtmpSampleAccess" << true << true;
	sendResponse(MSG_DATA, invoke.data());

// onMetaData
	invoke.clear();
	invoke << "onMetaData" << src->getMetaData();
	sendResponse(MSG_DATA, invoke.data());

	src->getConfigFrame([&](const RtmpPacket &pkt) {
		//DebugL<<"send initial frame";
        onSendMedia(pkt);
    });

	m_pRingReader = src->getRing()->attach();
	weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
	m_pRingReader->setReadCB([weakSelf](const RtmpPacket& pkt){
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		strongSelf->async([weakSelf,pkt]() {
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->onSendMedia(pkt);
		});
	});
	m_pRingReader->setDetachCB([weakSelf]() {
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		strongSelf->safeShutdown();
	});
    m_pPlayerSrc = src;
    if(src->getRing()->readerCount() == 1){
        src->seekTo(0);
    }
}

void RtmpSession::onCmd_pause(AMFDecoder &dec) {
	dec.load<AMFValue>();/* NULL */
	bool paused = dec.load<bool>();
	TraceL << paused;
	AMFValue status(AMF_OBJECT);
	status.set("level", "status");
	status.set("code", paused ? "NetStream.Pause.Notify" : "NetStream.Unpause.Notify");
	status.set("description", paused ? "Paused stream." : "Unpaused stream.");
	sendReply("onStatus", nullptr, status);
//streamBegin
	sendUserControl(paused ? CONTROL_STREAM_EOF : CONTROL_STREAM_BEGIN,
	STREAM_MEDIA);
	if (!m_pRingReader) {
		throw std::runtime_error("Rtmp not started yet!");
	}
	if (paused) {
		m_pRingReader->setReadCB(nullptr);
	} else {
		weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
		m_pRingReader->setReadCB([weakSelf](const RtmpPacket& pkt) {
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->async([weakSelf,pkt]() {
				auto strongSelf = weakSelf.lock();
				if(!strongSelf) {
					return;
				}
				strongSelf->onSendMedia(pkt);
			});
		});
	}
}

void RtmpSession::setMetaData(AMFDecoder &dec) {
	if (!m_pPublisherSrc) {
		throw std::runtime_error("not a publisher");
	}
	std::string type = dec.load<std::string>();
	if (type != "onMetaData") {
		throw std::runtime_error("can only set metadata");
	}
	m_pPublisherSrc->onGetMetaData(dec.load<AMFValue>());
}

void RtmpSession::onProcessCmd(AMFDecoder &dec) {
	std::string method = dec.load<std::string>();
	auto it = g_mapCmd.find(method);
	if (it == g_mapCmd.end()) {
		TraceL << "can not support cmd:" << method;
		return;
	}
	m_dNowReqID = dec.load<double>();
	auto fun = it->second;
	(this->*fun)(dec);
}

void RtmpSession::onRtmpChunk(RtmpPacket &chunkData) {
	switch (chunkData.typeId) {
	case MSG_CMD:
	case MSG_CMD3: {
		AMFDecoder dec(chunkData.strBuf, 0);
		onProcessCmd(dec);
	}
		break;

	case MSG_DATA:
	case MSG_DATA3: {
		AMFDecoder dec(chunkData.strBuf, 0);
		std::string type = dec.load<std::string>();
		TraceL << "notify:" << type;
		if (type == "@setDataFrame") {
			setMetaData(dec);
		}
	}
		break;
	case MSG_AUDIO:
	case MSG_VIDEO: {
		if (!m_pPublisherSrc) {
			throw std::runtime_error("Not a rtmp publisher!");
		}
		m_pPublisherSrc->onGetMedia(chunkData);
		if(!m_bPublisherSrcRegisted && m_pPublisherSrc->ready()){
			m_bPublisherSrcRegisted = true;
			m_pPublisherSrc->regist();
		}
	}
		break;
	default:
		WarnL << "unhandled message:" << (int) chunkData.typeId << hexdump(chunkData.strBuf.data(), chunkData.strBuf.size());
		break;
	}
}

void RtmpSession::onCmd_seek(AMFDecoder &dec) {
    dec.load<AMFValue>();/* NULL */
    auto milliSeconds = dec.load<AMFValue>().as_number();
    InfoL << "rtmp seekTo:" << milliSeconds/1000.0;
    auto stongSrc = m_pPlayerSrc.lock();
    if (stongSrc) {
        stongSrc->seekTo(milliSeconds);
    }
	AMFValue status(AMF_OBJECT);
	AMFEncoder invoke;
	status.set("level", "status");
	status.set("code", "NetStream.Seek.Notify");
	status.set("description", "Seeking.");
	sendReply("onStatus", nullptr, status);
}

void RtmpSession::onSendMedia(const RtmpPacket& pkt) {
	sendRtmp(pkt.typeId, pkt.streamId, pkt.strBuf, pkt.timeStamp, pkt.chunkId);
}

} /* namespace Rtmp */
} /* namespace ZL */
