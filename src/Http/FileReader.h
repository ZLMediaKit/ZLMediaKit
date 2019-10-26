/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#ifndef ZLMEDIAKIT_FILEREADER_H
#define ZLMEDIAKIT_FILEREADER_H

#include <stdlib.h>
#include <memory>
#include "Network/Buffer.h"
using namespace std;
using namespace toolkit;

class FileReader {
public:
    typedef std::shared_ptr<FileReader> Ptr;
    FileReader(const std::shared_ptr<FILE> &fp,uint64_t offset,uint64_t max_size);
    ~FileReader();
    Buffer::Ptr read(int size);
private:
    std::shared_ptr<FILE> _fp;
    uint64_t _max_size;
    uint64_t _read_offset = 0;
    std::shared_ptr<char> _map_addr;
};


#endif //ZLMEDIAKIT_FILEREADER_H
