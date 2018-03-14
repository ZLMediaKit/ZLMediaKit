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
#include "Rtmp/utils.h"

using namespace ZL::Util;

namespace ZL {
namespace Http {

static int sock_flags = SOCKET_DEFAULE_FLAGS | FLAG_MORE;

unordered_map<string, HttpSession::HttpCMDHandle> HttpSession::g_mapCmdIndex;
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


HttpSession::HttpSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock) :
		TcpSession(pTh, pSock) {
    GET_CONFIG_AND_REGISTER(string,rootPath,Config::Http::kRootPath);

    m_strPath = rootPath;
	static onceToken token([]() {
		g_mapCmdIndex.emplace("GET",&HttpSession::Handle_Req_GET);
		g_mapCmdIndex.emplace("POST",&HttpSession::Handle_Req_POST);
	}, nullptr);
}

HttpSession::~HttpSession() {
	//DebugL;
}

void HttpSession::onRecv(const Buffer::Ptr &pBuf) {
	onRecv(pBuf->data(),pBuf->size());
}
void HttpSession::onRecv(const char *data,int size){
    GET_CONFIG_AND_REGISTER(uint32_t,reqSize,Config::Http::kMaxReqSize);

    m_ticker.resetTime();
	if (m_strRcvBuf.size() + size >= reqSize) {
		WarnL << "接收缓冲区溢出:" << m_strRcvBuf.size() + size << "," << reqSize;
		shutdown();
		return;
	}
	m_strRcvBuf.append(data, size);
	size_t index;
	string onePkt;
	while ((index = m_strRcvBuf.find("\r\n\r\n")) != std::string::npos) {
		onePkt = m_strRcvBuf.substr(0, index + 4);
		m_strRcvBuf.erase(0, index + 4);
		switch (parserHttpReq(onePkt)) {
		case Http_failed:
			//失败
			shutdown();
			return;
		case Http_success:
			//成功
			break;
		case Http_moreData:
			//需要更多数据,恢复数据并退出
			m_strRcvBuf = onePkt + m_strRcvBuf;
			m_parser.Clear();
			return;
		}
	}
	m_parser.Clear();
}
inline HttpSession::HttpCode HttpSession::parserHttpReq(const string &str) {
	m_parser.Parse(str.data());
	urlDecode(m_parser);
	string cmd = m_parser.Method();
	auto it = g_mapCmdIndex.find(cmd);
	if (it == g_mapCmdIndex.end()) {
		WarnL << cmd;
		sendResponse("403 Forbidden", makeHttpHeader(true), "");
		return Http_failed;
	}
	auto fun = it->second;
	return (this->*fun)();
}
void HttpSession::onError(const SockException& err) {
	//WarnL << err.what();
    GET_CONFIG_AND_REGISTER(uint32_t,iFlowThreshold,Broadcast::kFlowThreshold);

    if(m_previousTagSize > iFlowThreshold * 1024){
        uint64_t totalBytes = m_previousTagSize;
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport,m_mediaInfo,totalBytes,*this);
    }
}

