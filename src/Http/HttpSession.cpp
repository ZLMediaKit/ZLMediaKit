/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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

int64_t HttpSession::onRecvHeader(const char *header,uint64_t len) {
	typedef void (HttpSession::*HttpCMDHandle)(int64_t &);
	static unordered_map<string, HttpCMDHandle> s_func_map;
	static onceToken token([]() {
		s_func_map.emplace("GET",&HttpSession::Handle_Req_GET);
		s_func_map.emplace("POST",&HttpSession::Handle_Req_POST);
	}, nullptr);

	_parser.Parse(header);
	urlDecode(_parser);
	string cmd = _parser.Method();
	auto it = s_func_map.find(cmd);
	if (it == s_func_map.end()) {
		sendResponse("403 Forbidden", true);
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
        //flv播放器
        WarnP(this) << "播放器("
                    << _mediaInfo._vhost << "/"
                    << _mediaInfo._app << "/"
                    << _mediaInfo._streamid
                    << ")断开:" << err.what();

        GET_CONFIG(uint32_t,iFlowThreshold,General::kFlowThreshold);
        if(_ui64TotalBytes > iFlowThreshold * 1024){
            NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport,
                                               _mediaInfo,
                                               _ui64TotalBytes,
                                               _ticker.createdTime()/1000,
                                               true,
                                               *this);
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
        sendResponse("101 Switching Protocols",false,nullptr,headerOut,nullptr,false);
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

    GET_CONFIG(uint32_t,reqCnt,Http::kMaxReqCount);
    bool bClose = (strcasecmp(_parser["Connection"].data(),"close") == 0) || ( ++_iReqCnt > reqCnt);

    weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
    MediaSource::findAsync(_mediaInfo,weakSelf.lock(), true,[weakSelf,bClose,this,cb](const MediaSource::Ptr &src){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            //本对象已经销毁
            return;
        }
        auto rtmp_src = dynamic_pointer_cast<RtmpMediaSource>(src);
        if(!rtmp_src){
            //未找到该流
            sendNotFound(bClose);
            return;
        }
        //找到流了
        auto onRes = [this,rtmp_src,cb](const string &err){
            bool authSuccess = err.empty();
            if(!authSuccess){
                sendResponse("401 Unauthorized", true, nullptr, KeyValue(), std::make_shared<HttpStringBody>(err));
                return ;
            }

            if(!cb) {
                //找到rtmp源，发送http头，负载后续发送
                sendResponse("200 OK", false, "video/x-flv",KeyValue(),nullptr,false);
            }else{
                cb();
            }

            //http-flv直播牺牲延时提升发送性能
            setSocketFlags();

            try{
                start(getPoller(),rtmp_src);
                _is_flv_stream = true;
            }catch (std::exception &ex){
                //该rtmp源不存在
                shutdown(SockException(Err_shutdown,"rtmp mediasource released"));
            }
        };

        weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
        Broadcast::AuthInvoker invoker = [weakSelf,onRes](const string &err){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf){
                return;
            }
            strongSelf->async([weakSelf,onRes,err](){
                auto strongSelf = weakSelf.lock();
                if(!strongSelf){
                    return;
                }
                onRes(err);
            });
        };
        auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed,_mediaInfo,invoker,*this);
        if(!flag){
            //该事件无人监听,默认不鉴权
            onRes("");
        }
    });
    return true;
}


void HttpSession::Handle_Req_GET(int64_t &content_len) {
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

    GET_CONFIG(uint32_t,reqCnt,Http::kMaxReqCount);
    bool bClose = (strcasecmp(_parser["Connection"].data(),"close") == 0) || ( ++_iReqCnt > reqCnt);

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
            strongSelf->sendResponse(status_code.data(), bClose,
                                     content_type.empty() ? nullptr : content_type.data(),
                                     responseHeader, body);
        });
    });
}

static string dateStr() {
    char buf[64];
    time_t tt = time(NULL);
    strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
    return buf;
}

