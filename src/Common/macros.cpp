/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "macros.h"
#include "config.h"

#include <atomic>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

using namespace toolkit;

#if defined(ENABLE_VERSION)
#include "ZLMVersion.h"
#endif

namespace mediakit {

/**
 * 本项目采用类MIT协议，用户在履行MIT协议义务的同时，应当同时遵循保留ZLMediaKit软件版权信息的义务。
 * 用户不得去除ZLMediaKit提供的各种服务中包括但不限于 "title"、"Server"、"User-Agent" 等字段中 "ZLMediaKit" 的信息。
 * 否则本项目主要权利人(项目发起人、主要作者)保留声索起诉的权利。
 * This project adopts a class MIT license. Users, while fulfilling the obligations of the MIT license, should also follow the obligation to retain the copyright information of ZLMediaKit software.
 * Users may not remove the "ZLMediaKit" information from the various services provided by ZLMediaKit, including but not limited to the "title", "Server", "User-Agent" fields.
 * Otherwise, the main rights holder of this project (project initiator, main author) reserves the right to claim and sue.
 
 
 * [AUTO-TRANSLATED:f214f734]
 */
#if !defined(ENABLE_VERSION)
static const char kServerNameDefault[] = "ZLMediaKit-9.0(build in " __DATE__ " " __TIME__ ")";
#else
static const char kServerNameDefault[] =
    "ZLMediaKit(git hash:" COMMIT_HASH "/" COMMIT_TIME ",branch:" BRANCH_NAME ",build time:" BUILD_TIME ")";
#endif

// 历史快照永不释放，避免并发读到已释放/已改写的 buffer
static std::mutex s_server_name_mtx;
static std::vector<std::unique_ptr<std::string>> s_server_name_keep;
static std::atomic<const char *> s_server_name { kServerNameDefault };

void setServerName(const char *name) {
    if (!name || !name[0]) {
        return;
    }
    // 禁止 CR/LF，避免注入到 HTTP/RTSP 头与 SDP
    if (std::strpbrk(name, "\r\n")) {
        WarnL << "reject server name containing CR/LF";
        return;
    }

    auto old_name = s_server_name.load(std::memory_order_acquire);
    auto holder = std::make_unique<std::string>(name);
    const char *published = holder->c_str();
    {
        std::lock_guard<std::mutex> lock(s_server_name_mtx);
        s_server_name_keep.emplace_back(std::move(holder));
    }
    s_server_name.store(published, std::memory_order_release);
    refreshDefaultHttpNotFound(old_name, published);
}

const char *getServerName() {
    return s_server_name.load(std::memory_order_acquire);
}

} // namespace mediakit
