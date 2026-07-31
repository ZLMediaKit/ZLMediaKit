/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cerrno>

#include "AtomicFile.h"
#include "Util/util.h"

#if defined(_WIN32)
#include <Windows.h>
#else
#include <sys/stat.h>
#endif

using namespace std;
using namespace toolkit;

namespace mediakit {

FILE *createFileForAtomicReplace(const string &target_path, string &temporary_path) {
    for (size_t i = 0; i < 10; ++i) {
        temporary_path = target_path + ".tmp." + makeRandStr(16);
        errno = 0;
        auto file = fopen(temporary_path.c_str(), "wbx");
        if (file || errno != EEXIST) {
            return file;
        }
    }
    return nullptr;
}

bool atomicReplaceFile(const string &temporary_path, const string &target_path) {
#if defined(_WIN32)
    if (ReplaceFileA(target_path.c_str(), temporary_path.c_str(), nullptr, 0, nullptr, nullptr)) {
        return true;
    }
    if (GetLastError() != ERROR_FILE_NOT_FOUND) {
        return false;
    }
    return MoveFileExA(temporary_path.c_str(), target_path.c_str(), MOVEFILE_WRITE_THROUGH) != FALSE;
#else
    struct stat status;
    if (stat(target_path.c_str(), &status) == 0 && chmod(temporary_path.c_str(), status.st_mode & 0777) != 0) {
        return false;
    }
    return rename(temporary_path.c_str(), target_path.c_str()) == 0;
#endif
}

} // namespace mediakit
