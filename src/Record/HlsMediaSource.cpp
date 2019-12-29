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
    _added = std::make_shared<bool>(false);
    addReaderCount();
}

void HlsCookieData::addReaderCount(){
    if(!*_added){
        auto src = dynamic_pointer_cast<HlsMediaSource>(MediaSource::find(HLS_SCHEMA,_info._vhost,_info._app,_info._streamid));
        if(src){
            src->modifyReaderCount(true);
            *_added = true;
            _src = src;
            _ring_reader = src->getRing()->attach(EventPollerPool::Instance().getPoller());
            auto added = _added;
            _ring_reader->setDetachCB([added](){
                //HlsMediaSource已经销毁
                *added = false;
            });
        }
    }
}

HlsCookieData::~HlsCookieData() {
    if (*_added) {
        auto src = _src.lock();
        if (src) {
            src->modifyReaderCount(false);
        }
        auto duration = (_ticker.createdTime() - _ticker.elapsedTime()) / 1000;
        WarnL << "HLS播放器(" << _info._vhost << "/" << _info._app << "/" << _info._streamid << ")断开,播放时间:" << duration;
        GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);
        if (_bytes > iFlowThreshold * 1024) {
            NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _info, _bytes, duration, true);
        }
    }
}

void HlsCookieData::addByteUsage(uint64_t bytes) {
    addReaderCount();
    _bytes += bytes;
    _ticker.resetTime();
}


}//namespace mediakit

