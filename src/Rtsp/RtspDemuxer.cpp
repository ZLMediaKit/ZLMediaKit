/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cctype>
#include <algorithm>
#include "RtspDemuxer.h"
#include "Util/base64.h"
#include "Extension/Factory.h"

using namespace std;

namespace mediakit {

void RtspDemuxer::loadSdp(const string &sdp){
    loadSdp(SdpParser(sdp));
}

void RtspDemuxer::loadSdp(const SdpParser &attr) {
    auto tracks = attr.getAvailableTrack();
    for (auto &track : tracks){
        switch (track->_type) {
            case TrackVideo: {
                makeVideoTrack(track);
            }
                break;
            case TrackAudio: {
                makeAudioTrack(track);
            }
                break;
            default:
                break;
        }
    }
    auto titleTrack = attr.getTrack(TrackTitle);
    if(titleTrack){
        _fDuration = titleTrack->_duration;
    }
}
bool RtspDemuxer::inputRtp(const RtpPacket::Ptr & rtp) {
    switch (rtp->type) {
    case TrackVideo:{
        if(_videoRtpDecoder){
            return _videoRtpDecoder->inputRtp(rtp, true);
        }
        return false;
    }
    case TrackAudio:{
        if(_audioRtpDecoder){
            _audioRtpDecoder->inputRtp(rtp, false);
            return false;
        }
        return false;
    }
    default:
        return false;
    }
}


void RtspDemuxer::makeAudioTrack(const SdpTrack::Ptr &audio) {
    //生成Track对象
    _audioTrack = dynamic_pointer_cast<AudioTrack>(Factory::getTrackBySdp(audio));
    if(_audioTrack){
        //生成RtpCodec对象以便解码rtp
        _audioRtpDecoder = Factory::getRtpDecoderByTrack(_audioTrack);
        if(_audioRtpDecoder){
            //设置rtp解码器代理，生成的frame写入该Track
            _audioRtpDecoder->addDelegate(_audioTrack);
            onAddTrack(_audioTrack);
        } else{
            //找不到相应的rtp解码器，该track无效
            _audioTrack.reset();
        }
    }
}

void RtspDemuxer::makeVideoTrack(const SdpTrack::Ptr &video) {
    //生成Track对象
    _videoTrack = dynamic_pointer_cast<VideoTrack>(Factory::getTrackBySdp(video));
    if(_videoTrack){
        //生成RtpCodec对象以便解码rtp
        _videoRtpDecoder = Factory::getRtpDecoderByTrack(_videoTrack);
        if(_videoRtpDecoder){
            //设置rtp解码器代理，生成的frame写入该Track
            _videoRtpDecoder->addDelegate(_videoTrack);
            onAddTrack(_videoTrack);
        }else{
            //找不到相应的rtp解码器，该track无效
            _videoTrack.reset();
        }
    }
}

} /* namespace mediakit */
