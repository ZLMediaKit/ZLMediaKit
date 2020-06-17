/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtmpSession.h"
#include "Common/config.h"
#include "Util/onceToken.h"
namespace mediakit {

RtmpSession::RtmpSession(const Socket::Ptr &pSock) : TcpSession(pSock) {
    DebugP(this);
    GET_CONFIG(uint32_t,keep_alive_sec,Rtmp::kKeepAliveSecond);
    pSock->setSendTimeOutSecond(keep_alive_sec);
    //起始接收buffer缓存设置为4K，节省内存
    pSock->setReadBuffer(std::make_shared<BufferRaw>(4 * 1024));
}

RtmpSession::~RtmpSession() {
    DebugP(this);
}

void RtmpSession::onError(const SockException& err) {
    bool isPlayer = !_pPublisherSrc;
    uint64_t duration = _ticker.createdTime()/1000;
    WarnP(this) << (isPlayer ? "RTMP播放器(" : "RTMP推流器(")
                << _mediaInfo._vhost << "/"
                << _mediaInfo._app << "/"
                << _mediaInfo._streamid
                << ")断开:" << err.what()
                << ",耗时(s):" << duration;

    //流量统计事件广播
    GET_CONFIG(uint32_t,iFlowThreshold,General::kFlowThreshold);

    if(_ui64TotalBytes > iFlowThreshold * 1024){
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _mediaInfo, _ui64TotalBytes, duration, isPlayer, static_cast<SockInfo &>(*this));
    }
}

void RtmpSession::onManager() {
    GET_CONFIG(uint32_t,handshake_sec,Rtmp::kHandshakeSecond);
    GET_CONFIG(uint32_t,keep_alive_sec,Rtmp::kKeepAliveSecond);

    if (_ticker.createdTime() > handshake_sec * 1000) {
        if (!_pRingReader && !_pPublisherSrc) {
            shutdown(SockException(Err_timeout,"illegal connection"));
        }
    }
    if (_pPublisherSrc) {
        //publisher
        if (_ticker.elapsedTime() > keep_alive_sec * 1000) {
            shutdown(SockException(Err_timeout,"recv data from rtmp pusher timeout"));
        }
    }
}

void RtmpSession::onRecv(const Buffer::Ptr &pBuf) {
    _ticker.resetTime();
    try {
        _ui64TotalBytes += pBuf->size();
        onParseRtmp(pBuf->data(), pBuf->size());
    } catch (exception &e) {
        shutdown(SockException(Err_shutdown, e.what()));
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
    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    std::shared_ptr<onceToken> pToken(new onceToken(nullptr,[pTicker,weakSelf](){
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            DebugP(strongSelf.get()) << "publish 回复时间:" << pTicker->elapsedTime() << "ms";
        }
    }));
    dec.load<AMFValue>();/* NULL */
    _mediaInfo.parse(_strTcUrl + "/" + getStreamId(dec.load<std::string>()));
    _mediaInfo._schema = RTMP_SCHEMA;

    auto onRes = [this,pToken](const string &err,bool enableRtxp,bool enableHls,bool enableMP4){
        auto src = dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA,
                                                                           _mediaInfo._vhost,
                                                                           _mediaInfo._app,
                                                                           _mediaInfo._streamid));
        bool authSuccess = err.empty();
        bool ok = (!src && !_pPublisherSrc && authSuccess);
        AMFValue status(AMF_OBJECT);
        status.set("level", ok ? "status" : "error");
        status.set("code", ok ? "NetStream.Publish.Start" : (authSuccess ? "NetStream.Publish.BadName" : "NetStream.Publish.BadAuth"));
        status.set("description", ok ? "Started publishing stream." : (authSuccess ? "Already publishing." : err.data()));
        status.set("clientid", "0");
        sendReply("onStatus", nullptr, status);
        if (!ok) {
            string errMsg = StrPrinter << (authSuccess ? "already publishing:" : err.data()) << " "
                                    << _mediaInfo._vhost << " "
                                    << _mediaInfo._app << " "
                                    << _mediaInfo._streamid;
            shutdown(SockException(Err_shutdown,errMsg));
            return;
        }
        _pPublisherSrc.reset(new RtmpMediaSourceImp(_mediaInfo._vhost,_mediaInfo._app,_mediaInfo._streamid));
        _pPublisherSrc->setListener(dynamic_pointer_cast<MediaSourceEvent>(shared_from_this()));
        //设置转协议
        _pPublisherSrc->setProtocolTranslation(enableRtxp,enableHls,enableMP4);

        //如果是rtmp推流客户端，那么加大TCP接收缓存，这样能提升接收性能
        _sock->setReadBuffer(std::make_shared<BufferRaw>(256 * 1024));
        setSocketFlags();
    };

    if(_mediaInfo._app.empty() || _mediaInfo._streamid.empty()){
        //不允许莫名其妙的推流url
        onRes("rtmp推流url非法", false, false, false);
        return;
    }

    Broadcast::PublishAuthInvoker invoker = [weakSelf,onRes,pToken](const string &err,bool enableRtxp,bool enableHls,bool enableMP4){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        strongSelf->async([weakSelf,onRes,err,pToken,enableRtxp,enableHls,enableMP4](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                return;
            }
            onRes(err,enableRtxp,enableHls,enableMP4);
        });
    };
    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPublish,_mediaInfo,invoker,static_cast<SockInfo &>(*this));
    if(!flag){
        //该事件无人监听，默认鉴权成功
        GET_CONFIG(bool,toRtxp,General::kPublishToRtxp);
        GET_CONFIG(bool,toHls,General::kPublishToHls);
        GET_CONFIG(bool,toMP4,General::kPublishToMP4);
        onRes("",toRtxp,toHls,toMP4);
    }
}

