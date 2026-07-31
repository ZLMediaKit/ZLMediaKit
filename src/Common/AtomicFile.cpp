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
#include <io.h>
#else
#include <sys/stat.h>
#include <unistd.h>
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

bool validateFileForAtomicReplace(FILE *file, const string &path) {
#if defined(_WIN32)
    auto handle = (HANDLE)_get_osfhandle(_fileno(file));
    BY_HANDLE_FILE_INFORMATION opened_info;
    if (handle == INVALID_HANDLE_VALUE || !GetFileInformationByHandle(handle, &opened_info)) {
        return false;
    }
    auto path_handle = CreateFileA(path.c_str(), FILE_READ_ATTRIBUTES,
                                   FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE,
                                   nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (path_handle == INVALID_HANDLE_VALUE) {
        return false;
    }
    BY_HANDLE_FILE_INFORMATION path_info;
    auto success = GetFileInformationByHandle(path_handle, &path_info) &&
                   opened_info.dwVolumeSerialNumber == path_info.dwVolumeSerialNumber &&
                   opened_info.nFileIndexHigh == path_info.nFileIndexHigh &&
                   opened_info.nFileIndexLow == path_info.nFileIndexLow;
    CloseHandle(path_handle);
    return success;
#else
    struct stat opened_status;
    struct stat path_status;
    return fstat(fileno(file), &opened_status) == 0 && lstat(path.c_str(), &path_status) == 0 &&
           S_ISREG(opened_status.st_mode) && opened_status.st_dev == path_status.st_dev &&
           opened_status.st_ino == path_status.st_ino;
#endif
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

bool removeFileForAtomicReplace(const string &temporary_path) {
#if defined(_WIN32)
    return DeleteFileA(temporary_path.c_str()) != FALSE || GetLastError() == ERROR_FILE_NOT_FOUND;
#else
    return unlink(temporary_path.c_str()) == 0 || errno == ENOENT;
#endif
}

} // namespace mediakit
