/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if !defined(_WIN32)
#include <dirent.h>
#endif //!defined(_WIN32)

#include <stdio.h>
#include <sys/stat.h>
#include <algorithm>
#include <iomanip>

#include "Common/config.h"
#include "strCoding.h"
#include "HttpSession.h"
#include "Util/base64.h"
#include "Util/SHA1.h"
using namespace toolkit;

namespace mediakit {

HttpSession::HttpSession(const Socket::Ptr &pSock) : TcpSession(pSock) {
    TraceP(this);
    GET_CONFIG(uint32_t,keep_alive_sec,Http::kKeepAliveSecond);
    pSock->setSendTimeOutSecond(keep_alive_sec);
    //起始接收buffer缓存设置为4K，节省内存
    pSock->setReadBuffer(std::make_shared<BufferRaw>(4 * 1024));
}

HttpSession::~HttpSession() {
    TraceP(this);
}

void HttpSession::Handle_Req_HEAD(int64_t &content_len){
    //暂时全部返回200 OK，因为HTTP GET存在按需生成流的操作，所以不能按照HTTP GET的流程返回
    //如果直接返回404，那么又会导致按需生成流的逻辑失效，所以HTTP HEAD在静态文件或者已存在资源时才有效
    //对于按需生成流的直播场景并不适用
    sendResponse("200 OK", true);
}

int64_t HttpSession::onRecvHeader(const char *header,uint64_t len) {
    typedef void (HttpSession::*HttpCMDHandle)(int64_t &);
    static unordered_map<string, HttpCMDHandle> s_func_map;
    static onceToken token([]() {
        s_func_map.emplace("GET",&HttpSession::Handle_Req_GET);
        s_func_map.emplace("POST",&HttpSession::Handle_Req_POST);
        s_func_map.emplace("HEAD",&HttpSession::Handle_Req_HEAD);
    }, nullptr);

    _parser.Parse(header);
    urlDecode(_parser);
    string cmd = _parser.Method();
    auto it = s_func_map.find(cmd);
    if (it == s_func_map.end()) {
        WarnP(this) << "不支持该命令:" << cmd;
        sendResponse("405 Not Allowed", true);
        return 0;
    }

    //跨域
    _origin = _parser["Origin"];

    //默认后面数据不是content而是header
    int64_t content_len = 0;
    auto &fun = it->second;
    try {
        (this->*fun)(content_len);
    }catch (exception &ex){
        shutdown(SockException(Err_shutdown,ex.what()));
    }

    //清空解析器节省内存
    _parser.Clear();
    //返回content长度
    return content_len;
}

void HttpSession::onRecvContent(const char *data,uint64_t len) {
    if(_contentCallBack){
        if(!_contentCallBack(data,len)){
            _contentCallBack = nullptr;
        }
    }
}

void HttpSession::onRecv(const Buffer::Ptr &pBuf) {
    _ticker.resetTime();
    input(pBuf->data(),pBuf->size());
}

void HttpSession::onError(const SockException& err) {
    if(_is_flv_stream){
        uint64_t duration = _ticker.createdTime()/1000;
        //flv播放器
        WarnP(this) << "FLV播放器("
                    << _mediaInfo._vhost << "/"
                    << _mediaInfo._app << "/"
                    << _mediaInfo._streamid
                    << ")断开:" << err.what()
                    << ",耗时(s):" << duration;

        GET_CONFIG(uint32_t,iFlowThreshold,General::kFlowThreshold);
        if(_ui64TotalBytes > iFlowThreshold * 1024){
            NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _mediaInfo, _ui64TotalBytes, duration , true, static_cast<SockInfo &>(*this));
        }
        return;
    }

    //http客户端
    if(_ticker.createdTime() < 10 * 1000){
        TraceP(this) << err.what();
    }else{
        WarnP(this) << err.what();
    }
}

void HttpSession::onManager() {
    GET_CONFIG(uint32_t,keepAliveSec,Http::kKeepAliveSecond);

    if(_ticker.elapsedTime() > keepAliveSec * 1000){
        //1分钟超时
        shutdown(SockException(Err_timeout,"session timeouted"));
    }
}

