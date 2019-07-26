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
#include "MediaFile/MediaRecorder.h"
#include "Rtmp/RtmpMediaSource.h"
#include "RtspDemuxer.h"
#include "Rtmp/RtmpMediaSourceMuxer.h"

using namespace toolkit;

namespace mediakit {

class RtspToRtmpMediaSource : public RtspMediaSource {
public:
    typedef std::shared_ptr<RtspToRtmpMediaSource> Ptr;

    RtspToRtmpMediaSource(const string &vhost,
                          const string &app,
                          const string &id,
                          bool bEnableHls = true,
                          //chenxiaolei 修改为int, 录像最大录制天数,0就是不录
                          int bRecordMp4 = 0,
                          int ringSize = 0) : RtspMediaSource(vhost, app, id,ringSize) {
        _recorder = std::make_shared<MediaRecorder>(vhost, app, id, bEnableHls, bRecordMp4);
    }

    virtual ~RtspToRtmpMediaSource() {}

    virtual void onGetSDP(const string &strSdp) override {
        _rtspDemuxer = std::make_shared<RtspDemuxer>(strSdp);
        RtspMediaSource::onGetSDP(strSdp);
    }

    virtual void onWrite(const RtpPacket::Ptr &rtp, bool bKeyPos) override {
        if (_rtspDemuxer) {
            bKeyPos = _rtspDemuxer->inputRtp(rtp);
            if (!_rtmpMuxer && _rtspDemuxer->isInited(2000)) {
                _rtmpMuxer = std::make_shared<RtmpMediaSourceMuxer>(getVhost(),
                                                                    getApp(),
                                                                    getId(),
                                                                    std::make_shared<TitleMete>(_rtspDemuxer->getDuration()));
                for (auto &track : _rtspDemuxer->getTracks(false)) {
                    _rtmpMuxer->addTrack(track);
                    _recorder->addTrack(track);
                    track->addDelegate(_rtmpMuxer);
                    track->addDelegate(_recorder);
                }
                _rtmpMuxer->setListener(_listener);
            }
        }
        RtspMediaSource::onWrite(rtp, bKeyPos);
    }

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override {
        RtspMediaSource::setListener(listener);
        if(_rtmpMuxer){
            _rtmpMuxer->setListener(listener);
        }
    }
    int readerCount() override {
        return RtspMediaSource::readerCount() + (_rtmpMuxer ? _rtmpMuxer->readerCount() : 0);
    }
private:
    RtspDemuxer::Ptr _rtspDemuxer;
    RtmpMediaSourceMuxer::Ptr _rtmpMuxer;
    MediaRecorder::Ptr _recorder;
};

} /* namespace mediakit */

#endif /* SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_ */
