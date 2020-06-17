/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP4_H
#define ZLMEDIAKIT_MP4_H
#ifdef ENABLE_MP4
#include <memory>
#include <string>
#include "mov-writer.h"
#include "mov-reader.h"
#include "mpeg4-hevc.h"
#include "mpeg4-avc.h"
#include "mpeg4-aac.h"
#include "mov-buffer.h"
#include "mov-format.h"
using namespace std;
namespace mediakit {

class MP4File {
public:
    friend struct mov_buffer_t;
    typedef std::shared_ptr<mov_writer_t> Writer;
    typedef std::shared_ptr<mov_reader_t> Reader;
    MP4File() = default;
    virtual ~MP4File() = default;

    Writer createWriter();
    Reader createReader();
    void openFile(const char *file,const char *mode);
    void closeFile();

    int onRead(void* data, uint64_t bytes);
    int onWrite(const void* data, uint64_t bytes);
    int onSeek( uint64_t offset);
    uint64_t onTell();
private:
    std::shared_ptr<FILE> _file;
};

}//namespace mediakit
#endif //NABLE_MP4RECORD
#endif //ZLMEDIAKIT_MP4_H
