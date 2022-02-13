/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

namespace mediakit {

//http-ts播发器，未实现ts解复用
class HttpTSPlayer : public HttpClientImp {
public:
    using Ptr = std::shared_ptr<HttpTSPlayer>;
    using onComplete = std::function<void(const toolkit::SockException &)>;

    HttpTSPlayer(const toolkit::EventPoller::Ptr &poller = nullptr);
    ~HttpTSPlayer() override = default;

    /**
     * 设置下载完毕或异常断开回调
     */
    void setOnComplete(onComplete cb);

    /**
     * 设置接收ts包回调
     */
    void setOnPacket(TSSegment::onSegment cb);

protected:
    ///HttpClient override///
    void onResponseHeader(const std::string &status, const HttpHeader &header) override;
    void onResponseBody(const char *buf, size_t size) override;
    void onResponseCompleted(const toolkit::SockException &ex) override;

private:
    void emitOnComplete(const toolkit::SockException &ex);

private:
    onComplete _on_complete;
    TSSegment::onSegment _on_segment;
};

}//namespace mediakit
#endif //HTTP_HTTPTSPLAYER_H
