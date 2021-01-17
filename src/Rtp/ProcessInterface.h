/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */


#ifndef ZLMEDIAKIT_PROCESSINTERFACE_H
#define ZLMEDIAKIT_PROCESSINTERFACE_H

#include <stdint.h>
#include <memory>

namespace mediakit {

class ProcessInterface {
public:
    using Ptr = std::shared_ptr<ProcessInterface>;
    ProcessInterface() = default;
    virtual ~ProcessInterface() = default;

    /**
      * 输入rtp
      * @param is_udp 是否为udp模式
      * @param data rtp数据指针
      * @param data_len rtp数据长度
      * @return 是否解析成功
      */
    virtual bool inputRtp(bool is_udp, const char *data, size_t data_len) = 0;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_PROCESSINTERFACE_H
