/*
 * MIT License
 *
 * Copyright (c) 2016-2020 xiongziliang <771730766@qq.com>
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
