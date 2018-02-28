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

#include "common.h"
#include <stdarg.h>
#include <unordered_map>
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/TimeTicker.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Http/HttpSession.h"
#include "cleaner.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Rtmp;
using namespace ZL::Rtsp;
using namespace ZL::Http;

static TcpServer::Ptr s_pRtspSrv;
static TcpServer::Ptr s_pRtmpSrv;
static TcpServer::Ptr s_pHttpSrv;

//////////////////////////environment init///////////////////////////

API_EXPORT void API_CALL onAppStart(){
	static onceToken s_token([](){
		Logger::Instance().add(std::make_shared<ConsoleChannel>("stdout", LTrace));
		EventPoller::Instance(true);

		cleaner::Instance().push_back([](){
			s_pRtspSrv.reset();
			s_pRtmpSrv.reset();
			s_pHttpSrv.reset();
			WorkThreadPool::Destory();
			UDPServer::Destory();
			AsyncTaskThread::Destory();
			EventPoller::Destory();
			DebugL << "clear common" << endl;
			Logger::Destory();
		});
	},nullptr);
}


API_EXPORT void API_CALL onAppExit(){
	cleaner::Destory();
}
API_EXPORT void API_CALL setGlobalOptionString(const char *key,const char *val){
    if(mINI::Instance().find(key) == mINI::Instance().end()){
        WarnL << "key:" << key << " not existed!";
    }
    mINI::Instance()[key] = val;
}
API_EXPORT unsigned short API_CALL initHttpServer(unsigned short port){
	s_pHttpSrv.reset(new TcpServer());
	try {
		s_pHttpSrv->start<HttpSession>(port);
		return s_pHttpSrv->getPort();
	} catch (std::exception &ex) {
		s_pHttpSrv.reset();
		WarnL << ex.what();
		return 0;
	}
}
API_EXPORT unsigned short API_CALL initRtspServer(unsigned short port) {
	s_pRtspSrv.reset(new TcpServer());
	try {
		s_pRtspSrv->start<RtspSession>(port);
		return s_pRtspSrv->getPort();
	} catch (std::exception &ex) {
		s_pRtspSrv.reset();
		WarnL << ex.what();
		return 0;
	}
}

API_EXPORT unsigned short API_CALL initRtmpServer(unsigned short port) {
	s_pRtmpSrv.reset(new TcpServer());
	try {
		s_pRtmpSrv->start<RtmpSession>(port);
		return s_pRtmpSrv->getPort();
	} catch (std::exception &ex) {
		s_pRtmpSrv.reset();
		WarnL << ex.what();
		return 0;
	}
}


API_EXPORT void API_CALL log_printf(LogType level,const char* file, const char* function, int line,const char *fmt,...){
	LogInfoMaker info((LogLevel)level,file,function,line);
	va_list pArg;
	va_start(pArg, fmt);
	char buf[4096];
	int n = vsprintf(buf, fmt, pArg);
	buf[n] = '\0';
	va_end(pArg);
	info << buf;
}
API_EXPORT void API_CALL log_setLevel(LogType level){
	Logger::Instance().setLevel((LogLevel)level);
}

class LogoutChannel: public LogChannel {
public:
	LogoutChannel(const string &name, onLogOut cb, LogLevel level = LDebug)
:LogChannel(name, level, "%Y-%m-%d %H:%M:%S"){
		_cb = cb;
	}
	virtual ~LogoutChannel(){}
	void write(const LogInfo_ptr &logInfo){
		if (level() > logInfo->getLevel()) {
			return;
		}
		stringstream strStream;
		logInfo->format(strStream, timeFormat().data(), false);
		auto strTmp = strStream.str();
		if (_cb) {
			_cb(strTmp.data(), strTmp.size());
		}
	}
private:
	onLogOut _cb = nullptr;
};

API_EXPORT void API_CALL log_setOnLogOut(onLogOut cb){
	std::shared_ptr<LogoutChannel> chn(new LogoutChannel("LogoutChannel",cb,LTrace));
	Logger::Instance().add(chn);
}