void RtmpSession::onCmd_deleteStream(AMFDecoder &dec) {
    AMFValue status(AMF_OBJECT);
    status.set("level", "status");
    status.set("code", "NetStream.Unpublish.Success");
    status.set("description", "Stop publishing.");
    sendReply("onStatus", nullptr, status);
    throw std::runtime_error(StrPrinter << "Stop publishing" << endl);
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
        string errMsg = StrPrinter << (authSuccess ? "no such stream:" : err.data()) << " "
                                 << _mediaInfo._vhost << " "
                                 << _mediaInfo._app << " "
                                 << _mediaInfo._streamid;
        shutdown(SockException(Err_shutdown,errMsg));
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

    auto &metadata = src->getMetaData();
    if(metadata){
        //在有metadata的情况下才发送metadata
        //其实metadata没什么用，有些推流器不产生metadata
        // onMetaData
        invoke.clear();
        invoke << "onMetaData" << metadata;
        sendResponse(MSG_DATA, invoke.data());
        auto duration = metadata["duration"].as_number();
        if(duration > 0){
            //这是点播，使用绝对时间戳
            _stamp[0].setPlayBack();
            _stamp[1].setPlayBack();
        }
    }


    src->getConfigFrame([&](const RtmpPacket::Ptr &pkt) {
        //DebugP(this)<<"send initial frame";
        onSendMedia(pkt);
    });

    //音频同步于视频
    _stamp[0].syncTo(_stamp[1]);
    _pRingReader = src->getRing()->attach(getPoller());
    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    _pRingReader->setReadCB([weakSelf](const RtmpMediaSource::RingDataType &pkt) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        if(strongSelf->_paused){
            return;
        }
        int i = 0;
        int size = pkt->size();
        strongSelf->setSendFlushFlag(false);
        pkt->for_each([&](const RtmpPacket::Ptr &rtmp){
            if(++i == size){
                strongSelf->setSendFlushFlag(true);
            }
            strongSelf->onSendMedia(rtmp);
        });
    });
    _pRingReader->setDetachCB([weakSelf]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->shutdown(SockException(Err_shutdown,"rtmp ring buffer detached"));
    });
    _pPlayerSrc = src;
    if (src->totalReaderCount() == 1) {
        src->seekTo(0);
    }
    //提高服务器发送性能
    setSocketFlags();
}

void RtmpSession::doPlayResponse(const string &err,const std::function<void(bool)> &cb){
    if(!err.empty()){
        //鉴权失败，直接返回播放失败
        sendPlayResponse(err, nullptr);
        cb(false);
        return;
    }

    //鉴权成功，查找媒体源并回复
    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    MediaSource::findAsync(_mediaInfo,weakSelf.lock(),[weakSelf,cb](const MediaSource::Ptr &src){
        auto rtmp_src = dynamic_pointer_cast<RtmpMediaSource>(src);
        auto strongSelf = weakSelf.lock();
        if(strongSelf){
            strongSelf->sendPlayResponse("", rtmp_src);
        }
        cb(rtmp_src.operator bool());
    });
}