void HttpSession::sendResponse(const char *pcStatus,
                               bool bClose,
                               const char *pcContentType,
                               const HttpSession::KeyValue &header,
                               const HttpBody::Ptr &body,
                               bool set_content_len ){
    GET_CONFIG(string,charSet,Http::kCharSet);
    GET_CONFIG(uint32_t,keepAliveSec,Http::kKeepAliveSecond);
    GET_CONFIG(uint32_t,reqCnt,Http::kMaxReqCount);

    //body默认为空
    int64_t size = 0;
    if (body && body->remainSize()) {
        //有body，获取body大小
        size = body->remainSize();
        if (size >= INT64_MAX) {
            //不固定长度的body，那么不设置content-length字段
            size = -1;
        }
    }

    if(!set_content_len || size == -1){
        //如果是不定长度body，或者不设置conten-length,
        //那么一定是Keep-Alive类型
        bClose = false;
    }

    HttpSession::KeyValue &headerOut = const_cast<HttpSession::KeyValue &>(header);
    headerOut.emplace("Date", dateStr());
    headerOut.emplace("Server", SERVER_NAME);
    headerOut.emplace("Connection", bClose ? "close" : "keep-alive");
    if(!bClose){
        headerOut.emplace("Keep-Alive",StrPrinter << "timeout=" << keepAliveSec << ", max=" << reqCnt << endl);
    }

    if(!_origin.empty()){
        //设置跨域
        headerOut.emplace("Access-Control-Allow-Origin",_origin);
        headerOut.emplace("Access-Control-Allow-Credentials", "true");
    }

    if(set_content_len && size >= 0){
        //文件长度为定值或者,且不是http-flv强制设置Content-Length
        headerOut["Content-Length"] = StrPrinter << size << endl;
    }

    if(size && !pcContentType){
        //有body时，设置缺省类型
        pcContentType = "text/plain";
    }

    if(size && pcContentType){
        //有body时，设置文件类型
        auto strContentType = StrPrinter << pcContentType << "; charset=" << charSet << endl;
        headerOut.emplace("Content-Type",strContentType);
    }

    //发送http头
    _StrPrinter printer;
    printer << "HTTP/1.1 " << pcStatus << "\r\n";
    for (auto &pr : header) {
        printer << pr.first << ": " << pr.second << "\r\n";
    }

    printer << "\r\n";
    send(printer << endl);
    _ticker.resetTime();

    if(!size){
        //没有body
        if(bClose){
            shutdown(SockException(Err_shutdown,StrPrinter << "close connection after send http header completed with status code:" << pcStatus));
        }
        return;
    }

    //发送http body
    GET_CONFIG(uint32_t,sendBufSize,Http::kSendBufSize);
    weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());

    string status_code = pcStatus;
    auto onFlush = [body,bClose,weakSelf,status_code]() {
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            //本对象已经销毁
            return false;
        }
        while(true){
            //更新超时计时器
            strongSelf->_ticker.resetTime();
            //读取文件
            auto sendBuf = body->readData(sendBufSize);
            if (!sendBuf) {
                //文件读完
                if(strongSelf->isSocketBusy() && bClose){
                    //套接字忙,我们等待触发下一次onFlush事件
                    //待所有数据flush到socket fd再移除onFlush事件监听
                    //标记文件读写完毕
                    return true;
                }
                //文件全部flush到socket fd，可以直接关闭socket了
                break;
            }

            //文件还未读完
            if(strongSelf->send(sendBuf) == -1) {
                //socket已经销毁，不再监听onFlush事件
                return false;
            }
            if(strongSelf->isSocketBusy()){
                //socket忙，那么停止继续写,等待下一次onFlush事件，然后再读文件写socket
                return true;
            }
            //socket还可写，继续写socket
        }

        if(bClose) {
            //最后一次flush事件，文件也发送完毕了，可以关闭socket了
            strongSelf->shutdown(SockException(Err_shutdown, StrPrinter << "close connection after send http body completed with status code:" << status_code));
        }
        //不再监听onFlush事件
        return false;
    };

    if(body->remainSize() > sendBufSize){
        //文件下载提升发送性能
        setSocketFlags();
    }
    onFlush();
    _sock->setOnFlush(onFlush);
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
	///////////////////是否断开本链接///////////////////////
    GET_CONFIG(uint32_t,reqCnt,Http::kMaxReqCount);

    bool bClose = (strcasecmp(_parser["Connection"].data(),"close") == 0) || ( ++_iReqCnt > reqCnt);
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
	NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastHttpRequest,_parser,invoker,consumed,*this);
	if(!consumed && doInvoke){
		//该事件无人消费，所以返回404
		invoker("404 Not Found",KeyValue(), HttpBody::Ptr());
	}
	return consumed;
}

void HttpSession::Handle_Req_POST(int64_t &content_len) {
	GET_CONFIG(uint64_t,maxReqSize,Http::kMaxReqSize);
    GET_CONFIG(int,maxReqCnt,Http::kMaxReqCount);

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
		bool bClose = (strcasecmp(_parser["Connection"].data(),"close") == 0) || ( ++_iReqCnt > maxReqCnt);

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
    GET_CONFIG(bool,ultraLowDelay,General::kUltraLowDelay);
    if(!ultraLowDelay) {
        //推流模式下，关闭TCP_NODELAY会增加推流端的延时，但是服务器性能将提高
        SockUtil::setNoDelay(_sock->rawFD(), false);
        //播放模式下，开启MSG_MORE会增加延时，但是能提高发送性能
        (*this) << SocketFlags(SOCKET_DEFAULE_FLAGS | FLAG_MORE);
    }
}

void HttpSession::onWrite(const Buffer::Ptr &buffer) {
	_ticker.resetTime();
    if(!_flv_over_websocket){
        _ui64TotalBytes += buffer->size();
        send(buffer);
        return;
    }

    WebSocketHeader header;
    header._fin = true;
    header._reserved = 0;
    header._opcode = WebSocketHeader::BINARY;
    header._mask_flag = false;
    WebSocketSplitter::encode(header,buffer);
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
