/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_EPOLL_H
#define ZLMEDIAKIT_EPOLL_H
#include "wepoll.h"
#include <map>
#include <mutex>

// 屏蔽 EPOLLET
#define EPOLLET 0

namespace toolkit {
// 索引handle
extern std::map<int, HANDLE> s_wepollHandleMap;
extern int s_handleIndex;
extern std::mutex s_handleMtx;
// 屏蔽epoll_create epoll_ctl epoll_wait参数差异
inline int epoll_create(int size) {
    HANDLE handle = ::epoll_create(size);
    if (!handle) {
        return -1;
    }
    {
        std::lock_guard<std::mutex> lck(s_handleMtx);
        int idx = ++s_handleIndex;
        s_wepollHandleMap[idx] = handle;
        return idx;
    }
}

inline int epoll_ctl(int ephnd, int op, SOCKET sock, struct epoll_event *ev) {
    HANDLE handle;
    {
        std::lock_guard<std::mutex> lck(s_handleMtx);
        handle = s_wepollHandleMap[ephnd];
    }
    return ::epoll_ctl(handle, op, sock, ev);
}

inline int epoll_wait(int ephnd, struct epoll_event *events, int maxevents, int timeout) {
    HANDLE handle;
    {
        std::lock_guard<std::mutex> lck(s_handleMtx);
        handle = s_wepollHandleMap[ephnd];
    }
    return ::epoll_wait(handle, events, maxevents, timeout);
}

} // namespace toolkit

#endif // ZLMEDIAKIT_EPOLL_H
