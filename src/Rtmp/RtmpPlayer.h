/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RtmpPlayer2_H_
#define SRC_RTMP_RtmpPlayer2_H_

#include <memory>
#include <string>
#include <functional>
#include "amf.h"
#include "Rtmp.h"
#include "RtmpProtocol.h"
#include "Player/PlayerBase.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Network/Socket.h"
#include "Network/TcpClient.h"

using namespace toolkit;
using namespace mediakit::Client;

namespace mediakit {
//实现了rtmp播放器协议部分的功能，及数据接收功能
class RtmpPlayer:public PlayerBase, public TcpClient,  public RtmpProtocol{
public:
    typedef std::shared_ptr<RtmpPlayer> Ptr;
    RtmpPlayer(const EventPoller::Ptr &poller);
    virtual ~RtmpPlayer();

    void play(const string &strUrl) override;
    void pause(bool bPause) override;
    void teardown() override;
protected:
    virtual bool onCheckMeta(const AMFValue &val) =0;
    virtual void onMediaData(const RtmpPacket::Ptr &chunkData) =0;
    uint32_t getProgressMilliSecond() const;
    void seekToMilliSecond(uint32_t ms);
protected:
    void onMediaData_l(const RtmpPacket::Ptr &chunkData);
    //在获取config帧后才触发onPlayResult_l(而不是收到play命令回复)，所以此时所有track都初始化完毕了
    void onPlayResult_l(const SockException &ex, bool handshakeCompleted);

    //form Tcpclient
    void onRecv(const Buffer::Ptr &pBuf) override;
    void onConnect(const SockException &err) override;
    void onErr(const SockException &ex) override;
    //from RtmpProtocol
    void onRtmpChunk(RtmpPacket &chunkData) override;
    void onStreamDry(uint32_t ui32StreamId) override;
    void onSendRawData(const Buffer::Ptr &buffer) override{
        send(buffer);
    }

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
    inline void send_play();
    inline void send_pause(bool bPause);
private:
    string _strApp;
    string _strStream;
    string _strTcUrl;
    bool _bPaused = false;

    unordered_map<int, function<void(AMFDecoder &dec)> > _mapOnResultCB;
    recursive_mutex _mtxOnResultCB;
    deque<function<void(AMFValue &dec)> > _dqOnStatusCB;
    recursive_mutex _mtxOnStatusCB;

    //超时功能实现
    Ticker _mediaTicker;
    std::shared_ptr<Timer> _pMediaTimer;
    std::shared_ptr<Timer> _pPlayTimer;
    //心跳定时器
    std::shared_ptr<Timer> _pBeatTimer;

    //播放进度控制
    uint32_t _iSeekTo = 0;
    uint32_t _aiFistStamp[2] = { 0, 0 };
    uint32_t _aiNowStamp[2] = { 0, 0 };
    Ticker _aNowStampTicker[2];
    bool _metadata_got = false;
    //是否为性能测试模式
    bool _benchmark_mode = false;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RtmpPlayer2_H_ */
