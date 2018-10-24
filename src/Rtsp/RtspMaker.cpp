//
// Created by xzl on 2018/10/24.
//

#include "RtspMaker.h"
#include "Common/Factory.h"

namespace mediakit {

void RtspMaker::addTrack(const Track::Ptr &track, uint32_t ssrc, int mtu) {
    if (track->getCodecId() == CodecInvalid) {
        addTrack(std::make_shared<TitleSdp>(), ssrc, mtu);
    } else {
        Sdp::Ptr sdp = Factory::getSdpByTrack(track);
        if (sdp) {
            addTrack(sdp, ssrc, mtu);
        }
    }
}

} /* namespace mediakit */