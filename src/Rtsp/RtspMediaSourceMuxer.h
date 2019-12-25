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

#ifndef ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H

#include "RtspMuxer.h"
#include "Rtsp/RtspMediaSource.h"

namespace mediakit {

class RtspMediaSourceMuxer : public RtspMuxer {
public:
    typedef std::shared_ptr<RtspMediaSourceMuxer> Ptr;

    RtspMediaSourceMuxer(const string &vhost,
                         const string &strApp,
                         const string &strId,
                         const TitleSdp::Ptr &title = nullptr) : RtspMuxer(title){
        _mediaSouce = std::make_shared<RtspMediaSource>(vhost,strApp,strId);
        getRtpRing()->setDelegate(_mediaSouce);
    }
    virtual ~RtspMediaSourceMuxer(){}

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        _mediaSouce->setListener(listener);
    }

    int readerCount() const{
        return _mediaSouce->readerCount();
    }

    void setTimeStamp(uint32_t stamp){
        _mediaSouce->setTimeStamp(stamp);
    }

    void onAllTrackReady(){
        _mediaSouce->setSdp(getSdp());
    }

    // 设置TrackSource
    void setTrackSource(const std::weak_ptr<TrackSource> &track_src){
        _mediaSouce->setTrackSource(track_src);
    }
private:
    RtspMediaSource::Ptr _mediaSouce;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H
