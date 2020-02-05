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
    _file_buf.reset(new char[bufSize],[](char *ptr){
        delete[] ptr;
    });
}

HlsMakerImp::~HlsMakerImp() {
    //录制完了
    flushLastSegment(true);
    if(isLive()){
        //hls直播才删除文件
        File::delete_file(_path_prefix.data());
    }
}

string HlsMakerImp::onOpenSegment(int index) {
    string segment_name , segment_path;
    {
        auto strDate = getTimeStr("%Y-%m-%d");
        auto strHour = getTimeStr("%H");
        auto strTime = getTimeStr("%M-%S");
        segment_name = StrPrinter << strDate + "/" + strHour + "/" + strTime << "_" << index << ".ts";
        segment_path = _path_prefix + "/" +  segment_name;
        if(isLive()){
            _segment_file_paths.emplace(index,segment_path);
        }
    }
    _file = makeFile(segment_path, true);
    if(!_file){
        WarnL << "create file falied," << segment_path << " " <<  get_uv_errmsg();
    }
    if(_params.empty()){
        return std::move(segment_name);
    }
    return std::move(segment_name + "?" + _params);
}

void HlsMakerImp::onDelSegment(int index) {
    auto it = _segment_file_paths.find(index);
    if(it == _segment_file_paths.end()){
        return;
    }
    File::delete_file(it->second.data());
    _segment_file_paths.erase(it);
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
        if(_media_src){
            _media_src->registHls();
        }
    } else{
        WarnL << "create hls file falied," << _path_hls << " " <<  get_uv_errmsg();
    }
    //DebugL << "\r\n"  << string(data,len);
}


std::shared_ptr<FILE> HlsMakerImp::makeFile(const string &file,bool setbuf) {
    auto file_buf = _file_buf;
    auto ret= shared_ptr<FILE>(File::createfile_file(file.data(), "wb"), [file_buf](FILE *fp) {
        if (fp) {
            fclose(fp);
        }
    });
    if(ret && setbuf){
        setvbuf(ret.get(), _file_buf.get(), _IOFBF, _buf_size);
    }
    return ret;
}

void HlsMakerImp::setMediaSource(const string &vhost, const string &app, const string &stream_id) {
    _media_src = std::make_shared<HlsMediaSource>(vhost, app, stream_id);
}

MediaSource::Ptr HlsMakerImp::getMediaSource() const{
    return _media_src;
}

}//namespace mediakit