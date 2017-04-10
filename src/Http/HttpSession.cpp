/*
 * HttpSession.cpp
 *
 *  Created on: 2016年9月22日
 *      Author: xzl
 */

#include <stdio.h>
#include <sys/stat.h>
#include <algorithm>
#include <dirent.h>

#include "config.h"
#include "strCoding.h"
#include "HttpSession.h"
#include "Util/File.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "Util/onceToken.h"
#include "Util/mini.hpp"
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

void HttpSession::onRecv(const Socket::Buffer::Ptr&pBuf) {
	static uint32_t reqSize =  mINI::Instance()[Config::Http::kMaxReqSize].as<uint32_t>();
	m_ticker.resetTime();
	if (m_strRcvBuf.size() + pBuf->size() >= reqSize) {
		WarnL << "接收缓冲区溢出!";
		shutdown();
		return;
	}
	m_strRcvBuf.append(pBuf->data(), pBuf->size());
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
		WarnL<<"HttpSession超时断开！";
		shutdown();
	}
}

inline HttpSession::HttpCode HttpSession::Handle_Req_GET() {
	string strUrl = strCoding::UrlUTF8Decode(m_parser.Url());
	string strFile = m_strPath + strUrl;
	string strConType = m_parser["Connection"];
	static uint32_t reqCnt =  mINI::Instance()[Config::Http::kMaxReqCount].as<uint32_t>();
	bool bClose = (strcasecmp(strConType.data(),"close") == 0) && ( ++m_iReqCnt < reqCnt);
	HttpCode eHttpCode = bClose ? Http_failed : Http_success;
	if (strFile.back() == '/') {
		//index the folder
		string strMeun;
		if (!makeMeun(strFile, strMeun)) {
			sendNotFound(bClose);
			return eHttpCode;
		}
		sendResponse("200 OK", makeHttpHeader(bClose,strMeun.size() ), strMeun);
		return eHttpCode;
	}
	//download the file
	struct stat tFileStat;
	if (0 != stat(strFile.data(), &tFileStat)) {
		sendNotFound(bClose);
		return eHttpCode;
	}

	TimeTicker();
	FILE *pFile = fopen(strFile.data(), "rb");
	if (pFile == NULL) {
		sendNotFound(bClose);
		return eHttpCode;
	}

	auto &strRange = m_parser["Range"];
	int64_t iRangeStart = 0, iRangeEnd = 0;
	iRangeStart = atoll(FindField(strRange.data(), "bytes=", "-").data());
	iRangeEnd = atoll(FindField(strRange.data(), "-", "\r\n").data());
	if (iRangeEnd == 0) {
		iRangeEnd = tFileStat.st_size - 1;
	}
	const char *pcHttpResult = NULL;
	if (strRange.size() == 0) {
		pcHttpResult = "200 OK";
	} else {
		pcHttpResult = "206 Partial Content";
		fseek(pFile, iRangeStart, SEEK_SET);
	}

	auto httpHeader=makeHttpHeader(bClose, iRangeEnd - iRangeStart + 1, get_mime_type(strUrl.data()));
	if (strRange.size() != 0) {
		httpHeader.emplace("Content-Range",StrPrinter<<"bytes " << iRangeStart << "-" << iRangeEnd << "/" << tFileStat.st_size<< endl);
	}

	sendResponse(pcHttpResult, httpHeader, "");
	if (iRangeEnd - iRangeStart < 0) {
		//file is empty!
		return eHttpCode;
	}

	//send the file
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
			//InfoL << "Send file ：" << iReq << " " << *piLeft;
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
		strRet += "上一级目录";
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
	headerOut.emplace("Connection", bClose ? "close" :
			StrPrinter << "keep-alive: timeout=" << keepAliveSec
			<< ", max=" << reqCnt << endl);
	headerOut.emplace("Date", dateStr());
	if(iContentSize >=0 && pcContentType !=nullptr){
		auto strContentType = StrPrinter << pcContentType << "; charset=" << charSet << endl;
		headerOut.emplace("Content-Type",strContentType.data());
		headerOut.emplace("Content-Length", StrPrinter<<iContentSize<<endl);
	}
	return headerOut;
}
inline HttpSession::HttpCode HttpSession::Handle_Req_POST() {
	int iContentLen = atoi(m_parser["Content-Length"].data());
	if (!iContentLen) {
		return Http_failed;
	}
	if ((int) m_strRcvBuf.size() < iContentLen) {
		return Http_moreData; //需要更多数据
	}
	auto strContent = m_strRcvBuf.substr(0, iContentLen);
	m_strRcvBuf.erase(0, iContentLen);

	string strUrl = strCoding::UrlUTF8Decode(m_parser.Url());
	string strConType = m_parser["Connection"];
	static uint32_t reqCnt = mINI::Instance()[Config::Http::kMaxReqCount].as<uint32_t>();
	bool bClose = (strcasecmp(strConType.data(),"close") == 0) && ( ++m_iReqCnt < reqCnt);
	m_parser.setUrl(strUrl);
	m_parser.setContent(strContent);

	weak_ptr<HttpSession> weakSelf = dynamic_pointer_cast<HttpSession>(shared_from_this());
	HttpResponseInvoker invoker = [weakSelf,bClose](const string &codeOut,
													const KeyValue &headerOut,
													const string &contentOut){
		auto strongSelf = weakSelf.lock();
		if(!strongSelf) {
			return;
		}
		strongSelf->async([weakSelf,bClose,codeOut,headerOut,contentOut]() {
			auto strongSelf = weakSelf.lock();
			if(!strongSelf) {
				return;
			}
			strongSelf->responseDelay(bClose,
					const_cast<string &>(codeOut),
					const_cast<KeyValue &>(headerOut),
					const_cast<string &>(contentOut));
			if(bClose){
				strongSelf->shutdown();
			}
		});
	};
	if(!NoticeCenter::Instance().emitEvent(Config::Broadcast::kBroadcastHttpRequest,m_parser,invoker)){
		invoker("404 Not Found",KeyValue(),"");
	}
	return Http_success;
}
void HttpSession::responseDelay(bool bClose,string &codeOut,KeyValue &headerOut, string &contentOut){
	if(codeOut.empty()){
		sendNotFound(bClose);
		return;
	}
	auto headerOther=makeHttpHeader(bClose,contentOut.size(),"text/json");
	headerOut.insert(headerOther.begin(), headerOther.end());
	sendResponse(codeOut.data(), headerOut, contentOut);
}
inline void HttpSession::sendNotFound(bool bClose) {
	static string notFound =  mINI::Instance()[Config::Http::kNotFound];
	sendResponse("404 Not Found", makeHttpHeader(bClose, notFound.size()), notFound);
}

} /* namespace Http */
} /* namespace ZL */
