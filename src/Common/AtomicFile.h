/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_ATOMICFILE_H
#define ZLMEDIAKIT_ATOMICFILE_H

#include <cstdio>
#include <string>

namespace mediakit {

// Create an exclusively named temporary sibling of target_path.
FILE *createFileForAtomicReplace(const std::string &target_path, std::string &temporary_path);

// Verify that path still names the file represented by file before publishing it.
bool validateFileForAtomicReplace(FILE *file, const std::string &path);

// Atomically publish temporary_path at target_path, replacing an existing file.
bool atomicReplaceFile(const std::string &temporary_path, const std::string &target_path);

// Remove a temporary file without recursively traversing a substituted path.
bool removeFileForAtomicReplace(const std::string &temporary_path);

} // namespace mediakit

#endif // ZLMEDIAKIT_ATOMICFILE_H
