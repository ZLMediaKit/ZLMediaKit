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

#include "Common/config.h"
#include "ShellSession.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Util/mini.h"

using namespace ZL::Util;

namespace ZL {
namespace Shell {

unordered_map<string, string> ShellSession::g_mapUser;
map<string, CMD &> ShellSession::g_mapCmd;
string ShellSession::g_serverName;

#define SHELL_REG_CMD(cmd) g_mapCmd.emplace(#cmd,CMDInstance<CMD_##cmd>::Instance())
ShellSession::ShellSession(const std::shared_ptr<ThreadPool> &_th,
		const Socket::Ptr &_sock) :
		TcpLimitedSession(_th, _sock) {
	static onceToken token([]() {
		SHELL_REG_CMD(rtmp);
		SHELL_REG_CMD(rtsp);
		SHELL_REG_CMD(help);
		g_serverName = mINI::Instance()[Config::Shell::kServerName];
	}, nullptr);
	requestLogin();
}

ShellSession::~ShellSession() {
}

void ShellSession::onRecv(const Socket::Buffer::Ptr&buf) {
	//DebugL << hexdump(buf->data(), buf->size());
	static uint32_t maxReqSize = mINI::Instance()[Config::Shell::kMaxReqSize].as<uint32_t>();
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
		if (!onProcessLine(line)) {
			shutdown();
			return;
		}
	}
}

void ShellSession::onError(const SockException& err) {
}

void ShellSession::onManager() {
	if (m_beatTicker.elapsedTime() > 1000 * 60 * 5) {
		//5 miniutes for alive
		shutdown();
		return;
	}
}
static int getArgs(char *buf, char *argv[], int max_argc = 16) {
	int argc = 0;
	bool start = false;
	int len = strlen(buf);
	for (int i = 0; i < len; i++) {
		if (argc == max_argc) {
			break;
		}
		if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\r'
				&& buf[i] != '\n') {
			if (!start) {
				start = true;
				argv[argc++] = buf + i;
			}
		} else {
			buf[i] = '\0';
			start = false;
		}

	}
	return argc;
}
inline bool ShellSession::onProcessLine(const string& line) {
	if (m_requestCB) {
		bool ret = m_requestCB(line);
		return ret;
	}
	//cmd process
	char *argv[16];
	int argc = getArgs((char *) line.data(), argv, sizeof(argv));
	if (argc == 0) {
		sendHead();
		return true;
	}
	string cmd = argv[0];
	auto it = g_mapCmd.find(cmd);
	if (it == g_mapCmd.end()) {
		auto sendStr = StrPrinter << "\tUnrecognized option:\"" << cmd << "\",Enter \"help\" to get help.\r\n" << endl;
		send(sendStr);
		sendHead();
		return true;
	}
	bool ret = it->second(this, argc, argv);
	sendHead();
	return ret;
}

inline void ShellSession::requestLogin() {
	send("\033[0m");
	send(StrPrinter << g_serverName << " login: " << endl);
	m_requestCB = [this](const string &line) {
		m_strUserName=line;
		requestPasswd();
		return true;
	};
}
inline void ShellSession::requestPasswd() {
	send("Password: \033[8m");
	m_requestCB = [this](const string &passwd) {
		if(!authUser(m_strUserName,passwd)) {
			send(StrPrinter
					<<"\033[0mPermission denied,"
					<<" please try again.\r\n"
					<<m_strUserName<<"@"<<g_serverName
					<<"'s password: \033[8m"
					<<endl);
			return true;
		}
		send("\033[0m");
		send("-----------------------------------------\r\n");
		send(StrPrinter<<"欢迎来到"<<g_serverName<<", 你可输入\"help\"查看帮助.\r\n"<<endl);
		send("-----------------------------------------\r\n");
		sendHead();
		m_requestCB=nullptr;
		return true;
	};
}

inline void ShellSession::sendHead() {
	send(StrPrinter << m_strUserName << "@" << g_serverName << ":" << endl);
}

inline bool ShellSession::authUser(const string& user, const string& pwd) {
	auto it = g_mapUser.find(user);
	if (it == g_mapUser.end()) {
		//WarnL << user << " " << pwd;
		return false;
	}
	return it->second == pwd;
}

}
/* namespace Shell */
} /* namespace ZL */
