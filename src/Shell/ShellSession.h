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

#ifndef SRC_SHELL_SHELLSESSION_H_
#define SRC_SHELL_SHELLSESSION_H_

#include <functional>
#include "CMD.h"
#include "Common/config.h"
#include "Util/TimeTicker.h"
#include "Network/TcpLimitedSession.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Shell {

class ShellSession: public TcpLimitedSession<MAX_TCP_SESSION>, public OutStream {
public:
	ShellSession(const std::shared_ptr<ThreadPool> &_th, const Socket::Ptr &_sock);
	virtual ~ShellSession();
	void onRecv(const Socket::Buffer::Ptr &) override;
	void onError(const SockException &err) override;
	void onManager() override;
	static void addUser(const string &userName,const string &userPwd){
		g_mapUser[userName] = userPwd;
	}
private:
	friend class CMD_help;
	inline bool onProcessLine(const string &);
	inline void requestLogin();
	inline void requestPasswd();
	inline void sendHead();
	inline bool authUser(const string &user, const string &pwd);
	void response(const string &str) override{
		send(str);
	}
	string &operator[](const string &key) override{
		return m_mapConfig[key];
	}
	int erase(const string &key) override{
		return m_mapConfig.erase(key);
	}

	function<bool(const string &)> m_requestCB;
	string m_strRecvBuf;
	Ticker m_beatTicker;
	string m_strUserName;
	unordered_map<string,string> m_mapConfig;
	static unordered_map<string, string> g_mapUser;
	static map<string, CMD&> g_mapCmd;
	static string g_serverName;
};

} /* namespace Shell */
} /* namespace ZL */

#endif /* SRC_SHELL_SHELLSESSION_H_ */
