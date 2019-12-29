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

#ifndef ZLMEDIAKIT_RTMPMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_RTMPMEDIASOURCEMUXER_H

#include "RtmpMuxer.h"
#include "Rtmp/RtmpMediaSource.h"

namespace mediakit {

class RtmpMediaSourceMuxer : public RtmpMuxer {
public:
    typedef std::shared_ptr<RtmpMediaSourceMuxer> Ptr;

    RtmpMediaSourceMuxer(const string &vhost,
                         const string &strApp,
                         const string &strId,
                         const TitleMeta::Ptr &title = nullptr) : RtmpMuxer(title){
        _mediaSouce = std::make_shared<RtmpMediaSource>(vhost,strApp,strId);
        getRtmpRing()->setDelegate(_mediaSouce);
    }
    virtual ~RtmpMediaSourceMuxer(){}

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        _mediaSouce->setListener(listener);
    }

    void setTimeStamp(uint32_t stamp){
        _mediaSouce->setTimeStamp(stamp);
    }

    int readerCount() const{
        return _mediaSouce->readerCount();
    }

    void onAllTrackReady(){
        _mediaSouce->setMetaData(getMetadata());
    }

    // 设置TrackSource
    void setTrackSource(const std::weak_ptr<TrackSource> &track_src){
        _mediaSouce->setTrackSource(track_src);
    }
private:
    RtmpMediaSource::Ptr _mediaSouce;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_RTMPMEDIASOURCEMUXER_H
