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
#include "Common/config.h"
#include "Util/TimeTicker.h"
#include "Network/TcpSession.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

namespace ZL {
namespace Shell {

class ShellSession: public TcpSession {
public:
	ShellSession(const std::shared_ptr<ThreadPool> &_th, const Socket::Ptr &_sock);
	virtual ~ShellSession();

	void onRecv(const Buffer::Ptr &) override;
    void onError(const SockException &err) override {};
	void onManager() override;

private:
	inline bool onCommandLine(const string &);
	inline void pleaseInputUser();
	inline void pleaseInputPasswd();
	inline void printShellPrefix();

	function<bool(const string &)> m_loginInterceptor;
	string m_strRecvBuf;
	Ticker m_beatTicker;
	string m_strUserName;
};

} /* namespace Shell */
} /* namespace ZL */

#endif /* SRC_SHELL_SHELLSESSION_H_ */
