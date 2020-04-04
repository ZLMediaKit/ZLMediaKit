/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBAPI_H
#define ZLMEDIAKIT_WEBAPI_H

#include <string>
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


void installWebApi();
void unInstallWebApi();
//配置文件路径
extern string g_ini_file;

#endif //ZLMEDIAKIT_WEBAPI_H