void RtmpSession::doPlay(AMFDecoder &dec){
    std::shared_ptr<Ticker> pTicker(new Ticker);
    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    std::shared_ptr<onceToken> pToken(new onceToken(nullptr,[pTicker,weakSelf](){
        auto strongSelf = weakSelf.lock();
        if(strongSelf) {
            DebugP(strongSelf.get()) << "play 回复时间:" << pTicker->elapsedTime() << "ms";
        }
    }));
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

    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed,_mediaInfo,invoker,static_cast<SockInfo &>(*this));
    if(!flag){
        //该事件无人监听,默认不鉴权
        doPlayResponse("",[pToken](bool){});
    }
}
void RtmpSession::onCmd_play2(AMFDecoder &dec) {
    doPlay(dec);
}

string RtmpSession::getStreamId(const string &str){
    string stream_id;
    string params;
    auto pos = str.find('?');
    if(pos != string::npos){
        //有url参数
        stream_id = str.substr(0,pos);
        //获取url参数
        params = str.substr(pos + 1);
    }else{
        //没有url参数
        stream_id = str;
    }

    pos = stream_id.find(":");
    if(pos != string::npos){
        //vlc和ffplay在播放 rtmp://127.0.0.1/record/0.mp4时，
        //传过来的url会是rtmp://127.0.0.1/record/mp4:0,
        //我们在这里还原成0.mp4
        stream_id = stream_id.substr(pos + 1) + "." + stream_id.substr(0,pos);
    }

    if(params.empty()){
        //没有url参数
        return stream_id;
    }

    //有url参数
    return stream_id + '?' + params;
}

void RtmpSession::onCmd_play(AMFDecoder &dec) {
    dec.load<AMFValue>();/* NULL */
    _mediaInfo.parse(_strTcUrl + "/" + getStreamId(dec.load<std::string>()));
    _mediaInfo._schema = RTMP_SCHEMA;
    doPlay(dec);
}

void RtmpSession::onCmd_pause(AMFDecoder &dec) {
    dec.load<AMFValue>();/* NULL */
    bool paused = dec.load<bool>();
    TraceP(this) << paused;
    AMFValue status(AMF_OBJECT);
    status.set("level", "status");
    status.set("code", paused ? "NetStream.Pause.Notify" : "NetStream.Unpause.Notify");
    status.set("description", paused ? "Paused stream." : "Unpaused stream.");
    sendReply("onStatus", nullptr, status);
    //streamBegin
    sendUserControl(paused ? CONTROL_STREAM_EOF : CONTROL_STREAM_BEGIN, STREAM_MEDIA);
    _paused = paused;
}

void RtmpSession::setMetaData(AMFDecoder &dec) {
    if (!_pPublisherSrc) {
        throw std::runtime_error("not a publisher");
    }
    std::string type = dec.load<std::string>();
    if (type != "onMetaData") {
        throw std::runtime_error("can only set metadata");
    }
    auto metadata = dec.load<AMFValue>();
//    dumpMetadata(metadata);
    _pPublisherSrc->setMetaData(metadata);
    _set_meta_data = true;
}

