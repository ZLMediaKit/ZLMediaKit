/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RTMPSESSION_H_
#define SRC_RTMP_RTMPSESSION_H_

#include <unordered_map>
#include "amf.h"
#include "Rtmp.h"
#include "utils.h"
#include "Common/config.h"
#include "RtmpProtocol.h"
#include "RtmpMediaSourceImp.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "Network/TcpSession.h"
#include "Common/Stamp.h"

using namespace toolkit;

namespace mediakit {

class RtmpSession: public TcpSession ,public  RtmpProtocol , public MediaSourceEvent{
public:
    typedef std::shared_ptr<RtmpSession> Ptr;
    RtmpSession(const Socket::Ptr &_sock);
    virtual ~RtmpSession();
    void onRecv(const Buffer::Ptr &pBuf) override;
    void onError(const SockException &err) override;
    void onManager() override;
private:
    void onProcessCmd(AMFDecoder &dec);
    void onCmd_connect(AMFDecoder &dec);
    void onCmd_createStream(AMFDecoder &dec);

    void onCmd_publish(AMFDecoder &dec);
    void onCmd_deleteStream(AMFDecoder &dec);

    void onCmd_play(AMFDecoder &dec);
    void onCmd_play2(AMFDecoder &dec);
    void doPlay(AMFDecoder &dec);
    void doPlayResponse(const string &err,const std::function<void(bool)> &cb);
    void sendPlayResponse(const string &err,const RtmpMediaSource::Ptr &src);

    void onCmd_seek(AMFDecoder &dec);
    void onCmd_pause(AMFDecoder &dec);
    void setMetaData(AMFDecoder &dec);

    void onSendMedia(const RtmpPacket::Ptr &pkt);
    void onSendRawData(const Buffer::Ptr &buffer) override{
        _ui64TotalBytes += buffer->size();
        send(buffer);
    }
    void onRtmpChunk(RtmpPacket &chunkData) override;

    template<typename first, typename second>
    inline void sendReply(const char *str, const first &reply, const second &status) {
        AMFEncoder invoke;
        invoke << str << _dNowReqID << reply << status;
        sendResponse(MSG_CMD, invoke.data());
    }

    //MediaSourceEvent override
    bool close(MediaSource &sender,bool force) override ;
    int totalReaderCount(MediaSource &sender) override;

    void setSocketFlags();
    string getStreamId(const string &str);
    void dumpMetadata(const AMFValue &metadata);
private:
    std::string _strTcUrl;
    MediaInfo _mediaInfo;
    double _dNowReqID = 0;
    bool _set_meta_data = false;
    Ticker _ticker;//数据接收时间
    RtmpMediaSource::RingType::RingReader::Ptr _pRingReader;
    std::shared_ptr<RtmpMediaSourceImp> _pPublisherSrc;
    std::weak_ptr<RtmpMediaSource> _pPlayerSrc;
    //时间戳修整器
    Stamp _stamp[2];
    //消耗的总流量
    uint64_t _ui64TotalBytes = 0;
    bool _paused = false;

};


/**
 * 支持ssl加密的rtmp服务器
 */
typedef TcpSessionWithSSL<RtmpSession> RtmpSessionWithSSL;

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPSESSION_H_ */
