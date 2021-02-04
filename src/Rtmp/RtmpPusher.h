/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RTMPPUSHER_H_
#define SRC_RTMP_RTMPPUSHER_H_

#include "RtmpProtocol.h"
#include "RtmpMediaSource.h"
#include "Network/TcpClient.h"
#include "Pusher/PusherBase.h"

namespace mediakit {

class RtmpPusher : public RtmpProtocol, public TcpClient, public PusherBase {
public:
    typedef std::shared_ptr<RtmpPusher> Ptr;
    RtmpPusher(const EventPoller::Ptr &poller,const RtmpMediaSource::Ptr &src);
    ~RtmpPusher() override;

    void publish(const string &url) override ;
    void teardown() override;

    void setOnPublished(const Event &cb) override {
        _on_published = cb;
    }

    void setOnShutdown(const Event &cb) override{
        _on_shutdown = cb;
    }

protected:
    //for Tcpclient override
    void onRecv(const Buffer::Ptr &buf) override;
    void onConnect(const SockException &err) override;
    void onErr(const SockException &ex) override;

    //for RtmpProtocol override
    void onRtmpChunk(RtmpPacket::Ptr chunk_data) override;
    void onSendRawData(Buffer::Ptr buffer) override{
        send(std::move(buffer));
    }

private:
    void onPublishResult(const SockException &ex, bool handshake_done);

    template<typename FUN>
    inline void addOnResultCB(const FUN &fun) {
        lock_guard<recursive_mutex> lck(_mtx_on_result);
        _map_on_result.emplace(_send_req_id, fun);
    }
    template<typename FUN>
    inline void addOnStatusCB(const FUN &fun) {
        lock_guard<recursive_mutex> lck(_mtx_on_status);
        _deque_on_status.emplace_back(fun);
    }

    void onCmd_result(AMFDecoder &dec);
    void onCmd_onStatus(AMFDecoder &dec);
    void onCmd_onMetaData(AMFDecoder &dec);

    inline void send_connect();
    inline void send_createStream();
    inline void send_publish();
    inline void send_metaData();
    void setSocketFlags();

private:
    string _app;
    string _stream_id;
    string _tc_url;

    recursive_mutex _mtx_on_result;
    recursive_mutex _mtx_on_status;
    deque<function<void(AMFValue &dec)> > _deque_on_status;
    unordered_map<int, function<void(AMFDecoder &dec)> > _map_on_result;

    //事件监听
    Event _on_shutdown;
    Event _on_published;

    //推流超时定时器
    std::shared_ptr<Timer> _publish_timer;
    std::weak_ptr<RtmpMediaSource> _publish_src;
    RtmpMediaSource::RingType::RingReader::Ptr _rtmp_reader;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPUSHER_H_ */
