/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLTOOLKIT_BUFFERSOCK_H
#define ZLTOOLKIT_BUFFERSOCK_H

#if !defined(_WIN32)
#include <sys/uio.h>
#include <limits.h>
#endif
#include <cassert>
#include <memory>
#include <string>
#include <vector>
#include <type_traits>
#include <functional>
#include "Util/util.h"
#include "Util/List.h"
#include "Util/ResourcePool.h"
#include "sockutil.h"
#include "Buffer.h"

namespace toolkit {

#if !defined(IOV_MAX)
#define IOV_MAX 1024
#endif

class BufferSock : public Buffer {
public:
    using Ptr = std::shared_ptr<BufferSock>;
    BufferSock(Buffer::Ptr ptr, struct sockaddr *addr = nullptr, int addr_len = 0);
    ~BufferSock() override = default;

    char *data() const override;
    size_t size() const override;
    const struct sockaddr *sockaddr() const;
    socklen_t  socklen() const;

private:
    int _addr_len = 0;
    struct sockaddr_storage _addr;
    Buffer::Ptr _buffer;
};

class BufferList : public noncopyable {
public:
    using Ptr = std::shared_ptr<BufferList>;
    using SendResult = std::function<void(const Buffer::Ptr &buffer, bool send_success)>;

    BufferList() = default;
    virtual ~BufferList() = default;

    virtual bool empty() = 0;
    virtual size_t count() = 0;
    virtual ssize_t send(int fd, int flags) = 0;

    static Ptr create(List<std::pair<Buffer::Ptr, bool> > list, SendResult cb, bool is_udp);

private:
    //对象个数统计
    ObjectStatistic<BufferList> _statistic;
};

}
#endif //ZLTOOLKIT_BUFFERSOCK_H
