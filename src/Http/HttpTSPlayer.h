/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HTTP_HTTPTSPLAYER_H
#define HTTP_HTTPTSPLAYER_H

#include "Http/HttpDownloader.h"
#include "Player/MediaPlayer.h"
#include "Rtp/TSDecoder.h"
using namespace toolkit;
namespace mediakit {

//http-ts播发器，未实现ts解复用
class HttpTSPlayer : public HttpClientImp{
public:
    typedef function<void(const SockException &)> onShutdown;
    typedef std::shared_ptr<HttpTSPlayer> Ptr;

    HttpTSPlayer(const EventPoller::Ptr &poller = nullptr, bool split_ts = true);
    ~HttpTSPlayer() override ;

    //设置异常断开回调
    void setOnDisconnect(const onShutdown &cb);
    //设置接收ts包回调
    void setOnPacket(const TSSegment::onSegment &cb);

protected:
    ///HttpClient override///
    int64_t onResponseHeader(const string &status,const HttpHeader &headers) override;
    void onResponseBody(const char *buf,int64_t size,int64_t recvedSize,int64_t totalSize) override;
    void onResponseCompleted() override;
    void onDisconnect(const SockException &ex) override ;

    //收到ts包
    virtual void onPacket(const char *data, uint64_t len);

private:
    //是否为mpegts负载
    bool _is_ts_content = false;
    //第一个包是否为ts包
    bool _is_first_packet_ts = false;
    //是否判断是否是ts并split
    bool _split_ts;
    TSSegment _segment;
    onShutdown _on_disconnect;
    TSSegment::onSegment _on_segment;
};

}//namespace mediakit
#endif //HTTP_HTTPTSPLAYER_H