bool HttpSession::checkWebSocket(){
    auto Sec_WebSocket_Key = _parser["Sec-WebSocket-Key"];
    if(Sec_WebSocket_Key.empty()){
        return false;
    }
    auto Sec_WebSocket_Accept = encodeBase64(SHA1::encode_bin(Sec_WebSocket_Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

    KeyValue headerOut;
    headerOut["Upgrade"] = "websocket";
    headerOut["Connection"] = "Upgrade";
    headerOut["Sec-WebSocket-Accept"] = Sec_WebSocket_Accept;
    if(!_parser["Sec-WebSocket-Protocol"].empty()){
        headerOut["Sec-WebSocket-Protocol"] = _parser["Sec-WebSocket-Protocol"];
    }

    auto res_cb = [this,headerOut](){
        _flv_over_websocket = true;
        sendResponse("101 Switching Protocols",false,nullptr,headerOut,nullptr, true);
    };

    //判断是否为websocket-flv
    if(checkLiveFlvStream(res_cb)){
        //这里是websocket-flv直播请求
        return true;
    }

    //如果checkLiveFlvStream返回false,则代表不是websocket-flv，而是普通的websocket连接
    if(!onWebSocketConnect(_parser)){
        sendResponse("501 Not Implemented",true, nullptr, headerOut);
        return true;
    }
    sendResponse("101 Switching Protocols",false, nullptr,headerOut);
    return true;
}

//http-flv 链接格式:http://vhost-url:port/app/streamid.flv?key1=value1&key2=value2
//如果url(除去?以及后面的参数)后缀是.flv,那么表明该url是一个http-flv直播。
bool HttpSession::checkLiveFlvStream(const function<void()> &cb){
    auto pos = strrchr(_parser.Url().data(),'.');
    if(!pos){
        //未找到".flv"后缀
        return false;
    }
    if(strcasecmp(pos,".flv") != 0){
        //未找到".flv"后缀
        return false;
    }

    //这是个.flv的流
    _mediaInfo.parse(string(RTMP_SCHEMA) + "://" + _parser["Host"] + _parser.FullUrl());
    if(_mediaInfo._app.empty() || _mediaInfo._streamid.size() < 5){
        //url不合法
        return false;
    }
    _mediaInfo._streamid.erase(_mediaInfo._streamid.size() - 4);//去除.flv后缀
    bool bClose = !strcasecmp(_parser["Connection"].data(),"close");

    weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());

    //鉴权结果回调
    auto onRes = [cb, weakSelf, bClose](const string &err){
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            //本对象已经销毁
            return;
        }

        if(!err.empty()){
            //播放鉴权失败
            strongSelf->sendResponse("401 Unauthorized", bClose, nullptr, KeyValue(), std::make_shared<HttpStringBody>(err));
            return;
        }

        //异步查找rtmp流
        MediaSource::findAsync(strongSelf->_mediaInfo, strongSelf, [weakSelf, bClose, cb](const MediaSource::Ptr &src) {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                //本对象已经销毁
                return;
            }
            auto rtmp_src = dynamic_pointer_cast<RtmpMediaSource>(src);
            if (!rtmp_src) {
                //未找到该流
                strongSelf->sendNotFound(bClose);
                return;
            }

            if (!cb) {
                //找到rtmp源，发送http头，负载后续发送
                strongSelf->sendResponse("200 OK", false, "video/x-flv", KeyValue(), nullptr, true);
            } else {
                //自定义发送http头
                cb();
            }

            //http-flv直播牺牲延时提升发送性能
            strongSelf->setSocketFlags();
            strongSelf->start(strongSelf->getPoller(), rtmp_src);
            strongSelf->_is_flv_stream = true;
        });
    };

    Broadcast::AuthInvoker invoker = [weakSelf, onRes](const string &err) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->async([onRes, err]() {
            onRes(err);
        });
    };

    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed, _mediaInfo, invoker, static_cast<SockInfo &>(*this));
    if (!flag) {
        //该事件无人监听,默认不鉴权
        onRes("");
    }
    return true;
}

void HttpSession::Handle_Req_GET(int64_t &content_len) {
    Handle_Req_GET_l(content_len, true);
}

