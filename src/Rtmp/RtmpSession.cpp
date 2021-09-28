/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtmpSession.h"
#include "Common/config.h"
#include "Util/onceToken.h"
namespace mediakit {

RtmpSession::RtmpSession(const Socket::Ptr &sock) : TcpSession(sock) {
    DebugP(this);
    GET_CONFIG(uint32_t,keep_alive_sec,Rtmp::kKeepAliveSecond);
    sock->setSendTimeOutSecond(keep_alive_sec);
}

RtmpSession::~RtmpSession() {
    DebugP(this);
}

void RtmpSession::onError(const SockException& err) {
    bool isPlayer = !_publisher_src;
    uint64_t duration = _ticker.createdTime()/1000;
    WarnP(this) << (isPlayer ? "RTMP播放器(" : "RTMP推流器(")
                << _media_info._vhost << "/"
                << _media_info._app << "/"
                << _media_info._streamid
                << ")断开:" << err.what()
                << ",耗时(s):" << duration;

    //流量统计事件广播
    GET_CONFIG(uint32_t,iFlowThreshold,General::kFlowThreshold);

    if(_total_bytes >= iFlowThreshold * 1024){
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _media_info, _total_bytes, duration, isPlayer, static_cast<SockInfo &>(*this));
    }
}

void RtmpSession::onManager() {
    GET_CONFIG(uint32_t,handshake_sec,Rtmp::kHandshakeSecond);
    GET_CONFIG(uint32_t,keep_alive_sec,Rtmp::kKeepAliveSecond);

    if (_ticker.createdTime() > handshake_sec * 1000) {
        if (!_ring_reader && !_publisher_src) {
            shutdown(SockException(Err_timeout,"illegal connection"));
        }
    }
    if (_publisher_src) {
        //publisher
        if (_ticker.elapsedTime() > keep_alive_sec * 1000) {
            shutdown(SockException(Err_timeout,"recv data from rtmp pusher timeout"));
        }
    }
}

void RtmpSession::onRecv(const Buffer::Ptr &buf) {
    _ticker.resetTime();
    try {
        _total_bytes += buf->size();
        onParseRtmp(buf->data(), buf->size());
    } catch (exception &ex) {
        shutdown(SockException(Err_shutdown, ex.what()));
    }
}

void RtmpSession::onCmd_connect(AMFDecoder &dec) {
    auto params = dec.load<AMFValue>();
    ///////////set chunk size////////////////
    sendChunkSize(60000);
    ////////////window Acknowledgement size/////
    sendAcknowledgementSize(5000000);
    ///////////set peerBandwidth////////////////
    sendPeerBandwidth(5000000);

    _media_info._app = params["app"].as_string();
    _tc_url = params["tcUrl"].as_string();
    if(_tc_url.empty()){
        //defaultVhost:默认vhost
        _tc_url = string(RTMP_SCHEMA) + "://" + DEFAULT_VHOST + "/" + _media_info._app;
    } else {
        auto pos = _tc_url.rfind('?');
        if (pos != string::npos) {
            //tc_url 中可能包含?以及参数，参见issue: #692
            _tc_url = _tc_url.substr(0, pos);
        }
    }
    bool ok = true; //(app == APP_NAME);
    AMFValue version(AMF_OBJECT);
    version.set("fmsVer", "FMS/3,0,1,123");
    version.set("capabilities", 31.0);
    AMFValue status(AMF_OBJECT);
    status.set("level", ok ? "status" : "error");
    status.set("code", ok ? "NetConnection.Connect.Success" : "NetConnection.Connect.InvalidApp");
    status.set("description", ok ? "Connection succeeded." : "InvalidApp.");
    status.set("objectEncoding", params["objectEncoding"]);
    sendReply(ok ? "_result" : "_error", version, status);
    if (!ok) {
        throw std::runtime_error("Unsupported application: " + _media_info._app);
    }

    AMFEncoder invoke;
    invoke << "onBWDone" << 0.0 << nullptr;
    sendResponse(MSG_CMD, invoke.data());
}

