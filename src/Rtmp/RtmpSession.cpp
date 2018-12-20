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


#include "RtmpSession.h"
#include "Common/config.h"
#include "Util/onceToken.h"

namespace mediakit {

static int kSockFlags = SOCKET_DEFAULE_FLAGS | FLAG_MORE;

RtmpSession::RtmpSession(const Socket::Ptr &pSock) : TcpSession(pSock) {
	DebugL << get_peer_ip();
	//设置10秒发送缓存
    pSock->setSendBufSecond(10);
    //设置15秒发送超时时间
    pSock->setSendTimeOutSecond(15);
}

RtmpSession::~RtmpSession() {
    DebugL << get_peer_ip();
    if(_delayTask){
        _delayTask();
        _delayTask = nullptr;
    }
}

void RtmpSession::onError(const SockException& err) {
	DebugL << err.what();

    //流量统计事件广播
    GET_CONFIG_AND_REGISTER(uint32_t,iFlowThreshold,Broadcast::kFlowThreshold);

    if(_ui64TotalBytes > iFlowThreshold * 1024){
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport,
                                           _mediaInfo,
                                           _ui64TotalBytes,
                                           _ticker.createdTime()/1000,
                                           *this);
    }
}

void RtmpSession::onManager() {
	if (_ticker.createdTime() > 15 * 1000) {
		if (!_pRingReader && !_pPublisherSrc) {
			WarnL << "非法链接:" << get_peer_ip();
			shutdown();
		}
	}
	if (_pPublisherSrc) {
		//publisher
		if (_ticker.elapsedTime() > 15 * 1000) {
			WarnL << "数据接收超时:" << get_peer_ip();
			shutdown();
		}
	}
    if(_delayTask){
        if(time(NULL) > _iTaskTimeLine){
            _delayTask();
            _delayTask = nullptr;
        }
    }
}

void RtmpSession::onRecv(const Buffer::Ptr &pBuf) {
	_ticker.resetTime();
	try {
        _ui64TotalBytes += pBuf->size();
		onParseRtmp(pBuf->data(), pBuf->size());
	} catch (exception &e) {
		WarnL << e.what();
		shutdown();
	}
}

void RtmpSession::onCmd_connect(AMFDecoder &dec) {
	auto params = dec.load<AMFValue>();
	double amfVer = 0;
	AMFValue objectEncoding = params["objectEncoding"];
	if(objectEncoding){
		amfVer = objectEncoding.as_number();
	}
	///////////set chunk size////////////////
	sendChunkSize(60000);
	////////////window Acknowledgement size/////
	sendAcknowledgementSize(5000000);
	///////////set peerBandwidth////////////////
	sendPeerBandwidth(5000000);

    _mediaInfo._app = params["app"].as_string();
    _strTcUrl = params["tcUrl"].as_string();
    if(_strTcUrl.empty()){
        //defaultVhost:默认vhost
        _strTcUrl = string(RTMP_SCHEMA) + "://" + DEFAULT_VHOST + "/" + _mediaInfo._app;
    }
	bool ok = true; //(app == APP_NAME);
	AMFValue version(AMF_OBJECT);
	version.set("fmsVer", "FMS/3,0,1,123");
	version.set("capabilities", 31.0);
	AMFValue status(AMF_OBJECT);
	status.set("level", ok ? "status" : "error");
	status.set("code", ok ? "NetConnection.Connect.Success" : "NetConnection.Connect.InvalidApp");
	status.set("description", ok ? "Connection succeeded." : "InvalidApp.");
	status.set("objectEncoding", amfVer);
	sendReply(ok ? "_result" : "_error", version, status);
	if (!ok) {
		throw std::runtime_error("Unsupported application: " + _mediaInfo._app);
	}

	AMFEncoder invoke;
	invoke << "onBWDone" << 0.0 << nullptr;
	sendResponse(MSG_CMD, invoke.data());
}

void RtmpSession::onCmd_createStream(AMFDecoder &dec) {
	sendReply("_result", nullptr, double(STREAM_MEDIA));
}