void HttpSession::Handle_Req_GET_l(int64_t &content_len, bool sendBody) {
    //先看看是否为WebSocket请求
    if(checkWebSocket()){
        content_len = -1;
        _contentCallBack = [this](const char *data,uint64_t len){
            WebSocketSplitter::decode((uint8_t *)data,len);
            //_contentCallBack是可持续的，后面还要处理后续数据
            return true;
        };
        return;
    }

    if(emitHttpEvent(false)){
        //拦截http api事件
        return;
    }

    if(checkLiveFlvStream()){
        //拦截http-flv播放器
        return;
    }

    bool bClose = !strcasecmp(_parser["Connection"].data(),"close");

    weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
    HttpFileManager::onAccessPath(*this, _parser, [weakSelf, bClose](const string &status_code, const string &content_type,
                                                                     const StrCaseMap &responseHeader, const HttpBody::Ptr &body) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->async([weakSelf, bClose, status_code, content_type, responseHeader, body]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            strongSelf->sendResponse(status_code.data(), bClose, content_type.data(), responseHeader, body);
        });
    });
}

static string dateStr() {
    char buf[64];
    time_t tt = time(NULL);
    strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

class AsyncSenderData {
public:
    friend class AsyncSender;
    typedef std::shared_ptr<AsyncSenderData> Ptr;
    AsyncSenderData(const TcpSession::Ptr &session, const HttpBody::Ptr &body, bool close_when_complete) {
        _session = dynamic_pointer_cast<HttpSession>(session);
        _body = body;
        _close_when_complete = close_when_complete;
    }
    ~AsyncSenderData() = default;
private:
    std::weak_ptr<HttpSession> _session;
    HttpBody::Ptr _body;
    bool _close_when_complete;
    bool _read_complete = false;
};

class AsyncSender {
public:
    typedef std::shared_ptr<AsyncSender> Ptr;
    static bool onSocketFlushed(const AsyncSenderData::Ptr &data) {
        if (data->_read_complete) {
            if (data->_close_when_complete) {
                //发送完毕需要关闭socket
                shutdown(data->_session.lock());
            }
            return false;
        }

        GET_CONFIG(uint32_t, sendBufSize, Http::kSendBufSize);
        data->_body->readDataAsync(sendBufSize, [data](const Buffer::Ptr &sendBuf) {
            auto session = data->_session.lock();
            if (!session) {
                //本对象已经销毁
                return;
            }
            session->async([data, sendBuf]() {
                auto session = data->_session.lock();
                if (!session) {
                    //本对象已经销毁
                    return;
                }
                onRequestData(data, session, sendBuf);
            }, false);
        });
        return true;
    }

private:
    static void onRequestData(const AsyncSenderData::Ptr &data, const std::shared_ptr<HttpSession> &session, const Buffer::Ptr &sendBuf) {
        session->_ticker.resetTime();
        if (sendBuf && session->send(sendBuf) != -1) {
            //文件还未读完，还需要继续发送
            if (!session->isSocketBusy()) {
                //socket还可写，继续请求数据
                onSocketFlushed(data);
            }
            return;
        }
        //文件写完了
        data->_read_complete = true;
        if (!session->isSocketBusy() && data->_close_when_complete) {
            shutdown(session);
        }
    }

    static void shutdown(const std::shared_ptr<HttpSession> &session) {
        if(session){
            session->shutdown(SockException(Err_shutdown, StrPrinter << "close connection after send http body completed."));
        }
    }
};

static const string kDate = "Date";
static const string kServer = "Server";
static const string kConnection = "Connection";
static const string kKeepAlive = "Keep-Alive";
static const string kContentType = "Content-Type";
static const string kContentLength = "Content-Length";
static const string kAccessControlAllowOrigin = "Access-Control-Allow-Origin";
static const string kAccessControlAllowCredentials = "Access-Control-Allow-Credentials";

void HttpSession::sendResponse(const char *pcStatus,
                               bool bClose,
                               const char *pcContentType,
                               const HttpSession::KeyValue &header,
                               const HttpBody::Ptr &body,
                               bool is_http_flv ){
    GET_CONFIG(string,charSet,Http::kCharSet);
    GET_CONFIG(uint32_t,keepAliveSec,Http::kKeepAliveSecond);

    //body默认为空
    int64_t size = 0;
    if (body && body->remainSize()) {
        //有body，获取body大小
        size = body->remainSize();
    }

    if(is_http_flv){
        //http-flv直播是Keep-Alive类型
        bClose = false;
    }else if(size >= INT64_MAX){
        //不固定长度的body，那么发送完body后应该关闭socket，以便浏览器做下载完毕的判断
        bClose = true;
    }

    HttpSession::KeyValue &headerOut = const_cast<HttpSession::KeyValue &>(header);
    headerOut.emplace(kDate, dateStr());
    headerOut.emplace(kServer, SERVER_NAME);
    headerOut.emplace(kConnection, bClose ? "close" : "keep-alive");
    if(!bClose){
        string keepAliveString = "timeout=";
        keepAliveString += to_string(keepAliveSec);
        keepAliveString += ", max=100";
        headerOut.emplace(kKeepAlive,std::move(keepAliveString));
    }

    if(!_origin.empty()){
        //设置跨域
        headerOut.emplace(kAccessControlAllowOrigin,_origin);
        headerOut.emplace(kAccessControlAllowCredentials, "true");
    }

    if(!is_http_flv && size >= 0 && size < INT64_MAX){
        //文件长度为固定值,且不是http-flv强制设置Content-Length
        headerOut[kContentLength] = to_string(size);
    }

    if(size && !pcContentType){
        //有body时，设置缺省类型
        pcContentType = "text/plain";
    }

    if(size && pcContentType){
        //有body时，设置文件类型
        string strContentType = pcContentType;
        strContentType += "; charset=";
        strContentType += charSet;
        headerOut.emplace(kContentType,std::move(strContentType));
    }

    //发送http头
    string str;
    str.reserve(256);
    str += "HTTP/1.1 " ;
    str += pcStatus ;
    str += "\r\n";
    for (auto &pr : header) {
        str += pr.first ;
        str += ": ";
        str += pr.second;
        str += "\r\n";
    }
    str += "\r\n";
    SockSender::send(std::move(str));
    _ticker.resetTime();

    if(!size){
        //没有body
        if(bClose){
            shutdown(SockException(Err_shutdown,StrPrinter << "close connection after send http header completed with status code:" << pcStatus));
        }
        return;
    }

    GET_CONFIG(uint32_t, sendBufSize, Http::kSendBufSize);
    if(body->remainSize() > sendBufSize){
        //文件下载提升发送性能
        setSocketFlags();
    }

    //发送http body
    AsyncSenderData::Ptr data = std::make_shared<AsyncSenderData>(shared_from_this(),body,bClose);
    _sock->setOnFlush([data](){
        return AsyncSender::onSocketFlushed(data);
    });
    AsyncSender::onSocketFlushed(data);
}

string HttpSession::urlDecode(const string &str){
    auto ret = strCoding::UrlDecode(str);
#ifdef _WIN32
    GET_CONFIG(string,charSet,Http::kCharSet);
    bool isGb2312 = !strcasecmp(charSet.data(), "gb2312");
    if (isGb2312) {
        ret = strCoding::UTF8ToGB2312(ret);
    }
#endif // _WIN32
    return ret;
}

void HttpSession::urlDecode(Parser &parser){
    parser.setUrl(urlDecode(parser.Url()));
    for(auto &pr : _parser.getUrlArgs()){
        const_cast<string &>(pr.second) = urlDecode(pr.second);
    }
}

bool HttpSession::emitHttpEvent(bool doInvoke){
    bool bClose = !strcasecmp(_parser["Connection"].data(),"close");
    /////////////////////异步回复Invoker///////////////////////////////
    weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
    HttpResponseInvoker invoker = [weakSelf,bClose](const string &codeOut, const KeyValue &headerOut, const HttpBody::Ptr &body){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf) {
            return;
        }
        strongSelf->async([weakSelf,bClose,codeOut,headerOut,body]() {
            auto strongSelf = weakSelf.lock();
            if(!strongSelf) {
                //本对象已经销毁
                return;
            }
            strongSelf->sendResponse(codeOut.data(), bClose, nullptr, headerOut, body);
        });
    };
    ///////////////////广播HTTP事件///////////////////////////
    bool consumed = false;//该事件是否被消费
    NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastHttpRequest,_parser,invoker,consumed,static_cast<SockInfo &>(*this));
    if(!consumed && doInvoke){
        //该事件无人消费，所以返回404
        invoker("404 Not Found",KeyValue(), HttpBody::Ptr());
    }
    return consumed;
}

