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

#ifndef HLSMAKERIMP_H
#define HLSMAKERIMP_H

#include <memory>
#include <string>
#include <stdlib.h>
#include "HlsMaker.h"
using namespace std;

namespace mediakit {

class HlsMakerImp : public HlsMaker{
public:
    HlsMakerImp(const string &m3u8_file,
                const string &params,
                uint32_t bufSize  = 64 * 1024,
                float seg_duration = 5,
                uint32_t seg_number = 3);
    virtual ~HlsMakerImp();
protected:
    string onOpenSegment(int index) override ;
    void onDelSegment(int index) override;
    void onWriteSegment(const char *data, int len) override;
    void onWriteHls(const char *data, int len) override;
private:
    string fullPath(int index);
    std::shared_ptr<FILE> makeFile(const string &file,bool setbuf = false);
private:
    std::shared_ptr<FILE> _file;
    std::shared_ptr<char> _file_buf;
    string _path_prefix;
    string _path_hls;
    string _params;
    int _buf_size;
    //是否为点播
    bool _is_vod;
};

}//namespace mediakit
#endif //HLSMAKERIMP_H