void RtmpSession::onCmd_createStream(AMFDecoder &dec) {
    sendReply("_result", nullptr, double(STREAM_MEDIA));
}

void RtmpSession::onCmd_publish(AMFDecoder &dec) {
    std::shared_ptr<Ticker> ticker(new Ticker);
    weak_ptr<RtmpSession> weak_self = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    std::shared_ptr<onceToken> pToken(new onceToken(nullptr,[ticker,weak_self](){
        auto strong_self = weak_self.lock();
        if(strong_self){
            DebugP(strong_self.get()) << "publish 回复时间:" << ticker->elapsedTime() << "ms";
        }
    }));
    dec.load<AMFValue>();/* NULL */
    _media_info.parse(_tc_url + "/" + getStreamId(dec.load<std::string>()));
    _media_info._schema = RTMP_SCHEMA;

    auto on_res = [this,pToken](const string &err, bool enableHls, bool enableMP4){
        auto src = dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA,
                                                                           _media_info._vhost,
                                                                           _media_info._app,
                                                                           _media_info._streamid));
        bool auth_success = err.empty();
        bool ok = (!src && !_publisher_src && auth_success);
        AMFValue status(AMF_OBJECT);
        status.set("level", ok ? "status" : "error");
        status.set("code", ok ? "NetStream.Publish.Start" : (auth_success ? "NetStream.Publish.BadName" : "NetStream.Publish.BadAuth"));
        status.set("description", ok ? "Started publishing stream." : (auth_success ? "Already publishing." : err.data()));
        status.set("clientid", "0");
        sendReply("onStatus", nullptr, status);
        if (!ok) {
            string errMsg = StrPrinter << (auth_success ? "already publishing:" : err.data()) << " "
                                       << _media_info._vhost << " "
                                       << _media_info._app << " "
                                       << _media_info._streamid;
            shutdown(SockException(Err_shutdown,errMsg));
            return;
        }
        _publisher_src.reset(new RtmpMediaSourceImp(_media_info._vhost, _media_info._app, _media_info._streamid));
        _publisher_src->setListener(dynamic_pointer_cast<MediaSourceEvent>(shared_from_this()));
        //设置转协议
        _publisher_src->setProtocolTranslation(enableHls, enableMP4);
        setSocketFlags();
    };

    if(_media_info._app.empty() || _media_info._streamid.empty()){
        //不允许莫名其妙的推流url
        on_res("rtmp推流url非法", false, false);
        return;
    }

    Broadcast::PublishAuthInvoker invoker = [weak_self, on_res, pToken](const string &err, bool enableHls, bool enableMP4) {
        auto strongSelf = weak_self.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->async([weak_self, on_res, err, pToken, enableHls, enableMP4]() {
            auto strongSelf = weak_self.lock();
            if (!strongSelf) {
                return;
            }
            on_res(err, enableHls, enableMP4);
        });
    };
    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPublish, _media_info, invoker, static_cast<SockInfo &>(*this));
    if(!flag){
        //该事件无人监听，默认鉴权成功
        GET_CONFIG(bool,to_hls,General::kPublishToHls);
        GET_CONFIG(bool,to_mp4,General::kPublishToMP4);
        on_res("", to_hls, to_mp4);
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
    bool auth_success = err.empty();
    bool ok = (src.operator bool() && auth_success);
    if (ok) {
        //stream begin
        sendUserControl(CONTROL_STREAM_BEGIN, STREAM_MEDIA);
    }
    // onStatus(NetStream.Play.Reset)
    AMFValue status(AMF_OBJECT);
    status.set("level", ok ? "status" : "error");
    status.set("code", ok ? "NetStream.Play.Reset" : (auth_success ? "NetStream.Play.StreamNotFound" : "NetStream.Play.BadAuth"));
    status.set("description", ok ? "Resetting and playing." : (auth_success ? "No such stream." : err.data()));
    status.set("details", _media_info._streamid);
    status.set("clientid", "0");
    sendReply("onStatus", nullptr, status);
    if (!ok) {
        string err_msg = StrPrinter << (auth_success ? "no such stream:" : err.data()) << " "
                                    << _media_info._vhost << " "
                                    << _media_info._app << " "
                                    << _media_info._streamid;
        shutdown(SockException(Err_shutdown, err_msg));
        return;
    }

    // onStatus(NetStream.Play.Start)
    status.clear();
    status.set("level", "status");
    status.set("code", "NetStream.Play.Start");
    status.set("description", "Started playing.");
    status.set("details", _media_info._streamid);
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
    status.set("details", _media_info._streamid);
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
        auto duration = metadata["duration"];
        if(duration && duration.as_number() > 0){
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
    src->pause(false);
    _ring_reader = src->getRing()->attach(getPoller());
    weak_ptr<RtmpSession> weakSelf = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    _ring_reader->setReadCB([weakSelf](const RtmpMediaSource::RingDataType &pkt) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        size_t i = 0;
        auto size = pkt->size();
        strongSelf->setSendFlushFlag(false);
        pkt->for_each([&](const RtmpPacket::Ptr &rtmp){
            if(++i == size){
                strongSelf->setSendFlushFlag(true);
            }
            strongSelf->onSendMedia(rtmp);
        });
    });
    _ring_reader->setDetachCB([weakSelf]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->shutdown(SockException(Err_shutdown,"rtmp ring buffer detached"));
    });
    src->pause(false);
    _player_src = src;
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
    weak_ptr<RtmpSession> weak_self = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    MediaSource::findAsync(_media_info, weak_self.lock(), [weak_self,cb](const MediaSource::Ptr &src){
        auto rtmp_src = dynamic_pointer_cast<RtmpMediaSource>(src);
        auto strong_self = weak_self.lock();
        if(strong_self){
            strong_self->sendPlayResponse("", rtmp_src);
        }
        cb(rtmp_src.operator bool());
    });
}