void HttpSession::Handle_Req_POST(int64_t &content_len) {
    GET_CONFIG(uint64_t,maxReqSize,Http::kMaxReqSize);

    int64_t totalContentLen = _parser["Content-Length"].empty() ? -1 : atoll(_parser["Content-Length"].data());

    if(totalContentLen == 0){
        //content为空
        //emitHttpEvent内部会选择是否关闭连接
        emitHttpEvent(true);
        return;
    }

    //根据Content-Length设置接收缓存大小
    if(totalContentLen > 0){
        _sock->setReadBuffer(std::make_shared<BufferRaw>(MIN(totalContentLen + 1,256 * 1024)));
    }else{
        //不定长度的Content-Length
        _sock->setReadBuffer(std::make_shared<BufferRaw>(256 * 1024));
    }

    if(totalContentLen > 0 && totalContentLen < maxReqSize ){
        //返回固定长度的content
        content_len = totalContentLen;
        auto parserCopy = _parser;
        _contentCallBack = [this,parserCopy](const char *data,uint64_t len){
            //恢复http头
            _parser = parserCopy;
            //设置content
            _parser.setContent(string(data,len));
            //触发http事件，emitHttpEvent内部会选择是否关闭连接
            emitHttpEvent(true);
            //清空数据,节省内存
            _parser.Clear();
            //content已经接收完毕
            return false;
        };
    }else{
        //返回不固定长度的content
        content_len = -1;
        auto parserCopy = _parser;
        std::shared_ptr<uint64_t> recvedContentLen = std::make_shared<uint64_t>(0);
        bool bClose = !strcasecmp(_parser["Connection"].data(),"close");

        _contentCallBack = [this,parserCopy,totalContentLen,recvedContentLen,bClose](const char *data,uint64_t len){
            *(recvedContentLen) += len;

            onRecvUnlimitedContent(parserCopy,data,len,totalContentLen,*(recvedContentLen));

            if(*(recvedContentLen) < totalContentLen){
                //数据还没接收完毕
                //_contentCallBack是可持续的，后面还要处理后续content数据
                return true;
            }

            //数据接收完毕
            if(!bClose){
                //keep-alive类型连接
                //content接收完毕，后续都是http header
                setContentLen(0);
                //content已经接收完毕
                return false;
            }

            //连接类型是close类型，收完content就关闭连接
            shutdown(SockException(Err_shutdown,"recv http content completed"));
            //content已经接收完毕
            return false ;
        };
    }
    //有后续content数据要处理,暂时不关闭连接
}

