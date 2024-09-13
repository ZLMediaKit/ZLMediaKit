/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_HTTPSESSION_H_
#define SRC_HTTP_HTTPSESSION_H_

#include <functional>
#include "Network/Session.h"
#include "Rtmp/FlvMuxer.h"
#include "HttpRequestSplitter.h"
#include "WebSocketSplitter.h"
#include "HttpCookieManager.h"
#include "HttpFileManager.h"
#include "TS/TSMediaSource.h"
#include "FMP4/FMP4MediaSource.h"

namespace mediakit {

class HttpSession: public toolkit::Session,
                   public FlvMuxer,
                   public HttpRequestSplitter,
                   public WebSocketSplitter {
public:
    using Ptr = std::shared_ptr<HttpSession>;
    using KeyValue = StrCaseMap;
    using HttpResponseInvoker = HttpResponseInvokerImp ;
    friend class AsyncSender;
    /**
     * @param errMsg 如果为空，则代表鉴权通过，否则为错误提示
     * @param accessPath 运行或禁止访问的根目录
     * @param cookieLifeSecond 鉴权cookie有效期
     * @param errMsg If empty, it means authentication passed, otherwise it is an error message
     * @param accessPath The root directory to run or prohibit access
     * @param cookieLifeSecond Authentication cookie validity period
     *
     * [AUTO-TRANSLATED:2e733a35]
     **/
    using HttpAccessPathInvoker = std::function<void(const std::string &errMsg,const std::string &accessPath, int cookieLifeSecond)>;

    HttpSession(const toolkit::Socket::Ptr &pSock);

    void onRecv(const toolkit::Buffer::Ptr &) override;
    void onError(const toolkit::SockException &err) override;
    void onManager() override;
    void setTimeoutSec(size_t second);
    void setMaxReqSize(size_t max_req_size);

protected:
    //FlvMuxer override
    void onWrite(const toolkit::Buffer::Ptr &data, bool flush) override ;
    void onDetach() override;
    std::shared_ptr<FlvMuxer> getSharedPtr() override;

    //HttpRequestSplitter override
    ssize_t onRecvHeader(const char *data,size_t len) override;
    void onRecvContent(const char *data,size_t len) override;

    /**
     * 重载之用于处理不定长度的content
     * 这个函数可用于处理大文件上传、http-flv推流
     * @param header http请求头
     * @param data content分片数据
     * @param len content分片数据大小
     * @param totalSize content总大小,如果为0则是不限长度content
     * @param recvedSize 已收数据大小
     * Overload for handling indefinite length content
     * This function can be used to handle large file uploads, http-flv streaming
     * @param header http request header
     * @param data content fragment data
     * @param len content fragment data size
     * @param totalSize total content size, if 0, it is unlimited length content
     * @param recvedSize received data size
     
     * [AUTO-TRANSLATED:ee75080d]
     */
    virtual void onRecvUnlimitedContent(const Parser &header,
                                        const char *data,
                                        size_t len,
                                        size_t totalSize,
                                        size_t recvedSize){
        shutdown(toolkit::SockException(toolkit::Err_shutdown,"http post content is too huge,default closed"));
    }

    /**
     * websocket客户端连接上事件
     * @param header http头
     * @return true代表允许websocket连接，否则拒绝
     * websocket client connection event
     * @param header http header
     * @return true means allow websocket connection, otherwise refuse
     
     * [AUTO-TRANSLATED:d857fb0f]
     */
    virtual bool onWebSocketConnect(const Parser &header){
        WarnP(this) << "http server do not support websocket default";
        return false;
    }

    //WebSocketSplitter override
    /**
     * 发送数据进行websocket协议打包后回调
     * @param buffer websocket协议数据
     * Callback after sending data for websocket protocol packaging
     * @param buffer websocket protocol data
     
     * [AUTO-TRANSLATED:48b3b028]
     */
    void onWebSocketEncodeData(toolkit::Buffer::Ptr buffer) override;

    /**
     * 接收到完整的一个webSocket数据包后回调
     * @param header 数据包包头
     * Callback after receiving a complete webSocket data packet
     * @param header data packet header
     
     * [AUTO-TRANSLATED:f506a7c5]
     */
    void onWebSocketDecodeComplete(const WebSocketHeader &header_in) override;

    // 重载获取客户端ip  [AUTO-TRANSLATED:6e497ea4]
    // Overload to get client ip
    std::string get_peer_ip() override;

private:
    void onHttpRequest_GET();
    void onHttpRequest_POST();
    void onHttpRequest_HEAD();
    void onHttpRequest_OPTIONS();

    bool checkLiveStream(const std::string &schema, const std::string  &url_suffix, const std::function<void(const MediaSource::Ptr &src)> &cb);

    bool checkLiveStreamFlv(const std::function<void()> &cb = nullptr);
    bool checkLiveStreamTS(const std::function<void()> &cb = nullptr);
    bool checkLiveStreamFMP4(const std::function<void()> &fmp4_list = nullptr);

    bool checkWebSocket();
    bool emitHttpEvent(bool doInvoke);
    void urlDecode(Parser &parser);
    void sendNotFound(bool bClose);
    void sendResponse(int code, bool bClose, const char *pcContentType = nullptr,
                      const HttpSession::KeyValue &header = HttpSession::KeyValue(),
                      const HttpBody::Ptr &body = nullptr, bool no_content_length = false);

    // 设置socket标志  [AUTO-TRANSLATED:4086e686]
    // Set socket flag
    void setSocketFlags();

protected:
    MediaInfo _media_info;

private:
    bool _is_live_stream = false;
    bool _live_over_websocket = false;
    // 超时时间  [AUTO-TRANSLATED:f15e2672]
    // Timeout
    size_t _keep_alive_sec = 0;
    // 最大http请求字节大小  [AUTO-TRANSLATED:c1fbc8e5]
    // Maximum http request byte size
    size_t _max_req_size = 0;
    // 消耗的总流量  [AUTO-TRANSLATED:45ad2785]
    // Total traffic consumed
    uint64_t _total_bytes_usage = 0;
    // http请求中的 Origin字段  [AUTO-TRANSLATED:7b8dd2c0]
    // Origin field in http request
    std::string _origin;
    Parser _parser;
    toolkit::Ticker _ticker;
    TSMediaSource::RingType::RingReader::Ptr _ts_reader;
    FMP4MediaSource::RingType::RingReader::Ptr _fmp4_reader;
    // 处理content数据的callback  [AUTO-TRANSLATED:38890e8d]
    // Callback to handle content data
    std::function<bool (const char *data,size_t len) > _on_recv_body;
};

using HttpsSession = toolkit::SessionWithSSL<HttpSession>;

} /* namespace mediakit */

#endif /* SRC_HTTP_HTTPSESSION_H_ */