void HttpSession::onManager() {
    GET_CONFIG_AND_REGISTER(uint32_t,keepAliveSec,Config::Http::kKeepAliveSecond);

    if(m_ticker.elapsedTime() > keepAliveSec * 1000){
		//1分钟超时
		WarnL<<"HttpSession timeouted!";
		shutdown();
	}
}
//http-flv 链接格式:http://vhost-url:port/app/streamid.flv?key1=value1&key2=value2
//如果url(除去?以及后面的参数)后缀是.flv,那么表明该url是一个http-flv直播。
inline bool HttpSession::checkLiveFlvStream(){
	auto pos = strrchr(m_parser.Url().data(),'.');
	if(!pos){
		//未找到".flv"后缀
		return false;
	}
	if(strcasecmp(pos,".flv") != 0){
		//未找到".flv"后缀
		return false;
	}
    //拼接成完整url
    auto fullUrl = string(HTTP_SCHEMA) + "://" + m_parser["Host"] + m_parser.FullUrl();
    m_mediaInfo.parse(fullUrl);
    m_mediaInfo.m_streamid.erase(m_mediaInfo.m_streamid.size() - 4);//去除.flv后缀

	auto mediaSrc = dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA,m_mediaInfo.m_vhost,m_mediaInfo.m_app,m_mediaInfo.m_streamid));
	if(!mediaSrc){
		//该rtmp源不存在
        sendNotFound(true);
        shutdown();
		return true;
	}
	if(!mediaSrc->ready()){
		//未准备好
        sendNotFound(true);
        shutdown();
		return true;
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
        //发送flv文件头
        char flv_file_header[] = "FLV\x1\x5\x0\x0\x0\x9"; // have audio and have video
        bool is_have_audio = false,is_have_video = false;

        mediaSrc->getConfigFrame([&](const RtmpPacket::Ptr &pkt){
            if(pkt->typeId == MSG_VIDEO){
                is_have_video = true;
            }
            if(pkt->typeId == MSG_AUDIO){
                is_have_audio = true;
            }
        });

        if (is_have_audio && is_have_video) {
            flv_file_header[4] = 0x05;
        } else if (is_have_audio && !is_have_video) {
            flv_file_header[4] = 0x04;
        } else if (!is_have_audio && is_have_video) {
            flv_file_header[4] = 0x01;
        } else {
            flv_file_header[4] = 0x00;
        }
        //send flv header
        send(flv_file_header, sizeof(flv_file_header) - 1);
        //send metadata
        AMFEncoder invoke;
        invoke << "onMetaData" << mediaSrc->getMetaData();
        sendRtmp(MSG_DATA, invoke.data(), 0);
        //send config frame
        mediaSrc->getConfigFrame([&](const RtmpPacket::Ptr &pkt){
            onSendMedia(pkt);
        });

        //开始发送rtmp负载

        //关闭tcp_nodelay ,优化性能
        SockUtil::setNoDelay(_sock->rawFD(),false);
        (*this) << SocketFlags(sock_flags);

        m_pRingReader = mediaSrc->getRing()->attach();
        weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
        m_pRingReader->setReadCB([weakSelf](const RtmpPacket::Ptr &pkt){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf) {
                return;
            }
            strongSelf->async([pkt,weakSelf](){
                auto strongSelf = weakSelf.lock();
                if(!strongSelf) {
                    return;
                }
                strongSelf->onSendMedia(pkt);
            });
        });
        m_pRingReader->setDetachCB([weakSelf](){
            auto strongSelf = weakSelf.lock();
            if(!strongSelf) {
                return;
            }
            strongSelf->async_first([weakSelf](){
                auto strongSelf = weakSelf.lock();
                if(!strongSelf) {
                    return;
                }
                strongSelf->shutdown();
            });
        });
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
    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPlayed,m_mediaInfo,invoker,*this);
    if(!flag){
        //该事件无人监听,默认不鉴权
        onRes("");
    }
    return true;
}
inline HttpSession::HttpCode HttpSession::Handle_Req_GET() {
	//先看看该http事件是否被拦截
	if(emitHttpEvent(false)){
		return Http_success;
	}
    //再看看是否为http-flv直播请求
	if(checkLiveFlvStream()){
		return Http_success;
	}
	//事件未被拦截，则认为是http下载请求

	auto fullUrl = string(HTTP_SCHEMA) + "://" + m_parser["Host"] + m_parser.FullUrl();
    m_mediaInfo.parse(fullUrl);

	string strFile = m_strPath + "/" + m_mediaInfo.m_vhost + m_parser.Url();
	/////////////HTTP连接是否需要被关闭////////////////
    GET_CONFIG_AND_REGISTER(uint32_t,reqCnt,Config::Http::kMaxReqCount);

    bool bClose = (strcasecmp(m_parser["Connection"].data(),"close") == 0) || ( ++m_iReqCnt > reqCnt);
	HttpCode eHttpCode = bClose ? Http_failed : Http_success;
	//访问的是文件夹
	if (strFile.back() == '/') {
		//生成文件夹菜单索引
		string strMeun;
		if (!makeMeun(strFile,m_mediaInfo.m_vhost, strMeun)) {
			//文件夹不存在
			sendNotFound(bClose);
			return eHttpCode;
		}
		sendResponse("200 OK", makeHttpHeader(bClose,strMeun.size() ), strMeun);
		return eHttpCode;
	}
	//访问的是文件
	struct stat tFileStat;
	if (0 != stat(strFile.data(), &tFileStat)) {
		//文件不存在
		sendNotFound(bClose);
		return eHttpCode;
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
		return eHttpCode;
	}

	//判断是不是分节下载
	auto &strRange = m_parser["Range"];
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
	auto Origin = m_parser["Origin"];
	if(!Origin.empty()){
		httpHeader["Access-Control-Allow-Origin"] = Origin;
		httpHeader["Access-Control-Allow-Credentials"] = "true";
	}
	//先回复HTTP头部分
	sendResponse(pcHttpResult, httpHeader, "");
	if (iRangeEnd - iRangeStart < 0) {
		//文件是空的!
		return eHttpCode;
	}
	//回复Content部分
	std::shared_ptr<int64_t> piLeft(new int64_t(iRangeEnd - iRangeStart + 1));

    GET_CONFIG_AND_REGISTER(uint32_t,sendBufSize,Config::Http::kSendBufSize);

	weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
	auto onFlush = [pFilePtr,bClose,weakSelf,piLeft]() {
		TimeTicker();
		auto strongSelf = weakSelf.lock();
		while(*piLeft && strongSelf){
            //更新超时定时器
            strongSelf->m_ticker.resetTime();
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
    (*this) << SocketFlags(sock_flags);

    //后台线程执行onFlush
    auto onFlushWrapper = [onFlush,weakSelf](){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return false;
        }
        strongSelf->async([onFlush,weakSelf](){
            //在后台线程完成文件读取，释放主线程性能
            if(!onFlush()){
                //如果onFlush返回false，则说明不再监听flush事件
                auto strongSelf = weakSelf.lock();
                if(strongSelf){
                    strongSelf->_sock->setOnFlush(nullptr);
                }
            }
        });
        return true;
    };

    onFlush();
	_sock->setOnFlush(onFlushWrapper);
	return Http_success;
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
	strPath = strPath.substr(m_strPath.length() + vhost.length() + 1);
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
	printer << "HTTP/1.1 " << pcStatus << " \r\n";
	for (auto &pr : header) {
		printer << pr.first << ": " << pr.second << "\r\n";
	}
	printer << "\r\n" << strContent;
	auto strSend = printer << endl;
	//DebugL << strSend;
	send(strSend);
	m_ticker.resetTime();
}
inline HttpSession::KeyValue HttpSession::makeHttpHeader(bool bClose, int64_t iContentSize,const char* pcContentType) {
	KeyValue headerOut;
    GET_CONFIG_AND_REGISTER(string,charSet,Config::Http::kCharSet);
    GET_CONFIG_AND_REGISTER(uint32_t,keepAliveSec,Config::Http::kKeepAliveSecond);
    GET_CONFIG_AND_REGISTER(uint32_t,reqCnt,Config::Http::kMaxReqCount);

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
    GET_CONFIG_AND_REGISTER(string,charSet,Config::Http::kCharSet);
	bool isGb2312 = !strcasecmp(charSet.data(), "gb2312");
	if (isGb2312) {
		ret = strCoding::UTF8ToGB2312(ret);
	}
#endif // _WIN32
    return ret;
}

inline void HttpSession::urlDecode(Parser &parser){
	parser.setUrl(urlDecode(parser.Url()));
	for(auto &pr : m_parser.getUrlArgs()){
		const_cast<string &>(pr.second) = urlDecode(pr.second);
	}
}

inline bool HttpSession::emitHttpEvent(bool doInvoke){
	///////////////////是否断开本链接///////////////////////
    GET_CONFIG_AND_REGISTER(uint32_t,reqCnt,Config::Http::kMaxReqCount);

    bool bClose = (strcasecmp(m_parser["Connection"].data(),"close") == 0) || ( ++m_iReqCnt > reqCnt);
	auto Origin = m_parser["Origin"];
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
	NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastHttpRequest,m_parser,invoker,consumed,*this);
	if(!consumed && doInvoke){
		//该事件无人消费，所以返回404
		invoker("404 Not Found",KeyValue(),"");
	}
	return consumed;
}
inline HttpSession::HttpCode HttpSession::Handle_Req_POST() {
	//////////////获取HTTP POST Content/////////////
	int iContentLen = atoi(m_parser["Content-Length"].data());
	if ((int) m_strRcvBuf.size() < iContentLen) {
		return Http_moreData; //需要更多数据
	}
	m_parser.setContent(m_strRcvBuf.substr(0, iContentLen));
	m_strRcvBuf.erase(0, iContentLen);
	//广播事件
	emitHttpEvent(true);
	return Http_success;
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
    GET_CONFIG_AND_REGISTER(string,notFound,Config::Http::kNotFound);

    sendResponse("404 Not Found", makeHttpHeader(bClose, notFound.size()), notFound);
}