void RtmpSession::onCmd_publish(AMFDecoder &dec) {
    std::shared_ptr<Ticker> pTicker(new Ticker);
    std::shared_ptr<onceToken> pToken(new onceToken(nullptr,[pTicker](){
        DebugL << "publish 回复时间:" << pTicker->elapsedTime() << "ms";
    }));
	dec.load<AMFValue>();/* NULL */
    _mediaInfo.parse(_strTcUrl + "/" + dec.load<std::string>());

    auto onRes = [this,pToken](const string &err){
        auto src = dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA,
                                                                           _mediaInfo._vhost,
                                                                           _mediaInfo._app,
                                                                           _mediaInfo._streamid,
                                                                           false));
        bool authSuccess = err.empty();
        bool ok = (!src && !_pPublisherSrc && authSuccess);
        AMFValue status(AMF_OBJECT);
        status.set("level", ok ? "status" : "error");
        status.set("code", ok ? "NetStream.Publish.Start" : (authSuccess ? "NetStream.Publish.BadName" : "NetStream.Publish.BadAuth"));
        status.set("description", ok ? "Started publishing stream." : (authSuccess ? "Already publishing." : err.data()));
        status.set("clientid", "0");
        sendReply("onStatus", nullptr, status);
        if (!ok) {
            WarnL << "onPublish:"
                  << (authSuccess ? "Already publishing:" : err.data()) << " "
                  << _mediaInfo._vhost << " "
                  << _mediaInfo._app << " "
                  << _mediaInfo._streamid << endl;
            shutdown();
            return;
        }
        _pPublisherSrc.reset(new RtmpToRtspMediaSource(_mediaInfo._vhost,_mediaInfo._app,_mediaInfo._streamid));
        _pPublisherSrc->setListener(dynamic_pointer_cast<MediaSourceEvent>(shared_from_this()));
    };

    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    Broadcast::AuthInvoker invoker = [weakSelf,onRes,pToken](const string &err){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        strongSelf->async([weakSelf,onRes,err,pToken](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                return;
            }
            onRes(err);
        });
    };
    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPublish,
                                                   _mediaInfo,
                                                   invoker,
                                                   *this);
    if(!flag){
        //该事件无人监听，默认鉴权成功
        onRes("");
    }
}

void RtmpSession::onCmd_deleteStream(AMFDecoder &dec) {
	AMFValue status(AMF_OBJECT);
	status.set("level", "status");
	status.set("code", "NetStream.Unpublish.Success");
	status.set("description", "Stop publishing.");
	sendReply("onStatus", nullptr, status);
	throw std::runtime_error(StrPrinter << "Stop publishing." << endl);
}

void RtmpSession::findStream(const function<void(const RtmpMediaSource::Ptr &src)> &cb,bool retry) {
    auto src = dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA,
                                                                       _mediaInfo._vhost,
                                                                       _mediaInfo._app,
                                                                       _mediaInfo._streamid,
                                                                       true));
    if(src || !retry){
        cb(src);
        return;
    }

    //广播未找到流
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastNotFoundStream,_mediaInfo,*this);

    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    auto task_id = this;
    auto media_info = _mediaInfo;

    auto onRegist = [task_id, weakSelf, media_info, cb](BroadcastMediaChangedArgs) {
        if(bRegist &&
           schema == media_info._schema &&
           vhost == media_info._vhost &&
           app == media_info._app &&
           stream == media_info._streamid){
            //播发器请求的rtmp流终于注册上了
            auto strongSelf = weakSelf.lock();
            if(!strongSelf) {
                return;
            }
            //切换到自己的线程再回复
            //如果触发 kBroadcastMediaChanged 事件的线程与本RtmpSession绑定的线程相同,
            //那么strongSelf->async操作可能是同步操作,
            //通过指定参数may_sync为false确保 NoticeCenter::delListener操作延后执行,
            //以便防止遍历事件监听对象map时做删除操作
            strongSelf->async([task_id,weakSelf,media_info,cb](){
                auto strongSelf = weakSelf.lock();
                if(!strongSelf) {
                    return;
                }
                DebugL << "收到rtmp注册事件,回复播放器:" << media_info._schema << "/" << media_info._vhost << "/" << media_info._app << "/" << media_info._streamid;

                //再找一遍媒体源，一般能找到
                strongSelf->findStream(cb,false);

                //取消延时任务，防止多次回复
                strongSelf->cancelDelyaTask();

                //取消事件监听
                //在事件触发时不能在当前线程移除事件监听,否则会导致遍历map时做删除操作导致程序崩溃
                NoticeCenter::Instance().delListener(task_id,Broadcast::kBroadcastMediaChanged);
            }, false);
        }
    };

    NoticeCenter::Instance().addListener(task_id, Broadcast::kBroadcastMediaChanged, onRegist);
    //5秒后执行失败回调
    doDelay(5, [cb,task_id]() {
        //取消监听该事件,该延时任务可以在本对象析构时或到达指定延时后调用
        //所以该对象在销毁前一定会被取消事件监听
        NoticeCenter::Instance().delListener(task_id,Broadcast::kBroadcastMediaChanged);
        cb(nullptr);
    });

}

