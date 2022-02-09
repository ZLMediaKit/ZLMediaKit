/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_HTTPCLIENTIMP_H_
#define SRC_HTTP_HTTPCLIENTIMP_H_

#include "HttpClient.h"
#include "Util/SSLBox.h"

namespace mediakit {

class HttpClientImp : public toolkit::TcpClientWithSSL<HttpClient> {
public:
    using Ptr = std::shared_ptr<HttpClientImp>;
    HttpClientImp() = default;
    ~HttpClientImp() override = default;

protected:
    void onConnect(const toolkit::SockException &ex) override;
};

} /* namespace mediakit */
#endif /* SRC_HTTP_HTTPCLIENTIMP_H_ */