void RtmpSession::doPlay(AMFDecoder &dec){
    std::shared_ptr<Ticker> ticker(new Ticker);
    weak_ptr<RtmpSession> weak_self = dynamic_pointer_cast<RtmpSession>(shared_from_this());
    std::shared_ptr<onceToken> token(new onceToken(nullptr, [ticker,weak_self](){
        auto strongSelf = weak_self.lock();
        if (strongSelf) {
            DebugP(strongSelf.get()) << "play 回复时间:" << ticker->elapsedTime() << "ms";
        }
    }));
    Broadcast::AuthInvoker invoker = [weak_self,token](const string &err){
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->async([weak_self, err, token]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->doPlayResponse(err, [token](bool) {});
        });
    };

    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed, _media_info, invoker, static_cast<SockInfo &>(*this));
    if(!flag){
        //该事件无人监听,默认不鉴权
        doPlayResponse("",[token](bool){});
    }
}

void RtmpSession::onCmd_play2(AMFDecoder &dec) {
    doPlay(dec);
}

string RtmpSession::getStreamId(const string &str){
    string stream_id;
    string params;
    auto pos = str.find('?');
    if (pos != string::npos) {
        //有url参数
        stream_id = str.substr(0, pos);
        //获取url参数
        params = str.substr(pos + 1);
    } else {
        //没有url参数
        stream_id = str;
    }

    pos = stream_id.find(":");
    if (pos != string::npos) {
        //vlc和ffplay在播放 rtmp://127.0.0.1/record/0.mp4时，
        //传过来的url会是rtmp://127.0.0.1/record/mp4:0,
        //我们在这里还原成0.mp4
        //实际使用时发现vlc，mpv等会传过来rtmp://127.0.0.1/record/mp4:0.mp4,这里做个判断
        auto ext = stream_id.substr(0, pos);
        stream_id = stream_id.substr(pos + 1);
        if (stream_id.find(ext) == string::npos) {
            stream_id = stream_id + "." + ext;
        }
    }

    if (params.empty()) {
        //没有url参数
        return stream_id;
    }

    //有url参数
    return stream_id + '?' + params;
}

