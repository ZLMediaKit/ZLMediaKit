/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
    RtmpSession(const Socket::Ptr &sock);
    ~RtmpSession() override;

    void onRecv(const Buffer::Ptr &buf) override;
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
    void onSendRawData(Buffer::Ptr buffer) override{
        _total_bytes += buffer->size();
        send(std::move(buffer));
    }
    void onRtmpChunk(RtmpPacket::Ptr chunk_data) override;

    template<typename first, typename second>
    inline void sendReply(const char *str, const first &reply, const second &status) {
        AMFEncoder invoke;
        invoke << str << _recv_req_id << reply << status;
        sendResponse(MSG_CMD, invoke.data());
    }

    ///////MediaSourceEvent override///////
    // 关闭
    bool close(MediaSource &sender, bool force) override;
    // 播放总人数
    int totalReaderCount(MediaSource &sender) override;
    // 获取媒体源类型
    MediaOriginType getOriginType(MediaSource &sender) const override;
    // 获取媒体源url或者文件路径
    string getOriginUrl(MediaSource &sender) const override;
    // 获取媒体源客户端相关信息
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;

    void setSocketFlags();
    string getStreamId(const string &str);
    void dumpMetadata(const AMFValue &metadata);

private:
    bool _paused = false;
    bool _set_meta_data = false;
    double _recv_req_id = 0;
    //消耗的总流量
    uint64_t _total_bytes = 0;

    std::string _tc_url;
    //时间戳修整器
    Stamp _stamp[2];
    //数据接收超时计时器
    Ticker _ticker;
    MediaInfo _media_info;
    std::weak_ptr<RtmpMediaSource> _player_src;
    AMFValue _publisher_metadata;
    std::shared_ptr<RtmpMediaSourceImp> _publisher_src;
    RtmpMediaSource::RingType::RingReader::Ptr _ring_reader;
};

/**
 * 支持ssl加密的rtmp服务器
 */
typedef TcpSessionWithSSL<RtmpSession> RtmpSessionWithSSL;

} /* namespace mediakit */
#endif /* SRC_RTMP_RTMPSESSION_H_ */
