/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
        makeConfigPacket();
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
