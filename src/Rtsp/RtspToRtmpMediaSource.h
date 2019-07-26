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

#ifndef SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_
#define SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_

#include "Rtmp/amf.h"
#include "RtspMediaSource.h"
#include "RtspDemuxer.h"
#include "Common/MultiMediaSourceMuxer.h"

using namespace toolkit;

namespace mediakit {

class RtspToRtmpMediaSource : public RtspMediaSource {
public:
    typedef std::shared_ptr<RtspToRtmpMediaSource> Ptr;

    RtspToRtmpMediaSource(const string &vhost,
                          const string &app,
                          const string &id,
                          bool bEnableHls = true,
                          bool bEnableMp4 = false,
                          int ringSize = 0) : RtspMediaSource(vhost, app, id,ringSize) {
        _bEnableHls = bEnableHls;
        _bEnableMp4 = bEnableMp4;
    }

    virtual ~RtspToRtmpMediaSource() {}

    virtual void onGetSDP(const string &strSdp) override {
        _demuxer = std::make_shared<RtspDemuxer>(strSdp);
        RtspMediaSource::onGetSDP(strSdp);
    }

    virtual void onWrite(const RtpPacket::Ptr &rtp, bool bKeyPos) override {
        if (_demuxer) {
            bKeyPos = _demuxer->inputRtp(rtp);
            if (!_muxer && _demuxer->isInited(2000)) {
                _muxer = std::make_shared<MultiMediaSourceMuxer>(getVhost(),
                                                                 getApp(),
                                                                 getId(),
                                                                 _demuxer->getDuration(),
                                                                 false,//不重复生成rtsp
                                                                 true,//转rtmp
                                                                 _bEnableHls,
                                                                 _bEnableMp4);
                for (auto &track : _demuxer->getTracks(false)) {
                    _muxer->addTrack(track);
                    track->addDelegate(_muxer);
                }
                _muxer->setListener(_listener);
            }
        }
        RtspMediaSource::onWrite(rtp, bKeyPos);
    }

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override {
        RtspMediaSource::setListener(listener);
        if(_muxer){
            _muxer->setListener(listener);
        }
    }
    int readerCount() override {
        return RtspMediaSource::readerCount() + (_muxer ? _muxer->readerCount() : 0);
    }
private:
    RtspDemuxer::Ptr _demuxer;
    MultiMediaSourceMuxer::Ptr _muxer;
    bool _bEnableHls;
    bool _bEnableMp4;
};

} /* namespace mediakit */

#endif /* SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_ */
