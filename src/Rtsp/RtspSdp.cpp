//
// Created by xzl on 2018/10/23.
//
#include "RtspSdp.h"

namespace ZL{
namespace Rtsp{


Sdp::Ptr Sdp::getSdpByTrack(const Track::Ptr &track) {
    switch (track->getCodecId()){
        case CodecH264:{
            H264Track::Ptr h264Track = dynamic_pointer_cast<H264Track>(track);
            if(!h264Track){
                return nullptr;
            }
            return std::make_shared<H264Sdp>(h264Track->getSps(),h264Track->getPps());
        }

        case CodecAAC:{
            AACTrack::Ptr aacTrack = dynamic_pointer_cast<AACTrack>(track);
            if(!aacTrack){
                return nullptr;
            }
            return std::make_shared<AACSdp>(aacTrack->getAacCfg(),aacTrack->getAudioSampleRate());
        }

        default:
            return nullptr;
    }
}




}
}

