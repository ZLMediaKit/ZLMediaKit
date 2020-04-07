/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
