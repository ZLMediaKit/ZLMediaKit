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

#include "ShellSession.h"
#include "Common/config.h"
#include "Util/CMD.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"

using namespace Config;
using namespace ZL::Util;

namespace ZL {
namespace Shell {

ShellSession::ShellSession(const std::shared_ptr<ThreadPool> &_th,
                           const Socket::Ptr &_sock) :
        TcpSession(_th, _sock) {
    pleaseInputUser();
}

ShellSession::~ShellSession() {
}

void ShellSession::onRecv(const Buffer::Ptr&buf) {
	//DebugL << hexdump(buf->data(), buf->size());
    GET_CONFIG_AND_REGISTER(uint32_t,maxReqSize,Config::Shell::kMaxReqSize);
    if (m_strRecvBuf.size() + buf->size() >= maxReqSize) {
		WarnL << "接收缓冲区溢出!";
		shutdown();
		return;
	}
	m_beatTicker.resetTime();
	m_strRecvBuf.append(buf->data(), buf->size());
	if (m_strRecvBuf.find("\xff\xf4\xff\0xfd\x06") != std::string::npos) {
		WarnL << "收到Ctrl+C.";
		send("\033[0m\r\n	Bye bye!\r\n");
		shutdown();
		return;
	}
	size_t index;
	string line;
	while ((index = m_strRecvBuf.find("\r\n")) != std::string::npos) {
		line = m_strRecvBuf.substr(0, index);
		m_strRecvBuf.erase(0, index + 2);
		if (!onCommandLine(line)) {
			shutdown();
			return;
		}
	}
}

void ShellSession::onManager() {
	if (m_beatTicker.elapsedTime() > 1000 * 60 * 5) {
		//5 miniutes for alive
		shutdown();
		return;
	}
}

inline bool ShellSession::onCommandLine(const string& line) {
    auto loginInterceptor = m_loginInterceptor;
    if (loginInterceptor) {
		bool ret = loginInterceptor(line);
		return ret;
	}
    try {
        std::shared_ptr<stringstream> ss(new stringstream);
        CMDRegister::Instance()(line,ss);
        send(ss->str());
    }catch(ExitException &ex){
        return false;
    }catch(std::exception &ex){
        send(ex.what());
        send("\r\n");
    }
    printShellPrefix();
	return true;
}

inline void ShellSession::pleaseInputUser() {
	send("\033[0m");
	send(StrPrinter << SERVER_NAME << " login: " << endl);
	m_loginInterceptor = [this](const string &user_name) {
		m_strUserName=user_name;
        pleaseInputPasswd();
		return true;
	};
}
inline void ShellSession::pleaseInputPasswd() {
	send("Password: \033[8m");
	m_loginInterceptor = [this](const string &passwd) {
        auto onAuth = [this](const string &errMessage){
            if(!errMessage.empty()){
                //鉴权失败
                send(StrPrinter
                     <<"\033[0mAuth failed("
                     << errMessage
                     <<"), please try again.\r\n"
                     <<m_strUserName<<"@"<<SERVER_NAME
                     <<"'s password: \033[8m"
                     <<endl);
                return;
            }
            send("\033[0m");
            send("-----------------------------------------\r\n");
            send(StrPrinter<<"欢迎来到"<<SERVER_NAME<<", 你可输入\"help\"查看帮助.\r\n"<<endl);
            send("-----------------------------------------\r\n");
            printShellPrefix();
            m_loginInterceptor=nullptr;
        };

        weak_ptr<ShellSession> weakSelf = dynamic_pointer_cast<ShellSession>(shared_from_this());
        Broadcast::AuthInvoker invoker = [weakSelf,onAuth](const string &errMessage){
            auto strongSelf =  weakSelf.lock();
            if(!strongSelf){
                return;
            }
            strongSelf->async([errMessage,weakSelf,onAuth](){
                auto strongSelf =  weakSelf.lock();
                if(!strongSelf){
                    return;
                }
                onAuth(errMessage);
            });
        };

        auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastShellLogin,m_strUserName,passwd,invoker,*this);
        if(!flag){
            //如果无人监听shell登录事件，那么默认shell无法登录
            onAuth("please listen kBroadcastShellLogin event");
        }
		return true;
	};
}

inline void ShellSession::printShellPrefix() {
	send(StrPrinter << m_strUserName << "@" << SERVER_NAME << "# " << endl);
}

}/* namespace Shell */
} /* namespace ZL */