void RtmpSession::onProcessCmd(AMFDecoder &dec) {
    typedef void (RtmpSession::*rtmpCMDHandle)(AMFDecoder &dec);
    static unordered_map<string, rtmpCMDHandle> s_cmd_functions;
    static onceToken token([]() {
        s_cmd_functions.emplace("connect",&RtmpSession::onCmd_connect);
        s_cmd_functions.emplace("createStream",&RtmpSession::onCmd_createStream);
        s_cmd_functions.emplace("publish",&RtmpSession::onCmd_publish);
        s_cmd_functions.emplace("deleteStream",&RtmpSession::onCmd_deleteStream);
        s_cmd_functions.emplace("play",&RtmpSession::onCmd_play);
        s_cmd_functions.emplace("play2",&RtmpSession::onCmd_play2);
        s_cmd_functions.emplace("seek",&RtmpSession::onCmd_seek);
        s_cmd_functions.emplace("pause",&RtmpSession::onCmd_pause);}, []() {});

    std::string method = dec.load<std::string>();
    auto it = s_cmd_functions.find(method);
    if (it == s_cmd_functions.end()) {
//		TraceP(this) << "can not support cmd:" << method;
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
        if (type == "@setDataFrame") {
            setMetaData(dec);
        }else{
            TraceP(this) << "unknown notify:" << type;
        }
    }
        break;
    case MSG_AUDIO:
    case MSG_VIDEO: {
        if (!_pPublisherSrc) {
            throw std::runtime_error("Not a rtmp publisher!");
        }
        GET_CONFIG(bool,rtmp_modify_stamp,Rtmp::kModifyStamp);
        if(rtmp_modify_stamp){
            int64_t dts_out;
            _stamp[chunkData.typeId % 2].revise(chunkData.timeStamp, chunkData.timeStamp, dts_out, dts_out, true);
            chunkData.timeStamp = dts_out;
        }

        if(!_set_meta_data && !chunkData.isCfgFrame()){
            _set_meta_data = true;
            _pPublisherSrc->setMetaData(TitleMeta().getMetadata());
        }
        _pPublisherSrc->onWrite(std::make_shared<RtmpPacket>(std::move(chunkData)));
    }
        break;
    default:
        WarnP(this) << "unhandled message:" << (int) chunkData.typeId << hexdump(chunkData.strBuf.data(), chunkData.strBuf.size());
        break;
    }
}

void RtmpSession::onCmd_seek(AMFDecoder &dec) {
    dec.load<AMFValue>();/* NULL */
    AMFValue status(AMF_OBJECT);
    AMFEncoder invoke;
    status.set("level", "status");
    status.set("code", "NetStream.Seek.Notify");
    status.set("description", "Seeking.");
    sendReply("onStatus", nullptr, status);

    auto milliSeconds = dec.load<AMFValue>().as_number();
    InfoP(this) << "rtmp seekTo(ms):" << milliSeconds;
    auto stongSrc = _pPlayerSrc.lock();
    if (stongSrc) {
        stongSrc->seekTo(milliSeconds);
    }
}

void RtmpSession::onSendMedia(const RtmpPacket::Ptr &pkt) {
    //rtmp播放器时间戳从零开始
    int64_t dts_out;
    _stamp[pkt->typeId % 2].revise(pkt->timeStamp, 0, dts_out, dts_out);
    sendRtmp(pkt->typeId, pkt->streamId, pkt, dts_out, pkt->chunkId);
}


bool RtmpSession::close(MediaSource &sender,bool force)  {
    //此回调在其他线程触发
    if(!_pPublisherSrc || (!force && _pPublisherSrc->totalReaderCount())){
        return false;
    }
    string err = StrPrinter << "close media:" << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    safeShutdown(SockException(Err_shutdown,err));
    return true;
}

int RtmpSession::totalReaderCount(MediaSource &sender) {
    return _pPublisherSrc ? _pPublisherSrc->totalReaderCount() : sender.readerCount();
}

void RtmpSession::setSocketFlags(){
    GET_CONFIG(int, mergeWriteMS, General::kMergeWriteMS);
    if(mergeWriteMS > 0) {
        //推流模式下，关闭TCP_NODELAY会增加推流端的延时，但是服务器性能将提高
        SockUtil::setNoDelay(_sock->rawFD(), false);
        //播放模式下，开启MSG_MORE会增加延时，但是能提高发送性能
        setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
    }
}

void RtmpSession::dumpMetadata(const AMFValue &metadata) {
    if(metadata.type() != AMF_OBJECT && metadata.type() != AMF_ECMA_ARRAY){
        WarnL << "invalid metadata type:" << metadata.type();
        return ;
    }
    _StrPrinter printer;
    metadata.object_for_each([&](const string &key, const AMFValue &val){
            printer << "\r\n" << key << "\t:" << val.to_string() ;
    });
    InfoL << _mediaInfo._vhost << " " << _mediaInfo._app << " " << _mediaInfo._streamid << (string)printer;
}
} /* namespace mediakit */
