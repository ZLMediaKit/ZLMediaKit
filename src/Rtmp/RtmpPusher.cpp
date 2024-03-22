/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtmpPusher.h"
#include "Rtmp/utils.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"
#include "Common/Parser.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

RtmpPusher::RtmpPusher(const EventPoller::Ptr &poller, const RtmpMediaSource::Ptr &src) : TcpClient(poller){
    _publish_src = src;
}

RtmpPusher::~RtmpPusher() {
    teardown();
    DebugL;
}

void RtmpPusher::teardown() {
    if (alive()) {
        _app.clear();
        _stream_id.clear();
        _tc_url.clear();
        _map_on_result.clear();
        _deque_on_status.clear();
        _publish_timer.reset();
        reset();
        shutdown(SockException(Err_shutdown, "teardown"));
    }
}

void RtmpPusher::onPublishResult_l(const SockException &ex, bool handshake_done) {
    DebugL << ex.what();
    if (ex.getErrCode() == Err_shutdown) {
        //主动shutdown的，不触发回调
        return;
    }
    if (!handshake_done) {
        //播放结果回调
        _publish_timer.reset();
        onPublishResult(ex);
    } else {
        //播放成功后异常断开回调
        onShutdown(ex);
    }

    if (ex) {
        shutdown(SockException(Err_shutdown,"teardown"));
    }
}

void RtmpPusher::publish(const string &url) {
    teardown();
    auto schema = findSubString(url.data(), nullptr, "://");
    auto host_url = findSubString(url.data(), "://", "/");
    _app = findSubString(url.data(), (host_url + "/").data(), "/");
    _stream_id = findSubString(url.data(), (host_url + "/" + _app + "/").data(), NULL);
    auto app_second = findSubString(_stream_id.data(), nullptr, "/");
    if (!app_second.empty() && app_second.find('?') == std::string::npos) {
        // _stream_id存在多级；不包含'?', 说明分割符'/'不是url参数的一部分
        _app += "/" + app_second;
        _stream_id.erase(0, app_second.size() + 1);
    }
    _tc_url = schema + "://" + host_url + "/" + _app;
    if (_app.empty() || _stream_id.empty()) {
        onPublishResult_l(SockException(Err_other, "rtmp url非法"), false);
        return;
    }
    DebugL << host_url << " " << _app << " " << _stream_id;

    uint16_t port = start_with(url, "rtmps") ? 443 : 1935;
    splitUrl(host_url, host_url, port);

    weak_ptr<RtmpPusher> weakSelf = static_pointer_cast<RtmpPusher>(shared_from_this());
    float publishTimeOutSec = (*this)[Client::kTimeoutMS].as<int>() / 1000.0f;
    _publish_timer.reset(new Timer(publishTimeOutSec, [weakSelf]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return false;
        }
        strongSelf->onPublishResult_l(SockException(Err_timeout, "publish rtmp timeout"), false);
        return false;
    }, getPoller()));

    if (!(*this)[Client::kNetAdapter].empty()) {
        setNetAdapter((*this)[Client::kNetAdapter]);
    }

    startConnect(host_url, port);
}

void RtmpPusher::onError(const SockException &ex){
    //定时器_pPublishTimer为空后表明握手结束了
    onPublishResult_l(ex, !_publish_timer);
}

void RtmpPusher::onConnect(const SockException &err){
    if (err) {
        onPublishResult_l(err, false);
        return;
    }
    weak_ptr<RtmpPusher> weak_self = static_pointer_cast<RtmpPusher>(shared_from_this());
    startClientSession([weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }

        strong_self->sendChunkSize(60000);
        strong_self->send_connect();
    });
}

void RtmpPusher::onRecv(const Buffer::Ptr &buf){
    try {
        onParseRtmp(buf->data(), buf->size());
    } catch (exception &e) {
        SockException ex(Err_other, e.what());
        //定时器_pPublishTimer为空后表明握手结束了
        onPublishResult_l(ex, !_publish_timer);
    }
}

void RtmpPusher::send_connect() {
    AMFValue obj(AMF_OBJECT);
    obj.set("app", _app);
    obj.set("type", "nonprivate");
    obj.set("tcUrl", _tc_url);
    obj.set("swfUrl", _tc_url);

    AMFValue fourCcList(AMF_STRICT_ARRAY);
    fourCcList.add("av01");
    fourCcList.add("vp09");
    fourCcList.add("hvc1");
    obj.set("fourCcList", fourCcList);

    sendInvoke("connect", obj);
    addOnResultCB([this](AMFDecoder &dec) {
        //TraceL << "connect result";
        dec.load<AMFValue>();
        auto val = dec.load<AMFValue>();
        auto level = val["level"].as_string();
        auto code = val["code"].as_string();
        if (level != "status") {
            throw std::runtime_error(StrPrinter << "connect 失败:" << level << " " << code << endl);
        }
        send_createStream();
    });
}

void RtmpPusher::send_createStream() {
    AMFValue obj(AMF_NULL);
    sendInvoke("createStream", obj);
    addOnResultCB([this](AMFDecoder &dec) {
        //TraceL << "createStream result";
        dec.load<AMFValue>();
        _stream_index = dec.load<int>();
        send_publish();
    });
}