void RtmpSession::onCmd_play(AMFDecoder &dec) {
    dec.load<AMFValue>();/* NULL */
    _media_info.parse(_tc_url + "/" + getStreamId(dec.load<std::string>()));
    _media_info._schema = RTMP_SCHEMA;
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
    auto strongSrc = _player_src.lock();
    if (strongSrc) {
        strongSrc->pause(paused);
    }
}

void RtmpSession::onCmd_playCtrl(AMFDecoder &dec) {
    dec.load<AMFValue>();
    auto ctrlObj = dec.load<AMFValue>();
    int ctrlType = ctrlObj["ctrlType"].as_integer();
    float speed = ctrlObj["speed"].as_number();

    AMFValue status(AMF_OBJECT);
    status.set("level", "status");
    status.set("code", "NetStream.Speed.Notify");
    status.set("description", "Speeding");
    sendReply("onStatus", nullptr, status);
    //streamBegin
    sendUserControl(CONTROL_STREAM_EOF, STREAM_MEDIA);

    auto stongSrc = _player_src.lock();
    if (stongSrc) {
        stongSrc->speed(speed);
    }
}

void RtmpSession::setMetaData(AMFDecoder &dec) {
    std::string type = dec.load<std::string>();
    if (type != "onMetaData") {
        throw std::runtime_error("can only set metadata");
    }
    _publisher_metadata = dec.load<AMFValue>();
}

void RtmpSession::onProcessCmd(AMFDecoder &dec) {
    typedef void (RtmpSession::*cmd_function)(AMFDecoder &dec);
    static unordered_map<string, cmd_function> s_cmd_functions;
    static onceToken token([]() {
        s_cmd_functions.emplace("connect", &RtmpSession::onCmd_connect);
        s_cmd_functions.emplace("createStream", &RtmpSession::onCmd_createStream);
        s_cmd_functions.emplace("publish", &RtmpSession::onCmd_publish);
        s_cmd_functions.emplace("deleteStream", &RtmpSession::onCmd_deleteStream);
        s_cmd_functions.emplace("play", &RtmpSession::onCmd_play);
        s_cmd_functions.emplace("play2", &RtmpSession::onCmd_play2);
        s_cmd_functions.emplace("seek", &RtmpSession::onCmd_seek);
        s_cmd_functions.emplace("pause", &RtmpSession::onCmd_pause);
        s_cmd_functions.emplace("onPlayCtrl", &RtmpSession::onCmd_playCtrl);
    });

    std::string method = dec.load<std::string>();
    auto it = s_cmd_functions.find(method);
    if (it == s_cmd_functions.end()) {
//		TraceP(this) << "can not support cmd:" << method;
        return;
    }
    _recv_req_id = dec.load<double>();
    auto fun = it->second;
    (this->*fun)(dec);
}

