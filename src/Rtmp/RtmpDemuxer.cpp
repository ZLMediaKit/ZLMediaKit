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

#include "RtmpDemuxer.h"
#include "Extension/Factory.h"

namespace mediakit {

void RtmpDemuxer::loadMetaData(const AMFValue &val){
    try {
        makeVideoTrack(val["videocodecid"]);
        makeAudioTrack(val["audiocodecid"]);
        val.object_for_each([&](const string &key, const AMFValue &val) {
            if (key == "duration") {
                _fDuration = val.as_number();
                return;
            }
        });
    }catch (std::exception &ex){
        WarnL << ex.what();
    }
}

bool RtmpDemuxer::inputRtmp(const RtmpPacket::Ptr &pkt) {
    switch (pkt->typeId) {
        case MSG_VIDEO: {
            if(!_tryedGetVideoTrack){
                _tryedGetVideoTrack = true;
                auto codec = AMFValue(pkt->getMediaType());
                makeVideoTrack(codec);
            }
            if(_videoRtmpDecoder){
                return _videoRtmpDecoder->inputRtmp(pkt, true);
            }
            return false;
        }

        case MSG_AUDIO: {
            if(!_tryedGetAudioTrack) {
                _tryedGetAudioTrack = true;
                auto codec = AMFValue(pkt->getMediaType());
                makeAudioTrack(codec);
            }
            if(_audioRtmpDecoder){
                _audioRtmpDecoder->inputRtmp(pkt, false);
                return false;
            }
            return false;
        }
        default:
            return false;
    }
}

void RtmpDemuxer::makeVideoTrack(const AMFValue &videoCodec) {
    //生成Track对象
    _videoTrack = dynamic_pointer_cast<VideoTrack>(Factory::getTrackByAmf(videoCodec));
    if (_videoTrack) {
        //生成rtmpCodec对象以便解码rtmp
        _videoRtmpDecoder = Factory::getRtmpCodecByTrack(_videoTrack);
        if (_videoRtmpDecoder) {
            //设置rtmp解码器代理，生成的frame写入该Track
            _videoRtmpDecoder->addDelegate(_videoTrack);
            onAddTrack(_videoTrack);
        } else {
            //找不到相应的rtmp解码器，该track无效
            _videoTrack.reset();
        }
    }
}

void RtmpDemuxer::makeAudioTrack(const AMFValue &audioCodec) {
    //生成Track对象
    _audioTrack = dynamic_pointer_cast<AudioTrack>(Factory::getTrackByAmf(audioCodec));
    if (_audioTrack) {
        //生成rtmpCodec对象以便解码rtmp
        _audioRtmpDecoder = Factory::getRtmpCodecByTrack(_audioTrack);
        if (_audioRtmpDecoder) {
            //设置rtmp解码器代理，生成的frame写入该Track
            _audioRtmpDecoder->addDelegate(_audioTrack);
            onAddTrack(_audioTrack);
        } else {
            //找不到相应的rtmp解码器，该track无效
            _audioTrack.reset();
        }
    }
}


} /* namespace mediakit */
