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

#ifndef HLSRECORDER_H
#define HLSRECORDER_H

#include "HlsMakerImp.h"
#include "TsMuxer.h"

namespace mediakit {

class HlsRecorder : public TsMuxer {
public:
    typedef std::shared_ptr<HlsRecorder> Ptr;
    HlsRecorder(const string &m3u8_file, const string &params){
        GET_CONFIG(uint32_t,hlsNum,Hls::kSegmentNum);
        GET_CONFIG(uint32_t,hlsBufSize,Hls::kFileBufSize);
        GET_CONFIG(uint32_t,hlsDuration,Hls::kSegmentDuration);
        _hls = new HlsMakerImp(m3u8_file,params,hlsBufSize,hlsDuration,hlsNum);
    }
    ~HlsRecorder(){
        delete _hls;
    }
    void setMediaSource(const string &vhost, const string &app, const string &stream_id){
        _hls->setMediaSource(vhost, app, stream_id);
    }

    MediaSource::Ptr getMediaSource() const{
        return _hls->getMediaSource();
    }
protected:
    void onTs(const void *packet, int bytes,uint32_t timestamp,int flags) override {
        _hls->inputData((char *)packet,bytes,timestamp);
    };
private:
    HlsMakerImp *_hls;
};

}//namespace mediakit

#endif //HLSRECORDER_H