#define RTMP_STREAM_LIVE    "live"
void RtmpPusher::send_publish() {
    AMFEncoder enc;
    enc << "publish" << ++_send_req_id << nullptr << _stream_id << RTMP_STREAM_LIVE;
    sendRequest(MSG_CMD, enc.data());

    addOnStatusCB([this](AMFValue &val) {
        auto level = val["level"].as_string();
        auto code = val["code"].as_string();
        if (level != "status") {
            throw std::runtime_error(StrPrinter << "publish 失败:" << level << " " << code << endl);
        }
        //start send media
        send_metaData();
    });
}

void RtmpPusher::send_metaData(){
    auto src = _publish_src.lock();
    if (!src) {
        throw std::runtime_error("the media source was released");
    }

    // metadata
    src->getMetaData([&](const AMFValue &metadata) {
        AMFEncoder enc;
        enc << "@setDataFrame" << "onMetaData" << metadata;
        sendRequest(MSG_DATA, enc.data());
    });

    // config frame
    src->getConfigFrame([&](const RtmpPacket::Ptr &pkt) {
        sendRtmp(pkt->type_id, _stream_index, pkt, pkt->time_stamp, pkt->chunk_id);
    });

    src->pause(false);
    _rtmp_reader = src->getRing()->attach(getPoller());
    weak_ptr<RtmpPusher> weak_self = static_pointer_cast<RtmpPusher>(shared_from_this());
    _rtmp_reader->setReadCB([weak_self](const RtmpMediaSource::RingDataType &pkt) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }

        size_t i = 0;
        auto size = pkt->size();
        strong_self->setSendFlushFlag(false);
        pkt->for_each([&](const RtmpPacket::Ptr &rtmp) {
            if (++i == size) {
                strong_self->setSendFlushFlag(true);
            }
            if (rtmp->type_id == MSG_DATA) {
                // update metadata
                AMFEncoder enc;
                enc << "@setDataFrame";
                auto pkt = enc.data();
                pkt.append(rtmp->data(), rtmp->size());
                strong_self->sendRequest(MSG_DATA, pkt);
            } else {
                strong_self->sendRtmp(rtmp->type_id, strong_self->_stream_index, rtmp, rtmp->time_stamp, rtmp->chunk_id);
            }
        });
    });
    _rtmp_reader->setDetachCB([weak_self]() {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->onPublishResult_l(SockException(Err_other, "媒体源被释放"), !strong_self->_publish_timer);
        }
    });
    onPublishResult_l(SockException(Err_success, "success"), false);
    //提升发送性能
    setSocketFlags();
}

void RtmpPusher::setSocketFlags(){
    GET_CONFIG(int, mergeWriteMS, General::kMergeWriteMS);
    if (mergeWriteMS > 0) {
        //提高发送性能
        setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
        SockUtil::setNoDelay(getSock()->rawFD(), false);
    }
}

void RtmpPusher::onCmd_result(AMFDecoder &dec){
    auto req_id = dec.load<int>();
    auto it = _map_on_result.find(req_id);
    if (it != _map_on_result.end()) {
        it->second(dec);
        _map_on_result.erase(it);
    } else {
        WarnL << "unhandled _result";
    }
}

void RtmpPusher::onCmd_onStatus(AMFDecoder &dec) {
    AMFValue val;
    while (true) {
        val = dec.load<AMFValue>();
        if (val.type() == AMF_OBJECT) {
            break;
        }
    }
    if (val.type() != AMF_OBJECT) {
        throw std::runtime_error("onStatus:the result object was not found");
    }

    if (_deque_on_status.size()) {
        _deque_on_status.front()(val);
        _deque_on_status.pop_front();
    } else {
        auto level = val["level"];
        auto code = val["code"].as_string();
        if (level.type() == AMF_STRING) {
            if (level.as_string() != "status") {
                throw std::runtime_error(StrPrinter << "onStatus 失败:" << level.as_string() << " " << code << endl);
            }
        }
    }
}

void RtmpPusher::onRtmpChunk(RtmpPacket::Ptr packet) {
    auto &chunk_data = *packet;
    switch (chunk_data.type_id) {
        case MSG_CMD:
        case MSG_CMD3: {
            typedef void (RtmpPusher::*rtmpCMDHandle)(AMFDecoder &dec);
            static unordered_map<string, rtmpCMDHandle> g_mapCmd;
            static onceToken token([]() {
                g_mapCmd.emplace("_error", &RtmpPusher::onCmd_result);
                g_mapCmd.emplace("_result", &RtmpPusher::onCmd_result);
                g_mapCmd.emplace("onStatus", &RtmpPusher::onCmd_onStatus);
            });

            AMFDecoder dec(chunk_data.buffer, 0, chunk_data.type_id == MSG_CMD3 ? 3 : 0);
            std::string type = dec.load<std::string>();
            auto it = g_mapCmd.find(type);
            if (it != g_mapCmd.end()) {
                auto fun = it->second;
                (this->*fun)(dec);
            } else {
                WarnL << "can not support cmd:" << type;
            }
            break;
        }

        default:
            //WarnL << "unhandled message:" << (int) chunk_data.type_id << hexdump(chunk_data.buffer.data(), chunk_data.buffer.size());
            break;
    }
}


} /* namespace mediakit */

