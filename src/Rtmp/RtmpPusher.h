/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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

class RtmpPusher: public RtmpProtocol , public TcpClient , public PusherBase{
public:
    typedef std::shared_ptr<RtmpPusher> Ptr;
    RtmpPusher(const EventPoller::Ptr &poller,const RtmpMediaSource::Ptr &src);
    virtual ~RtmpPusher();

    void publish(const string &strUrl) override ;

    void teardown() override;

    void setOnPublished(const Event &cb) override {
        _onPublished = cb;
    }

    void setOnShutdown(const Event &cb) override{
        _onShutdown = cb;
    }
protected:
    //for Tcpclient override
    void onRecv(const Buffer::Ptr &pBuf) override;
    void onConnect(const SockException &err) override;
    void onErr(const SockException &ex) override;

    //for RtmpProtocol override
    void onRtmpChunk(RtmpPacket &chunkData) override;
    void onSendRawData(const Buffer::Ptr &buffer) override{
        send(buffer);
    }
private:
    void onPublishResult(const SockException &ex,bool handshakeCompleted);

    template<typename FUN>
    inline void addOnResultCB(const FUN &fun) {
        lock_guard<recursive_mutex> lck(_mtxOnResultCB);
        _mapOnResultCB.emplace(_iReqID, fun);
    }
    template<typename FUN>
    inline void addOnStatusCB(const FUN &fun) {
        lock_guard<recursive_mutex> lck(_mtxOnStatusCB);
        _dqOnStatusCB.emplace_back(fun);
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
    string _strApp;
    string _strStream;
    string _strTcUrl;

    unordered_map<int, function<void(AMFDecoder &dec)> > _mapOnResultCB;
    recursive_mutex _mtxOnResultCB;
    deque<function<void(AMFValue &dec)> > _dqOnStatusCB;
    recursive_mutex _mtxOnStatusCB;
    //超时功能实现
    std::shared_ptr<Timer> _pPublishTimer;
    //源
    std::weak_ptr<RtmpMediaSource> _pMediaSrc;
    RtmpMediaSource::RingType::RingReader::Ptr _pRtmpReader;
    //事件监听
    Event _onShutdown;
    Event _onPublished;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPUSHER_H_ */