void RtmpSession::onRtmpChunk(RtmpPacket::Ptr packet) {
    auto &chunk_data = *packet;
    switch (chunk_data.type_id) {
    case MSG_CMD:
    case MSG_CMD3: {
        AMFDecoder dec(chunk_data.buffer, chunk_data.type_id == MSG_CMD3 ? 3 : 0);
        onProcessCmd(dec);
        break;
    }

    case MSG_DATA:
    case MSG_DATA3: {
        AMFDecoder dec(chunk_data.buffer, chunk_data.type_id == MSG_DATA3 ? 3 : 0);
        std::string type = dec.load<std::string>();
        if (type == "@setDataFrame") {
            setMetaData(dec);
        } else if (type == "onMetaData") {
            //兼容某些不规范的推流器
            _publisher_metadata = dec.load<AMFValue>();
        } else {
            TraceP(this) << "unknown notify:" << type;
        }
        break;
    }

    case MSG_AUDIO:
    case MSG_VIDEO: {
        if (!_publisher_src) {
            WarnL << "Not a rtmp publisher!";
            return;
        }
        GET_CONFIG(bool, rtmp_modify_stamp, Rtmp::kModifyStamp);
        if (rtmp_modify_stamp) {
            int64_t dts_out;
            _stamp[chunk_data.type_id % 2].revise(chunk_data.time_stamp, chunk_data.time_stamp, dts_out, dts_out, true);
            chunk_data.time_stamp = (uint32_t)dts_out;
        }

        if (!_set_meta_data) {
            _set_meta_data = true;
            _publisher_src->setMetaData(_publisher_metadata ? _publisher_metadata : TitleMeta().getMetadata());
        }
        _publisher_src->onWrite(std::move(packet));
        break;
    }

    default:
        WarnP(this) << "unhandled message:" << (int) chunk_data.type_id << hexdump(chunk_data.buffer.data(), chunk_data.buffer.size());
        break;
    }
}

void RtmpSession::onCmd_seek(AMFDecoder &dec) {
    dec.load<AMFValue>();/* NULL */
    AMFValue status(AMF_OBJECT);
    status.set("level", "status");
    status.set("code", "NetStream.Seek.Notify");
    status.set("description", "Seeking.");
    sendReply("onStatus", nullptr, status);

    auto milliSeconds = (uint32_t)(dec.load<AMFValue>().as_number());
    InfoP(this) << "rtmp seekTo(ms):" << milliSeconds;
    auto strong_src = _player_src.lock();
    if (strong_src) {
        strong_src->seekTo(milliSeconds);
    }
}

void RtmpSession::onSendMedia(const RtmpPacket::Ptr &pkt) {
    //rtmp播放器时间戳从零开始
    int64_t dts_out;
    _stamp[pkt->type_id % 2].revise(pkt->time_stamp, 0, dts_out, dts_out);
    sendRtmp(pkt->type_id, pkt->stream_index, pkt, (uint32_t)dts_out, pkt->chunk_id);
}


bool RtmpSession::close(MediaSource &sender,bool force)  {
    //此回调在其他线程触发
    if(!_publisher_src || (!force && _publisher_src->totalReaderCount())){
        return false;
    }
    string err = StrPrinter << "close media:" << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    safeShutdown(SockException(Err_shutdown,err));
    return true;
}

int RtmpSession::totalReaderCount(MediaSource &sender) {
    return _publisher_src ? _publisher_src->totalReaderCount() : sender.readerCount();
}

MediaOriginType RtmpSession::getOriginType(MediaSource &sender) const{
    return MediaOriginType::rtmp_push;
}

string RtmpSession::getOriginUrl(MediaSource &sender) const {
    return _media_info._full_url;
}

std::shared_ptr<SockInfo> RtmpSession::getOriginSock(MediaSource &sender) const {
    return const_cast<RtmpSession *>(this)->shared_from_this();
}

void RtmpSession::setSocketFlags(){
    GET_CONFIG(int, merge_write_ms, General::kMergeWriteMS);
    if (merge_write_ms > 0) {
        //推流模式下，关闭TCP_NODELAY会增加推流端的延时，但是服务器性能将提高
        SockUtil::setNoDelay(getSock()->rawFD(), false);
        //播放模式下，开启MSG_MORE会增加延时，但是能提高发送性能
        setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
    }
}

void RtmpSession::dumpMetadata(const AMFValue &metadata) {
    if (metadata.type() != AMF_OBJECT && metadata.type() != AMF_ECMA_ARRAY) {
        WarnL << "invalid metadata type:" << metadata.type();
        return;
    }
    _StrPrinter printer;
    metadata.object_for_each([&](const string &key, const AMFValue &val) {
        printer << "\r\n" << key << "\t:" << val.to_string();
    });
    InfoL << _media_info._vhost << " " << _media_info._app << " " << _media_info._streamid << (string) printer;
}
} /* namespace mediakit */
