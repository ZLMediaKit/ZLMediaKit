/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBHOOK_H
#define ZLMEDIAKIT_WEBHOOK_H

#include <string>
#include <functional>
#include "jsoncpp/json.h"

//支持json或urlencoded方式传输参数
#define JSON_ARGS

#ifdef JSON_ARGS
typedef Json::Value ArgsType;
#else
typedef mediakit::HttpArgs ArgsType;
#endif

namespace Hook {
//web hook回复最大超时时间
extern const std::string kTimeoutSec;
}//namespace Hook

void installWebHook();
void unInstallWebHook();
/**
 * 触发http hook请求
 * @param url 请求地址
 * @param body 请求body
 * @param func 回调
 */
void do_http_hook(const std::string &url, const ArgsType &body, const std::function<void(const Json::Value &, const std::string &)> &func = nullptr);
#endif //ZLMEDIAKIT_WEBHOOK_H
