/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef RECORDINFO_H_
#define RECORDINFO_H_

#include "Common/config.h"

namespace mediakit {

class RecordInfo {
public:
    time_t ui64StartedTime; // GMT 标准时间，单位秒
    time_t ui64TimeLen;     // 录像长度，需要注意 mp4 单位是秒，而 hls ts 单位是毫秒
    off_t  ui64FileSize;    // 文件大小，单位 BYTE
    string strFilePath;     // 文件路径
    string strFileName;     // 文件名称
    string strFolder;       // 文件夹路径
    string strUrl;          // 播放路径
    string strAppName;      // 应用名称
    string strStreamId;     // 流 ID
    string strVhost;        // vhost
};

} // namespace mediakit

#endif // RECORDINFO_H_