void RtmpSession::sendPlayResponse(const string &err,const RtmpMediaSource::Ptr &src){
    bool authSuccess = err.empty();
    bool ok = (src.operator bool() && authSuccess);
    if (ok) {
        //stream begin
        sendUserControl(CONTROL_STREAM_BEGIN, STREAM_MEDIA);
    }
    // onStatus(NetStream.Play.Reset)
    AMFValue status(AMF_OBJECT);
    status.set("level", ok ? "status" : "error");
    status.set("code", ok ? "NetStream.Play.Reset" : (authSuccess ? "NetStream.Play.StreamNotFound" : "NetStream.Play.BadAuth"));
    status.set("description", ok ? "Resetting and playing." : (authSuccess ? "No such stream." : err.data()));
    status.set("details", _mediaInfo._streamid);
    status.set("clientid", "0");
    sendReply("onStatus", nullptr, status);
    if (!ok) {
        WarnL << (authSuccess ? "No such stream:" : err.data()) << " "
              << _mediaInfo._vhost << " "
              << _mediaInfo._app << " "
              << _mediaInfo._streamid
              << endl;
        shutdown();
        return;
    }

    // onStatus(NetStream.Play.Start)
    status.clear();
    status.set("level", "status");
    status.set("code", "NetStream.Play.Start");
    status.set("description", "Started playing.");
    status.set("details", _mediaInfo._streamid);
    status.set("clientid", "0");
    sendReply("onStatus", nullptr, status);

    // |RtmpSampleAccess(true, true)
    AMFEncoder invoke;
    invoke << "|RtmpSampleAccess" << true << true;
    sendResponse(MSG_DATA, invoke.data());

    //onStatus(NetStream.Data.Start)
    invoke.clear();
    AMFValue obj(AMF_OBJECT);
    obj.set("code", "NetStream.Data.Start");
    invoke << "onStatus" << obj;
    sendResponse(MSG_DATA, invoke.data());

    //onStatus(NetStream.Play.PublishNotify)
    status.clear();
    status.set("level", "status");
    status.set("code", "NetStream.Play.PublishNotify");
    status.set("description", "Now published.");
    status.set("details", _mediaInfo._streamid);
    status.set("clientid", "0");
    sendReply("onStatus", nullptr, status);

    // onMetaData
    invoke.clear();
    invoke << "onMetaData" << src->getMetaData();
    sendResponse(MSG_DATA, invoke.data());

    src->getConfigFrame([&](const RtmpPacket::Ptr &pkt) {
        //DebugL<<"send initial frame";
        onSendMedia(pkt);
    });

    _pRingReader = src->getRing()->attach();
    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    SockUtil::setNoDelay(_sock->rawFD(), false);
    _pRingReader->setReadCB([weakSelf](const RtmpPacket::Ptr &pkt) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->async([weakSelf, pkt]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            strongSelf->onSendMedia(pkt);
        });
    });
    _pRingReader->setDetachCB([weakSelf]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->safeShutdown();
    });
    _pPlayerSrc = src;
    if (src->getRing()->readerCount() == 1) {
        src->seekTo(0);
    }

    //提高发送性能
    (*this) << SocketFlags(kSockFlags);
    SockUtil::setNoDelay(_sock->rawFD(),false);
}

void RtmpSession::doPlayResponse(const string &err,const std::function<void(bool)> &cb){
    if(!err.empty()){
        //鉴权失败，直接返回播放失败
        sendPlayResponse(err, nullptr);
        cb(false);
        return;
    }

    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    //鉴权成功，查找媒体源并回复
    findStream([weakSelf,cb](const RtmpMediaSource::Ptr &src){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->sendPlayResponse("", src);
        }
        cb(src.operator bool());
    });
}

