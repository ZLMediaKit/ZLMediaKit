/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_HTTPCLIENTTYPES_H_
#define SRC_HTTP_HTTPCLIENTTYPES_H_

#include <stdint.h>
#include <string>

#include "Util/util.h"

namespace mediakit {

enum class HttpClientScheme : uint8_t {
    Http = 0,
    Https = 1,
};

enum class HttpClientTransportAction : uint8_t {
    ReuseConnection = 0,
    ConnectDirect = 1,
    ConnectProxyTunnel = 2,
    ConnectQuic = 3,
};

struct HttpClientRequestPlan {
    StrCaseMap header;
    std::string url;
    std::string host;
    std::string host_header;
    std::string transport_host;
    std::string path;
    std::string last_host;
    std::string method;
    uint16_t port = 0;
    uint16_t transport_port = 0;
    HttpClientScheme scheme = HttpClientScheme::Http;
    bool keep_alive = true;
    bool persistent = false;
    bool host_changed = false;
    bool protocol_changed = false;
    bool auto_http3 = false;
};

} /* namespace mediakit */

#endif /* SRC_HTTP_HTTPCLIENTTYPES_H_ */