void HttpSession::sendNotFound(bool bClose) {
    GET_CONFIG(string,notFound,Http::kNotFound);
    sendResponse("404 Not Found", bClose,"text/html",KeyValue(),std::make_shared<HttpStringBody>(notFound));
}

void HttpSession::setSocketFlags(){
    GET_CONFIG(int, mergeWriteMS, General::kMergeWriteMS);
    if(mergeWriteMS > 0) {
        //推流模式下，关闭TCP_NODELAY会增加推流端的延时，但是服务器性能将提高
        SockUtil::setNoDelay(_sock->rawFD(), false);
        //播放模式下，开启MSG_MORE会增加延时，但是能提高发送性能
        setSendFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
    }
}

void HttpSession::onWrite(const Buffer::Ptr &buffer, bool flush) {
    if(flush){
        //需要flush那么一次刷新缓存
        HttpSession::setSendFlushFlag(true);
    }

    _ticker.resetTime();
    if(!_flv_over_websocket){
        _ui64TotalBytes += buffer->size();
        send(buffer);
    }else{
        WebSocketHeader header;
        header._fin = true;
        header._reserved = 0;
        header._opcode = WebSocketHeader::BINARY;
        header._mask_flag = false;
        WebSocketSplitter::encode(header,buffer);
    }

    if(flush){
        //本次刷新缓存后，下次不用刷新缓存
        HttpSession::setSendFlushFlag(false);
    }
}

void HttpSession::onWebSocketEncodeData(const Buffer::Ptr &buffer){
    _ui64TotalBytes += buffer->size();
    send(buffer);
}

void HttpSession::onDetach() {
    shutdown(SockException(Err_shutdown,"rtmp ring buffer detached"));
}

std::shared_ptr<FlvMuxer> HttpSession::getSharedPtr(){
    return dynamic_pointer_cast<FlvMuxer>(shared_from_this());
}

} /* namespace mediakit */
