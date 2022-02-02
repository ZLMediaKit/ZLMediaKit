/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_HTTPCONST_H
#define ZLMEDIAKIT_HTTPCONST_H

#include <string>

namespace mediakit{

/**
 * 根据http错误代码获取字符说明
 * @param status 譬如404
 * @return 错误代码字符说明，譬如Not Found
 */
const char *getHttpStatusMessage(int status);

/**
 * 根据文件后缀返回http mime
 * @param name 文件后缀，譬如html
 * @return mime值，譬如text/html
 */
const std::string &getHttpContentType(const char *name);

}//mediakit

#endif //ZLMEDIAKIT_HTTPCONST_H
