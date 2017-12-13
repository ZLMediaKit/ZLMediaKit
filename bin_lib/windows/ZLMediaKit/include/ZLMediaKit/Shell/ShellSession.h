/*
 * ShellSession.h
 *
 *  Created on: 2016年9月26日
 *      Author: xzl
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