void HttpSession::onSendMedia(const RtmpPacket::Ptr &pkt) {
	auto modifiedStamp = pkt->timeStamp;
	auto &firstStamp = m_aui32FirstStamp[pkt->typeId % 2];
	if(!firstStamp){
		firstStamp = modifiedStamp;
	}
	if(modifiedStamp >= firstStamp){
		//计算时间戳增量
		modifiedStamp -= firstStamp;
	}else{
		//发生回环，重新计算时间戳增量
		CLEAR_ARR(m_aui32FirstStamp);
		modifiedStamp = 0;
	}
	sendRtmp(pkt, modifiedStamp);
}

#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

class RtmpTagHeader {
public:
	uint8_t type = 0;
	uint8_t data_size[3] = {0};
	uint8_t timestamp[3] = {0};
	uint8_t timestamp_ex = 0;
	uint8_t streamid[3] = {0}; /* Always 0. */
}PACKED;

#if defined(_WIN32)
#pragma pack(pop)
#endif // defined(_WIN32)

class BufferRtmp : public Buffer{
public:
    typedef std::shared_ptr<BufferRtmp> Ptr;
    BufferRtmp(const RtmpPacket::Ptr & pkt):_rtmp(pkt){}
    virtual ~BufferRtmp(){}

    char *data() override {
        return (char *)_rtmp->strBuf.data();
    }
    uint32_t size() const override {
        return _rtmp->strBuf.size();
    }
private:
    RtmpPacket::Ptr _rtmp;
};