void RtmpSession::doPlay(AMFDecoder &dec){
    std::shared_ptr<Ticker> pTicker(new Ticker);
    std::shared_ptr<onceToken> pToken(new onceToken(nullptr,[pTicker](){
        DebugL << "play 回复时间:" << pTicker->elapsedTime() << "ms";
    }));
    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    Broadcast::AuthInvoker invoker = [weakSelf,pToken](const string &err){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        strongSelf->async([weakSelf,err,pToken](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                return;
            }
            strongSelf->doPlayResponse(err,[pToken](bool){});
        });
    };
    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed,_mediaInfo,invoker,*this);
    if(!flag){
        //该事件无人监听,默认不鉴权
        doPlayResponse("",[pToken](bool){});
    }
}
void RtmpSession::onCmd_play2(AMFDecoder &dec) {
	doPlay(dec);
}
void RtmpSession::onCmd_play(AMFDecoder &dec) {
	dec.load<AMFValue>();/* NULL */
    _mediaInfo.parse(_strTcUrl + "/" + dec.load<std::string>());
	doPlay(dec);
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
	if (!_pRingReader) {
		throw std::runtime_error("Rtmp not started yet!");
	}
	if (paused) {
		_pRingReader->setReadCB(nullptr);
	} else {
		weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
		_pRingReader->setReadCB([weakSelf](const RtmpPacket::Ptr &pkt) {
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
	if (!_pPublisherSrc) {
		throw std::runtime_error("not a publisher");
	}
	std::string type = dec.load<std::string>();
	if (type != "onMetaData") {
		throw std::runtime_error("can only set metadata");
	}
	_pPublisherSrc->onGetMetaData(dec.load<AMFValue>());
}

void RtmpSession::onProcessCmd(AMFDecoder &dec) {
    typedef void (RtmpSession::*rtmpCMDHandle)(AMFDecoder &dec);
    static unordered_map<string, rtmpCMDHandle> g_mapCmd;
    static onceToken token([]() {
        g_mapCmd.emplace("connect",&RtmpSession::onCmd_connect);
        g_mapCmd.emplace("createStream",&RtmpSession::onCmd_createStream);
        g_mapCmd.emplace("publish",&RtmpSession::onCmd_publish);
        g_mapCmd.emplace("deleteStream",&RtmpSession::onCmd_deleteStream);
        g_mapCmd.emplace("play",&RtmpSession::onCmd_play);
        g_mapCmd.emplace("play2",&RtmpSession::onCmd_play2);
        g_mapCmd.emplace("seek",&RtmpSession::onCmd_seek);
        g_mapCmd.emplace("pause",&RtmpSession::onCmd_pause);}, []() {});

    std::string method = dec.load<std::string>();
	auto it = g_mapCmd.find(method);
	if (it == g_mapCmd.end()) {
		TraceL << "can not support cmd:" << method;
		return;
	}
	_dNowReqID = dec.load<double>();
	auto fun = it->second;
	(this->*fun)(dec);
}

void RtmpSession::onRtmpChunk(RtmpPacket &chunkData) {
	switch (chunkData.typeId) {
	case MSG_CMD:
	case MSG_CMD3: {
		AMFDecoder dec(chunkData.strBuf, chunkData.typeId == MSG_CMD3 ? 1 : 0);
		onProcessCmd(dec);
	}
		break;

	case MSG_DATA:
	case MSG_DATA3: {
		AMFDecoder dec(chunkData.strBuf, chunkData.typeId == MSG_CMD3 ? 1 : 0);
		std::string type = dec.load<std::string>();
		TraceL << "notify:" << type;
		if (type == "@setDataFrame") {
			setMetaData(dec);
		}
	}
		break;
	case MSG_AUDIO:
	case MSG_VIDEO: {
		if (!_pPublisherSrc) {
			throw std::runtime_error("Not a rtmp publisher!");
		}
		GET_CONFIG_AND_REGISTER(bool,rtmp_modify_stamp,Rtmp::kModifyStamp);
		if(rtmp_modify_stamp){
			chunkData.timeStamp = _stampTicker[chunkData.typeId % 2].elapsedTime();
		}
		_pPublisherSrc->onWrite(std::make_shared<RtmpPacket>(chunkData));
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
    InfoL << "rtmp seekTo(ms):" << milliSeconds;
    auto stongSrc = _pPlayerSrc.lock();
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

void RtmpSession::onSendMedia(const RtmpPacket::Ptr &pkt) {
	auto modifiedStamp = pkt->timeStamp;
	auto &firstStamp = _aui32FirstStamp[pkt->typeId % 2];
	if(!firstStamp){
		firstStamp = modifiedStamp;
	}
	if(modifiedStamp >= firstStamp){
		//计算时间戳增量
		modifiedStamp -= firstStamp;
	}else{
		//发生回环，重新计算时间戳增量
		CLEAR_ARR(_aui32FirstStamp);
		modifiedStamp = 0;
	}
	sendRtmp(pkt->typeId, pkt->streamId, pkt->strBuf, modifiedStamp, pkt->chunkId);
}

void RtmpSession::doDelay(int delaySec, const std::function<void()> &fun) {
    if(_delayTask){
        _delayTask();
    }
    _delayTask = fun;
    _iTaskTimeLine = time(NULL) + delaySec;
}

void RtmpSession::cancelDelyaTask(){
    _delayTask = nullptr;
}


} /* namespace mediakit */
