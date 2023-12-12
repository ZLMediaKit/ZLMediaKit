/*
* Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
*
* This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
*
* Use of this source code is governed by MIT-like license that can be found in the
* LICENSE file in the root of the source tree. All contributing project authors
* may be found in the AUTHORS file in the root of the source tree.
*/


#include "JemallocUtil.h"
#include "Util/logger.h"
#include <cstdint>
#ifdef USE_JEMALLOC
#include <array>
#include <iostream>
#include <jemalloc/jemalloc.h>
#endif

namespace mediakit {

void set_profile_active(bool active) {
#ifdef USE_JEMALLOC
    int err = mallctl("prof.active", nullptr, nullptr, (void *)&active, sizeof(active));
    if (err != 0) {
        WarnL << "mallctl failed with: " << err;
    }
#endif
}

void JemallocUtil::enable_profiling() {
    set_profile_active(true);
}
void JemallocUtil::disable_profiling() {
    set_profile_active(false);
}
void JemallocUtil::dump(const std::string &file_name) {
#ifdef USE_JEMALLOC
    auto *c_str = file_name.c_str();
    int err = mallctl("prof.dump", nullptr, nullptr, &c_str, sizeof(const char *));
    if (err != 0) {
        std::cerr << "mallctl failed with: " << err << std::endl;
    }
#endif
}
std::string JemallocUtil::get_malloc_stats() {
#ifdef USE_JEMALLOC
    std::string res;
    malloc_stats_print([](void *opaque, const char *s) { ((std::string *)opaque)->append(s); }, &res, "J");
    return res;
#else
    return "";
#endif
}

void JemallocUtil::some_malloc_stats(const std::function<void(const char *, uint64_t)> &fn) {
#ifdef USE_JEMALLOC
    constexpr std::array<const char *, 8> STATS = {
        "stats.allocated", "stats.active", "stats.metadata", "stats.metadata_thp",
        "stats.resident",  "stats.mapped", "stats.retained", "stats.zero_reallocs",
    };

    for (const char *stat : STATS) {
        size_t value;
        size_t len = sizeof(value);
        auto err = mallctl(stat, &value, &len, nullptr, 0);
        if (err != 0) {
            ErrorL << "Failed reading " << stat << ": " << err;
            continue;
        }
        fn(stat, value);
    }
#endif
}
} // namespace mediakit