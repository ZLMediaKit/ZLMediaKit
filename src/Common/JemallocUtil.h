/*
* Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
*
* This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
*
* Use of this source code is governed by MIT license that can be found in the
* LICENSE file in the root of the source tree. All contributing project authors
* may be found in the AUTHORS file in the root of the source tree.
*/

#ifndef ZLMEDIAKIT_JEMALLOCUTIL_H
#define ZLMEDIAKIT_JEMALLOCUTIL_H
#include <functional>
#include <string>
#include <cstdint>
namespace mediakit {
class JemallocUtil {
public:
    JemallocUtil() = default;
    ~JemallocUtil() = default;

    static void enable_profiling();

    static void disable_profiling();

    static void dump(const std::string &file_name);
    static std::string get_malloc_stats();
    static void some_malloc_stats(const std::function<void(const char *, uint64_t)> &fn);
};
} // namespace mediakit
#endif // ZLMEDIAKIT_JEMALLOCUTIL_H
