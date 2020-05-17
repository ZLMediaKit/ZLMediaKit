/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_HTTPCLIENTIMP_H_
#define SRC_HTTP_HTTPCLIENTIMP_H_

#include "HttpClient.h"
#include "Util/SSLBox.h"
using namespace toolkit;
namespace mediakit {

class HttpClientImp: public TcpClientWithSSL<HttpClient> {
public:
    typedef std::shared_ptr<HttpClientImp> Ptr;
    HttpClientImp() {}
    virtual ~HttpClientImp() {}
protected:
    void onConnect(const SockException &ex)  override ;
};

} /* namespace mediakit */
#endif /* SRC_HTTP_HTTPCLIENTIMP_H_ */
