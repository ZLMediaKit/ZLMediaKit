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


using namespace ZL::Util;

namespace ZL {
namespace Http {

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
	static unordered_map<string, string> mapType;
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
	}, nullptr);
	if(!dot){
		return "text/plain";
	}
	string strDot(dot);
	transform(strDot.begin(), strDot.end(), strDot.begin(), (int (*)(int))tolower);
	auto it = mapType.find(strDot);
	if (it == mapType.end()) {
		return "text/plain";
	}
	return it->second.data();
}


HttpSession::HttpSession(const std::shared_ptr<ThreadPool> &pTh, const Socket::Ptr &pSock) :
		TcpLimitedSession(pTh, pSock) {
	static string rootPath =  mINI::Instance()[Config::Http::kRootPath];
	m_strPath = rootPath;
	static onceToken token([]() {
		g_mapCmdIndex.emplace("GET",&HttpSession::Handle_Req_GET);
		g_mapCmdIndex.emplace("POST",&HttpSession::Handle_Req_POST);
	}, nullptr);
}

HttpSession::~HttpSession() {
	//DebugL;
}

void HttpSession::onRecv(const Socket::Buffer::Ptr &pBuf) {
	onRecv(pBuf->data(),pBuf->size());
}
void HttpSession::onRecv(const char *data,int size){
	static uint32_t reqSize =  mINI::Instance()[Config::Http::kMaxReqSize].as<uint32_t>();
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
}

void HttpSession::onManager() {
	static uint32_t keepAliveSec =  mINI::Instance()[Config::Http::kKeepAliveSecond].as<uint32_t>();
	if(m_ticker.elapsedTime() > keepAliveSec * 1000){
		//1分钟超时
		WarnL<<"HttpSession timeouted!";
		shutdown();
	}
}

inline HttpSession::HttpCode HttpSession::Handle_Req_GET() {
	//先看看该http事件是否被拦截
	if(emitHttpEvent(false)){
		return Http_success;
	}
	//事件未被拦截，则认为是http下载请求

	string strFile = m_strPath + m_parser.Url();
	/////////////HTTP连接是否需要被关闭////////////////
	static uint32_t reqCnt =  mINI::Instance()[Config::Http::kMaxReqCount].as<uint32_t>();
	bool bClose = (strcasecmp(m_parser["Connection"].data(),"close") == 0) && ( ++m_iReqCnt < reqCnt);
	HttpCode eHttpCode = bClose ? Http_failed : Http_success;
	//访问的是文件夹
	if (strFile.back() == '/') {
		//生成文件夹菜单索引
		string strMeun;
		if (!makeMeun(strFile, strMeun)) {
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
	FILE *pFile = fopen(strFile.data(), "rb");
	if (pFile == NULL) {
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
		fseek(pFile, iRangeStart, SEEK_SET);
	}
	auto httpHeader=makeHttpHeader(bClose, iRangeEnd - iRangeStart + 1, get_mime_type(strFile.data()));
	if (strRange.size() != 0) {
		//分节下载返回Content-Range头
		httpHeader.emplace("Content-Range",StrPrinter<<"bytes " << iRangeStart << "-" << iRangeEnd << "/" << tFileStat.st_size<< endl);
	}
	//先回复HTTP头部分
	sendResponse(pcHttpResult, httpHeader, "");
	if (iRangeEnd - iRangeStart < 0) {
		//文件时空的!
		return eHttpCode;
	}
	//回复Content部分
	std::shared_ptr<int64_t> piLeft(new int64_t(iRangeEnd - iRangeStart + 1));
	std::shared_ptr<FILE> pFilePtr(pFile, [](FILE *pFp) {
		fclose(pFp);
	});
	static uint32_t sendBufSize =  mINI::Instance()[Config::Http::kSendBufSize].as<uint32_t>();
	std::shared_ptr<char> pacSendBuf(new char[sendBufSize],[](char *ptr){
		delete [] ptr;
	});
	weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
	auto onFlush = [pFilePtr,bClose,weakSelf,piLeft,pacSendBuf]() {
		TimeTicker();
		auto strongSelf = weakSelf.lock();
		while(*piLeft && strongSelf){
			strongSelf->m_ticker.resetTime();
			int64_t iReq = MIN(sendBufSize,*piLeft);
			int64_t iRead = fread(pacSendBuf.get(), 1, iReq, pFilePtr.get());
			*piLeft -= iRead;
			//InfoL << "Send file :" << iReq << " " << *piLeft;
			if (iRead < iReq || !*piLeft) {
				//send completed!
				//FatalL << "send completed!";
				if(iRead>0) {
					strongSelf->send(pacSendBuf.get(), iRead);
				}
				if(bClose) {
					strongSelf->shutdown();
				}
				return false;
			}

			int iSent=strongSelf->send(pacSendBuf.get(), iRead);
			if(iSent == -1) {
				//send error
				//FatalL << "send error";
				return false;
			}
			if(iSent < iRead) {
				//send wait
				//FatalL << "send wait";
				return true;
			}
			//send success
		}
		return false;
	};
	onFlush();
	sock->setOnFlush(onFlush);
	return Http_success;
}

inline bool HttpSession::makeMeun(const string &strFullPath, string &strRet) {
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
	strPath = strPath.substr(m_strPath.length(), strFullPath.length() - m_strPath.length());
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
	static string serverName =  mINI::Instance()[Config::Http::kServerName];
	static string charSet =  mINI::Instance()[Config::Http::kCharSet];
	static uint32_t keepAliveSec =  mINI::Instance()[Config::Http::kKeepAliveSecond].as<uint32_t>();
	static uint32_t reqCnt =  mINI::Instance()[Config::Http::kMaxReqCount].as<uint32_t>();

	headerOut.emplace("Server", serverName);
	headerOut.emplace("Connection", bClose ? "close" : "keep-alive");
    if(!bClose){
        headerOut.emplace("Keep-Alive",StrPrinter << "timeout=" << keepAliveSec << ", max=" << reqCnt << endl);
    }
	headerOut.emplace("Date", dateStr());
	if(iContentSize >=0 && pcContentType !=nullptr){
		auto strContentType = StrPrinter << pcContentType << "; charset=" << charSet << endl;
		headerOut.emplace("Content-Type",strContentType.data());
		headerOut.emplace("Content-Length", StrPrinter<<iContentSize<<endl);
	}
	return headerOut;
}

string HttpSession::urlDecode(const string &str){
	auto ret = strCoding::UrlUTF8Decode(str);
#ifdef _WIN32
	static bool isGb2312 = !strcasecmp(mINI::Instance()[Config::Http::kCharSet].data(), "gb2312");
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
	static uint32_t reqCnt = mINI::Instance()[Config::Http::kMaxReqCount].as<uint32_t>();
	bool bClose = (strcasecmp(m_parser["Connection"].data(),"close") == 0) && ( ++m_iReqCnt < reqCnt);
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
	NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastHttpRequest,m_parser,invoker,(bool &)consumed);
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
	static string notFound =  mINI::Instance()[Config::Http::kNotFound];
	sendResponse("404 Not Found", makeHttpHeader(bClose, notFound.size()), notFound);
}

} /* namespace Http */
} /* namespace ZL */
