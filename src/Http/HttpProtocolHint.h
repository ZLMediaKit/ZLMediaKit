/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_HTTP_HTTPPROTOCOLHINT_H_
#define SRC_HTTP_HTTPPROTOCOLHINT_H_

#include <stdint.h>
#include <string>

#include "Common/Parser.h"

namespace mediakit {

struct HttpAltSvcEndpoint {
    std::string host;
    uint16_t port = 0;
};

void appendAltSvcHeader(StrCaseMap &headers);
void updateAltSvcCache(const std::string &origin_host, uint16_t origin_port, const std::string &alt_svc);
bool lookupAltSvc(const std::string &origin_host, uint16_t origin_port, HttpAltSvcEndpoint *endpoint);
void eraseAltSvc(const std::string &origin_host, uint16_t origin_port);

} // namespace mediakit

#endif /* SRC_HTTP_HTTPPROTOCOLHINT_H_ */