void HttpSession::sendRtmp(const RtmpPacket::Ptr &pkt, uint32_t ui32TimeStamp) {
	auto size = htonl(m_previousTagSize);
    send((char *)&size,4);//send PreviousTagSize
	RtmpTagHeader header;
	header.type = pkt->typeId;
	set_be24(header.data_size, pkt->strBuf.size());
	header.timestamp_ex = (uint8_t) ((ui32TimeStamp >> 24) & 0xff);
	set_be24(header.timestamp,ui32TimeStamp & 0xFFFFFF);
    send((char *)&header, sizeof(header));//send tag header
    send(std::make_shared<BufferRtmp>(pkt));//send tag data
	m_previousTagSize += (pkt->strBuf.size() + sizeof(header) + 4);
	m_ticker.resetTime();
}

void HttpSession::sendRtmp(uint8_t ui8Type, const std::string& strBuf, uint32_t ui32TimeStamp) {
    auto size = htonl(m_previousTagSize);
    send((char *)&size,4);//send PreviousTagSize
    RtmpTagHeader header;
    header.type = ui8Type;
    set_be24(header.data_size, strBuf.size());
    header.timestamp_ex = (uint8_t) ((ui32TimeStamp >> 24) & 0xff);
    set_be24(header.timestamp,ui32TimeStamp & 0xFFFFFF);
    send((char *)&header, sizeof(header));//send tag header
    send(strBuf);//send tag data
    m_previousTagSize += (strBuf.size() + sizeof(header) + 4);
    m_ticker.resetTime();
}

} /* namespace Http */
} /* namespace ZL */
