/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

namespace mediakit {

class ShellSession: public toolkit::TcpSession {
public:
    ShellSession(const toolkit::Socket::Ptr &_sock);
    virtual ~ShellSession();

    void onRecv(const toolkit::Buffer::Ptr &) override;
    void onError(const toolkit::SockException &err) override;
    void onManager() override;

private:
    inline bool onCommandLine(const std::string &);
    inline void pleaseInputUser();
    inline void pleaseInputPasswd();
    inline void printShellPrefix();

    std::function<bool(const std::string &)> _loginInterceptor;
    std::string _strRecvBuf;
    toolkit::Ticker _beatTicker;
    std::string _strUserName;
};

} /* namespace mediakit */

#endif /* SRC_SHELL_SHELLSESSION_H_ */
