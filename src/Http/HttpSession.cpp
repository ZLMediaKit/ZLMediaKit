/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#include "Common/config.h"
#include "strCoding.h"
#include "HttpSession.h"
#include "Util/File.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "Util/onceToken.h"
#include "Util/mini.h"
#include "Util/NoticeCenter.h"
#include "Util/base64.h"
#include "Util/SHA1.h"
#include "Rtmp/utils.h"
using namespace toolkit;

namespace mediakit {

static int kSockFlags = SOCKET_DEFAULE_FLAGS | FLAG_MORE;

string dateStr() {
	char buf[64];
	time_t tt = time(NULL);
	strftime(buf, sizeof buf, "%a, %b %d %Y %H:%M:%S GMT", gmtime(&tt));
	return buf;
}
static const char*
get_mime_type(const char* name) {
	const char* dot;
	dot = strrchr(name, '.');
	static HttpSession::KeyValue mapType;
	static onceToken token([&]() {
		mapType.emplace(".html","text/html");
		mapType.emplace(".htm","text/html");
		mapType.emplace(".mp4","video/mp4");
		mapType.emplace(".m3u8","application/vnd.apple.mpegurl");
		mapType.emplace(".jpg","image/jpeg");
		mapType.emplace(".jpeg","image/jpeg");
		mapType.emplace(".gif","image/gif");
		mapType.emplace(".png","image/png");
		mapType.emplace(".ico","image/x-icon");
		mapType.emplace(".css","text/css");
		mapType.emplace(".js","application/javascript");
		mapType.emplace(".au","audio/basic");
		mapType.emplace(".wav","audio/wav");
		mapType.emplace(".avi","video/x-msvideo");
		mapType.emplace(".mov","video/quicktime");
		mapType.emplace(".qt","video/quicktime");
		mapType.emplace(".mpeg","video/mpeg");
		mapType.emplace(".mpe","video/mpeg");
		mapType.emplace(".vrml","model/vrml");
		mapType.emplace(".wrl","model/vrml");
		mapType.emplace(".midi","audio/midi");
		mapType.emplace(".mid","audio/midi");
		mapType.emplace(".mp3","audio/mpeg");
		mapType.emplace(".ogg","application/ogg");
		mapType.emplace(".pac","application/x-ns-proxy-autoconfig");
        mapType.emplace(".flv","video/x-flv");
	}, nullptr);
	if(!dot){
		return "text/plain";
	}
	auto it = mapType.find(dot);
	if (it == mapType.end()) {
		return "text/plain";
	}
	return it->second.data();
}


HttpSession::HttpSession(const Socket::Ptr &pSock) : TcpSession(pSock) {

	//设置10秒发送缓存
	pSock->setSendBufSecond(10);
	//设置15秒发送超时时间
	pSock->setSendTimeOutSecond(15);

    GET_CONFIG_AND_REGISTER(string,rootPath,Http::kRootPath);
    _strPath = rootPath;
}

HttpSession::~HttpSession() {
	//DebugL;
}

int64_t HttpSession::onRecvHeader(const char *header,uint64_t len) {
	typedef bool (HttpSession::*HttpCMDHandle)(int64_t &);
	static unordered_map<string, HttpCMDHandle> g_mapCmdIndex;
	static onceToken token([]() {
		g_mapCmdIndex.emplace("GET",&HttpSession::Handle_Req_GET);
		g_mapCmdIndex.emplace("POST",&HttpSession::Handle_Req_POST);
	}, nullptr);

	_parser.Parse(header);
	urlDecode(_parser);
	string cmd = _parser.Method();
	auto it = g_mapCmdIndex.find(cmd);
	if (it == g_mapCmdIndex.end()) {
		WarnL << cmd;
		sendResponse("403 Forbidden", makeHttpHeader(true), "");
		shutdown();
		return 0;
	}

	//默认后面数据不是content而是header
	int64_t content_len = 0;
	auto &fun = it->second;
	if(!(this->*fun)(content_len)){
		shutdown();
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
	//WarnL << err.what();
    GET_CONFIG_AND_REGISTER(uint32_t,iFlowThreshold,Broadcast::kFlowThreshold);

    if(_ui64TotalBytes > iFlowThreshold * 1024){
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport,
										   _mediaInfo,
										   _ui64TotalBytes,
										   _ticker.createdTime()/1000,
										   *this);
    }
}

void HttpSession::onManager() {
    GET_CONFIG_AND_REGISTER(uint32_t,keepAliveSec,Http::kKeepAliveSecond);

    if(_ticker.elapsedTime() > keepAliveSec * 1000){
		//1分钟超时
		WarnL<<"HttpSession timeouted!";
		shutdown();
	}
}


inline bool HttpSession::checkWebSocket(){
	auto Sec_WebSocket_Key = _parser["Sec-WebSocket-Key"];
	if(Sec_WebSocket_Key.empty()){
		return false;
	}
	auto Sec_WebSocket_Accept = encodeBase64(SHA1::encode_bin(Sec_WebSocket_Key + "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"));

	KeyValue headerOut;
	headerOut["Upgrade"] = "websocket";
	headerOut["Connection"] = "Upgrade";
	headerOut["Sec-WebSocket-Accept"] = Sec_WebSocket_Accept;
	sendResponse("101 Switching Protocols",headerOut,"");
	return true;
}
//http-flv 链接格式:http://vhost-url:port/app/streamid.flv?key1=value1&key2=value2
//如果url(除去?以及后面的参数)后缀是.flv,那么表明该url是一个http-flv直播。
inline bool HttpSession::checkLiveFlvStream(){
	auto pos = strrchr(_parser.Url().data(),'.');
	if(!pos){
		//未找到".flv"后缀
		return false;
	}
	if(strcasecmp(pos,".flv") != 0){
		//未找到".flv"后缀
		return false;
	}
    //拼接成完整url
    auto fullUrl = string(HTTP_SCHEMA) + "://" + _parser["Host"] + _parser.FullUrl();
    _mediaInfo.parse(fullUrl);
    _mediaInfo._streamid.erase(_mediaInfo._streamid.size() - 4);//去除.flv后缀

	auto mediaSrc = dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA,_mediaInfo._vhost,_mediaInfo._app,_mediaInfo._streamid));
	if(!mediaSrc){
		//该rtmp源不存在
		return false;
	}

    auto onRes = [this,mediaSrc](const string &err){
        bool authSuccess = err.empty();
        if(!authSuccess){
            sendResponse("401 Unauthorized", makeHttpHeader(true,err.size()),err);
            shutdown();
            return ;
        }
        //找到rtmp源，发送http头，负载后续发送
        sendResponse("200 OK", makeHttpHeader(false,0,get_mime_type(".flv")), "");

        //开始发送rtmp负载
        //关闭tcp_nodelay ,优化性能
        SockUtil::setNoDelay(_sock->rawFD(),false);
        (*this) << SocketFlags(kSockFlags);

		try{
			start(mediaSrc);
		}catch (std::exception &ex){
			//该rtmp源不存在
			shutdown();
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
    return true;
}
inline bool HttpSession::Handle_Req_GET(int64_t &content_len) {
	//先看看是否为WebSocket请求
	if(checkWebSocket()){
		content_len = -1;
		auto parserCopy = _parser;
		_contentCallBack = [this,parserCopy](const char *data,uint64_t len){
			onRecvWebSocketData(parserCopy,data,len);
			//_contentCallBack是可持续的，后面还要处理后续数据
			return true;
		};
		return true;
	}

	//先看看该http事件是否被拦截
	if(emitHttpEvent(false)){
		return true;
	}

    //再看看是否为http-flv直播请求
	if(checkLiveFlvStream()){
		return true;
	}

	//事件未被拦截，则认为是http下载请求
	auto fullUrl = string(HTTP_SCHEMA) + "://" + _parser["Host"] + _parser.FullUrl();
    _mediaInfo.parse(fullUrl);

	string strFile = _strPath + "/" + _mediaInfo._vhost + _parser.Url();
	/////////////HTTP连接是否需要被关闭////////////////
    GET_CONFIG_AND_REGISTER(uint32_t,reqCnt,Http::kMaxReqCount);

    bool bClose = (strcasecmp(_parser["Connection"].data(),"close") == 0) || ( ++_iReqCnt > reqCnt);
	//访问的是文件夹
	if (strFile.back() == '/') {
		//生成文件夹菜单索引
		string strMeun;
		if (!makeMeun(strFile,_mediaInfo._vhost, strMeun)) {
			//文件夹不存在
			sendNotFound(bClose);
			return !bClose;
		}
		sendResponse("200 OK", makeHttpHeader(bClose,strMeun.size() ), strMeun);
		return !bClose;
	}
	//访问的是文件
	struct stat tFileStat;
	if (0 != stat(strFile.data(), &tFileStat)) {
		//文件不存在
		sendNotFound(bClose);
		return !bClose;
	}
    //文件智能指针，防止退出时未关闭
    std::shared_ptr<FILE> pFilePtr(fopen(strFile.data(), "rb"), [](FILE *pFile) {
        if(pFile){
            fclose(pFile);
        }
    });

	if (!pFilePtr) {
		//打开文件失败
		sendNotFound(bClose);
		return !bClose;
	}

	//判断是不是分节下载
	auto &strRange = _parser["Range"];
	int64_t iRangeStart = 0, iRangeEnd = 0;
	iRangeStart = atoll(FindField(strRange.data(), "bytes=", "-").data());
	iRangeEnd = atoll(FindField(strRange.data(), "-", "\r\n").data());
	if (iRangeEnd == 0) {
		iRangeEnd = tFileStat.st_size - 1;
	}
	const char *pcHttpResult = NULL;
	if (strRange.size() == 0) {
		//全部下载
		pcHttpResult = "200 OK";
	} else {
		//分节下载
		pcHttpResult = "206 Partial Content";
		fseek(pFilePtr.get(), iRangeStart, SEEK_SET);
	}
	auto httpHeader=makeHttpHeader(bClose, iRangeEnd - iRangeStart + 1, get_mime_type(strFile.data()));
	if (strRange.size() != 0) {
		//分节下载返回Content-Range头
		httpHeader.emplace("Content-Range",StrPrinter<<"bytes " << iRangeStart << "-" << iRangeEnd << "/" << tFileStat.st_size<< endl);
	}
	auto Origin = _parser["Origin"];
	if(!Origin.empty()){
		httpHeader["Access-Control-Allow-Origin"] = Origin;
		httpHeader["Access-Control-Allow-Credentials"] = "true";
	}
	//先回复HTTP头部分
	sendResponse(pcHttpResult, httpHeader, "");
	if (iRangeEnd - iRangeStart < 0) {
		//文件是空的!
		return !bClose;
	}
	//回复Content部分
	std::shared_ptr<int64_t> piLeft(new int64_t(iRangeEnd - iRangeStart + 1));

    GET_CONFIG_AND_REGISTER(uint32_t,sendBufSize,Http::kSendBufSize);

	weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
	auto onFlush = [pFilePtr,bClose,weakSelf,piLeft]() {
		TimeTicker();
		auto strongSelf = weakSelf.lock();
		while(*piLeft && strongSelf){
            //更新超时定时器
            strongSelf->_ticker.resetTime();
            //从循环池获取一个内存片
            auto sendBuf = strongSelf->obtainBuffer();
            sendBuf->setCapacity(sendBufSize);
            //本次需要读取文件字节数
			int64_t iReq = MIN(sendBufSize,*piLeft);
            //读文件	
			int iRead;
			do{
				 iRead = fread(sendBuf->data(), 1, iReq, pFilePtr.get());
			}while(-1 == iRead && UV_EINTR == get_uv_error(false));
            //文件剩余字节数
			
			*piLeft -= iRead;

			if (iRead < iReq || !*piLeft) {
                //文件读完
				//InfoL << "send complete!" << iRead << " " << iReq << " " << *piLeft;
				if(iRead>0) {
					sendBuf->setSize(iRead);
					strongSelf->send(sendBuf);
				}
				if(bClose) {
					strongSelf->shutdown();
				}
				return false;
			}
            //文件还未读完
            sendBuf->setSize(iRead);
            int iSent = strongSelf->send(sendBuf);
			if(iSent == -1) {
				//InfoL << "send error";
				return false;
			}
			if(iSent < iRead) {
				//数据回滚
				fseek(pFilePtr.get(), -iRead, SEEK_CUR);
				*piLeft += iRead;
				return true;
			}
            if(strongSelf->isSocketBusy()){
                //套接字忙，那么停止继续写
                return true;
            }
			//send success
		}
		return false;
	};
	//关闭tcp_nodelay ,优化性能
	SockUtil::setNoDelay(_sock->rawFD(),false);
    //设置MSG_MORE，优化性能
    (*this) << SocketFlags(kSockFlags);

    onFlush();
	_sock->setOnFlush(onFlush);
	return true;
}

inline bool HttpSession::makeMeun(const string &strFullPath,const string &vhost, string &strRet) {
	string strPathPrefix(strFullPath);
	strPathPrefix = strPathPrefix.substr(0, strPathPrefix.length() - 1);
	if (!File::is_dir(strPathPrefix.data())) {
		return false;
	}
	strRet = "<html>\r\n"
			"<head>\r\n"
			"<title>文件索引</title>\r\n"
			"</head>\r\n"
			"<body>\r\n"
			"<h1>文件索引:";

	string strPath = strFullPath;
	strPath = strPath.substr(_strPath.length() + vhost.length() + 1);
	strRet += strPath;
	strRet += "</h1>\r\n";
	if (strPath != "/") {
		strRet += "<li><a href=\"";
		strRet += "/";
		strRet += "\">";
		strRet += "根目录";
		strRet += "</a></li>\r\n";

		strRet += "<li><a href=\"";
		strRet += "../";
		strRet += "\">";
		strRet += "上级目录";
		strRet += "</a></li>\r\n";
	}

	DIR *pDir;
	dirent *pDirent;
	if ((pDir = opendir(strPathPrefix.data())) == NULL) {
		return false;
	}
	set<string> setFile;
	while ((pDirent = readdir(pDir)) != NULL) {
		if (File::is_special_dir(pDirent->d_name)) {
			continue;
		}
		if(pDirent->d_name[0] == '.'){
			continue;
		}
		setFile.emplace(pDirent->d_name);
	}
	for(auto &strFile :setFile ){
		string strAbsolutePath = strPathPrefix + "/" + strFile;
		if (File::is_dir(strAbsolutePath.data())) {
			strRet += "<li><a href=\"";
			strRet += strFile;
			strRet += "/\">";
			strRet += strFile;
			strRet += "/</a></li>\r\n";
		} else { //是文件
			strRet += "<li><a href=\"";
			strRet += strFile;
			strRet += "\">";
			strRet += strFile;
			struct stat fileData;
			if (0 == stat(strAbsolutePath.data(), &fileData)) {
				auto &fileSize = fileData.st_size;
				if (fileSize < 1024) {
					strRet += StrPrinter << " (" << fileData.st_size << "B)" << endl;
				} else if (fileSize < 1024 * 1024) {
					strRet += StrPrinter << " (" << fileData.st_size / 1024 << "KB)" << endl;
				} else if (fileSize < 1024 * 1024 * 1024) {
					strRet += StrPrinter << " (" << fileData.st_size / 1024 / 1024 << "MB)" << endl;
				} else {
					strRet += StrPrinter << " (" << fileData.st_size / 1024 / 1024 / 1024 << "GB)" << endl;
				}
			}
			strRet += "</a></li>\r\n";
		}
	}
	closedir(pDir);
	strRet += "<ul>\r\n";
	strRet += "</ul>\r\n</body></html>";
	return true;
}
inline void HttpSession::sendResponse(const char* pcStatus, const KeyValue& header, const string& strContent) {
	_StrPrinter printer;
	printer << "HTTP/1.1 " << pcStatus << "\r\n";
	for (auto &pr : header) {
		printer << pr.first << ": " << pr.second << "\r\n";
	}
	printer << "\r\n" << strContent;
	auto strSend = printer << endl;
	//DebugL << strSend;
	send(strSend);
	_ticker.resetTime();
}
inline HttpSession::KeyValue HttpSession::makeHttpHeader(bool bClose, int64_t iContentSize,const char* pcContentType) {
	KeyValue headerOut;
    GET_CONFIG_AND_REGISTER(string,charSet,Http::kCharSet);
    GET_CONFIG_AND_REGISTER(uint32_t,keepAliveSec,Http::kKeepAliveSecond);
    GET_CONFIG_AND_REGISTER(uint32_t,reqCnt,Http::kMaxReqCount);

	headerOut.emplace("Date", dateStr());
	headerOut.emplace("Server", SERVER_NAME);
	headerOut.emplace("Connection", bClose ? "close" : "keep-alive");
	if(!bClose){
		headerOut.emplace("Keep-Alive",StrPrinter << "timeout=" << keepAliveSec << ", max=" << reqCnt << endl);
	}
	if(pcContentType){
		auto strContentType = StrPrinter << pcContentType << "; charset=" << charSet << endl;
		headerOut.emplace("Content-Type",strContentType);
	}
	if(iContentSize > 0){
		headerOut.emplace("Content-Length", StrPrinter<<iContentSize<<endl);
	}
	return headerOut;
}

string HttpSession::urlDecode(const string &str){
	auto ret = strCoding::UrlUTF8Decode(str);
#ifdef _WIN32
    GET_CONFIG_AND_REGISTER(string,charSet,Http::kCharSet);
	bool isGb2312 = !strcasecmp(charSet.data(), "gb2312");
	if (isGb2312) {
		ret = strCoding::UTF8ToGB2312(ret);
	}
#endif // _WIN32
    return ret;
}

inline void HttpSession::urlDecode(Parser &parser){
	parser.setUrl(urlDecode(parser.Url()));
	for(auto &pr : _parser.getUrlArgs()){
		const_cast<string &>(pr.second) = urlDecode(pr.second);
	}
}

inline bool HttpSession::emitHttpEvent(bool doInvoke){
	///////////////////是否断开本链接///////////////////////
    GET_CONFIG_AND_REGISTER(uint32_t,reqCnt,Http::kMaxReqCount);

    bool bClose = (strcasecmp(_parser["Connection"].data(),"close") == 0) || ( ++_iReqCnt > reqCnt);
	auto Origin = _parser["Origin"];
	/////////////////////异步回复Invoker///////////////////////////////
	weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
	HttpResponseInvoker invoker = [weakSelf,bClose,Origin](const string &codeOut, const KeyValue &headerOut, const string &contentOut){
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		strongSelf->async([weakSelf,bClose,codeOut,headerOut,contentOut,Origin]() {
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->responseDelay(Origin,bClose,codeOut,headerOut,contentOut);
			if(bClose){
				strongSelf->shutdown();
			}
		});
	};
	///////////////////广播HTTP事件///////////////////////////
	bool consumed = false;//该事件是否被消费
	NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastHttpRequest,_parser,invoker,consumed,*this);
	if(!consumed && doInvoke){
		//该事件无人消费，所以返回404
		invoker("404 Not Found",KeyValue(),"");
		if(bClose){
			//close类型，回复完毕，关闭连接
			shutdown();
		}
	}
	return consumed;
}
inline bool HttpSession::Handle_Req_POST(int64_t &content_len) {
	GET_CONFIG_AND_REGISTER(uint64_t,maxReqSize,Http::kMaxReqSize);
    GET_CONFIG_AND_REGISTER(int,maxReqCnt,Http::kMaxReqCount);

    int64_t totalContentLen = _parser["Content-Length"].empty() ? -1 : atoll(_parser["Content-Length"].data());

	if(totalContentLen == 0){
		//content为空
		//emitHttpEvent内部会选择是否关闭连接
		emitHttpEvent(true);
		return true;
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
            shutdown();
            //content已经接收完毕
            return false ;
		};
	}
	//有后续content数据要处理,暂时不关闭连接
	return true;
}
void HttpSession::responseDelay(const string &Origin,bool bClose,
								const string &codeOut,const KeyValue &headerOut,
								const string &contentOut){
	if(codeOut.empty()){
		sendNotFound(bClose);
		return;
	}
	auto headerOther=makeHttpHeader(bClose,contentOut.size(),"text/plain");
	if(!Origin.empty()){
		headerOther["Access-Control-Allow-Origin"] = Origin;
		headerOther["Access-Control-Allow-Credentials"] = "true";
	}
	const_cast<KeyValue &>(headerOut).insert(headerOther.begin(), headerOther.end());
	sendResponse(codeOut.data(), headerOut, contentOut);
}
inline void HttpSession::sendNotFound(bool bClose) {
    GET_CONFIG_AND_REGISTER(string,notFound,Http::kNotFound);
    sendResponse("404 Not Found", makeHttpHeader(bClose, notFound.size()), notFound);
}


void HttpSession::onWrite(const Buffer::Ptr &buffer) {
	weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
	async([weakSelf,buffer](){
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		strongSelf->_ticker.resetTime();
		strongSelf->_ui64TotalBytes += buffer->size();
		strongSelf->send(buffer);
	});
}

void HttpSession::onWrite(const char *data, int len) {
	BufferRaw::Ptr buffer(new BufferRaw);
	buffer->assign(data,len);

	weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
	async([weakSelf,buffer](){
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		strongSelf->_ticker.resetTime();
		strongSelf->_ui64TotalBytes += buffer->size();
		strongSelf->send(buffer);
	});
}

void HttpSession::onDetach() {
	safeShutdown();
}

std::shared_ptr<FlvMuxer> HttpSession::getSharedPtr(){
	return dynamic_pointer_cast<FlvMuxer>(shared_from_this());
}

} /* namespace mediakit */
