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

#include "ShellSession.h"
#include "Util/CMD.h"
#include "Util/onceToken.h"
#include "Util/NoticeCenter.h"
#include "Common/config.h"
#include "ShellCMD.h"
using namespace toolkit;

namespace mediakit {

static onceToken s_token([]() {
    REGIST_CMD(media);
}, nullptr);

ShellSession::ShellSession(const Socket::Ptr &_sock) : TcpSession(_sock) {
    DebugP(this);
    pleaseInputUser();
}

ShellSession::~ShellSession() {
    DebugP(this);
}

void ShellSession::onRecv(const Buffer::Ptr&buf) {
    //DebugL << hexdump(buf->data(), buf->size());
    GET_CONFIG(uint32_t,maxReqSize,Shell::kMaxReqSize);
    if (_strRecvBuf.size() + buf->size() >= maxReqSize) {
        shutdown(SockException(Err_other,"recv buffer overflow"));
        return;
    }
    _beatTicker.resetTime();
    _strRecvBuf.append(buf->data(), buf->size());
    if (_strRecvBuf.find("\xff\xf4\xff\0xfd\x06") != std::string::npos) {
        send("\033[0m\r\n	Bye bye!\r\n");
        shutdown(SockException(Err_other,"received Ctrl+C"));
        return;
    }
    size_t index;
    string line;
    while ((index = _strRecvBuf.find("\r\n")) != std::string::npos) {
        line = _strRecvBuf.substr(0, index);
        _strRecvBuf.erase(0, index + 2);
        if (!onCommandLine(line)) {
            shutdown(SockException(Err_other,"exit cmd"));
            return;
        }
    }
}

void ShellSession::onError(const SockException &err){
    WarnP(this) << err.what();
}

void ShellSession::onManager() {
    if (_beatTicker.elapsedTime() > 1000 * 60 * 5) {
        //5 miniutes for alive
        shutdown(SockException(Err_timeout,"session timeout"));
        return;
    }
}

inline bool ShellSession::onCommandLine(const string& line) {
    auto loginInterceptor = _loginInterceptor;
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
    _loginInterceptor = [this](const string &user_name) {
        _strUserName=user_name;
        pleaseInputPasswd();
        return true;
    };
}
inline void ShellSession::pleaseInputPasswd() {
    send("Password: \033[8m");
    _loginInterceptor = [this](const string &passwd) {
        auto onAuth = [this](const string &errMessage){
            if(!errMessage.empty()){
                //鉴权失败
                send(StrPrinter
                     <<"\033[0mAuth failed("
                     << errMessage
                     <<"), please try again.\r\n"
                     <<_strUserName<<"@"<<SERVER_NAME
                     <<"'s password: \033[8m"
                     <<endl);
                return;
            }
            send("\033[0m");
            send("-----------------------------------------\r\n");
            send(StrPrinter<<"欢迎来到"<<SERVER_NAME<<", 你可输入\"help\"查看帮助.\r\n"<<endl);
            send("-----------------------------------------\r\n");
            printShellPrefix();
            _loginInterceptor=nullptr;
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

        auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastShellLogin,_strUserName,passwd,invoker,*this);
        if(!flag){
            //如果无人监听shell登录事件，那么默认shell无法登录
            onAuth("please listen kBroadcastShellLogin event");
        }
        return true;
    };
}

inline void ShellSession::printShellPrefix() {
    send(StrPrinter << _strUserName << "@" << SERVER_NAME << "# " << endl);
}

}/* namespace mediakit */
