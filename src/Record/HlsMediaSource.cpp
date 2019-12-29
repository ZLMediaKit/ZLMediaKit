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

#include "HlsMediaSource.h"

namespace mediakit{

HlsCookieData::HlsCookieData(const MediaInfo &info) {
    _info = info;
    addReaderCount();
}

void HlsCookieData::addReaderCount(){
    if(!_added){
        auto src = dynamic_pointer_cast<HlsMediaSource>(MediaSource::find(HLS_SCHEMA,_info._vhost,_info._app,_info._streamid));
        if(src){
            src->modifyReaderCount(true);
            _added = true;
        }
    }

}

HlsCookieData::~HlsCookieData() {
    if(_added){
        auto src = dynamic_pointer_cast<HlsMediaSource>(MediaSource::find(HLS_SCHEMA,_info._vhost,_info._app,_info._streamid));
        if(src){
            src->modifyReaderCount(false);
        }
    }
}

void HlsCookieData::addByteUsage(uint64_t bytes) {
    addReaderCount();
    _bytes += bytes;
}


}//namespace mediakit

