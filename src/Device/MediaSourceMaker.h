//
// Created by xzl on 2018/10/24.
//

#ifndef ZLMEDIAKIT_MEDIASOURCEMAKER_H
#define ZLMEDIAKIT_MEDIASOURCEMAKER_H

#include "Player/Track.h"
#include "Rtsp/RtspMediaSource.h"
#include "Rtmp/RtmpMediaSource.h"

namespace mediakit {

class MediaSourceMaker {
public:
    MediaSourceMaker() {}
    virtual ~MediaSourceMaker() {}
private:
    RtspMediaSource::Ptr _rtspSrc;
    RtmpMediaSource::Ptr _rtmpSrc;
};

} //namespace mediakit
#endif //ZLMEDIAKIT_MEDIASOURCEMAKER_H
