/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ZLMEDIAKIT_WEBAPI_H
#define ZLMEDIAKIT_WEBAPI_H

#include <string>
#include "FFmpegSource.h"
using namespace std;

namespace mediakit {

////////////RTSP服务器配置///////////
namespace Rtsp {
extern const string kPort;
} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
extern const string kPort;
} //namespace RTMP

}  // namespace mediakit

//chenxiaolei 新增数据库配置的通道使用的proxyMap
extern unordered_map<string ,PlayerProxy::Ptr> m_s_proxyMap;
extern recursive_mutex m_s_proxyMapMtx;

//chenxiaolei 新增数据库配置的通道使用的proxyMap
extern unordered_map<string ,FFmpegSource::Ptr> m_s_ffmpegMap;
extern recursive_mutex m_s_ffmpegMapMtx;

//chenxiaolei 配置生效方法
extern void processProxyCfg(const Json::Value &proxyData, const bool initialize);
//chenxiaolei 配置(数组,多个)生效方法
extern void processProxyCfgs(const Json::Value &cfg_root);

extern void installWebApi();
extern void unInstallWebApi();


#endif //ZLMEDIAKIT_WEBAPI_H
