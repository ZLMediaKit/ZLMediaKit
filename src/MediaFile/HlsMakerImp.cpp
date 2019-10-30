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

#include "HlsMakerImp.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
using namespace toolkit;

namespace mediakit {

HlsMakerImp::HlsMakerImp(const string &m3u8_file,
                         const string &params,
                         uint32_t bufSize,
                         float seg_duration,
                         uint32_t seg_number) : HlsMaker(seg_duration, seg_number) {
    _path_prefix = m3u8_file.substr(0, m3u8_file.rfind('/'));
    _path_hls = m3u8_file;
    _params = params;
    _buf_size = bufSize;
    _is_vod = seg_number == 0;
    _file_buf.reset(new char[bufSize],[](char *ptr){
        delete[] ptr;
    });
}

HlsMakerImp::~HlsMakerImp() {
    //录制完了
    flushLastSegment(true);
    if(!_is_vod){
        //hls直播才删除文件
        File::delete_file(_path_prefix.data());
    }
}

string HlsMakerImp::onOpenSegment(int index) {
    auto full_path = fullPath(index);
    _file = makeFile(full_path, true);
    if(!_file){
        WarnL << "create file falied," << full_path << " " <<  get_uv_errmsg();
    }
    //DebugL << index << " " << full_path;
    if(_params.empty()){
        return StrPrinter << index << ".ts";
    }
    return StrPrinter << index << ".ts" << "?" << _params;
}

void HlsMakerImp::onDelSegment(int index) {
    //WarnL << index;
    File::delete_file(fullPath(index).data());
}

void HlsMakerImp::onWriteSegment(const char *data, int len) {
    if (_file) {
        fwrite(data, len, 1, _file.get());
    }
}

void HlsMakerImp::onWriteHls(const char *data, int len) {
    auto hls = makeFile(_path_hls);
    if(hls){
        fwrite(data,len,1,hls.get());
        hls.reset();
    } else{
        WarnL << "create hls file falied," << _path_hls << " " <<  get_uv_errmsg();
    }
    //DebugL << "\r\n"  << string(data,len);
}

string HlsMakerImp::fullPath(int index) {
    return StrPrinter << _path_prefix << "/" << index << ".ts";
}

std::shared_ptr<FILE> HlsMakerImp::makeFile(const string &file,bool setbuf) {
    auto ret= shared_ptr<FILE>(File::createfile_file(file.data(), "wb"), [](FILE *fp) {
        if (fp) {
            fclose(fp);
        }
    });
    if(ret && setbuf){
        setvbuf(ret.get(), _file_buf.get(), _IOFBF, _buf_size);
    }
    return ret;
}

}//namespace mediakit