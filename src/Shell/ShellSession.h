/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_SHELL_SHELLSESSION_H_
#define SRC_SHELL_SHELLSESSION_H_

#include <functional>
#include "Common/config.h"
#include "Util/TimeTicker.h"
#include "Network/TcpSession.h"
using namespace toolkit;

namespace mediakit {

class ShellSession: public TcpSession {
public:
    ShellSession(const Socket::Ptr &_sock);
    virtual ~ShellSession();

    void onRecv(const Buffer::Ptr &) override;
    void onError(const SockException &err) override;
    void onManager() override;

private:
    inline bool onCommandLine(const string &);
    inline void pleaseInputUser();
    inline void pleaseInputPasswd();
    inline void printShellPrefix();

    function<bool(const string &)> _loginInterceptor;
    string _strRecvBuf;
    Ticker _beatTicker;
    string _strUserName;
};

} /* namespace mediakit */

#endif /* SRC_SHELL_SHELLSESSION_H_ */
