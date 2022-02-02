/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

class RtspMediaSourceMuxer : public RtspMuxer, public MediaSourceEventInterceptor,
                             public std::enable_shared_from_this<RtspMediaSourceMuxer> {
public:
    typedef std::shared_ptr<RtspMediaSourceMuxer> Ptr;

    RtspMediaSourceMuxer(const std::string &vhost,
                         const std::string &strApp,
                         const std::string &strId,
                         const TitleSdp::Ptr &title = nullptr) : RtspMuxer(title){
        _media_src = std::make_shared<RtspMediaSource>(vhost,strApp,strId);
        getRtpRing()->setDelegate(_media_src);
    }

    ~RtspMediaSourceMuxer() override{}

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        setDelegate(listener);
        _media_src->setListener(shared_from_this());
    }

    int readerCount() const{
        return _media_src->readerCount();
    }

    void setTimeStamp(uint32_t stamp){
        _media_src->setTimeStamp(stamp);
    }

    void onAllTrackReady(){
        _media_src->setSdp(getSdp());
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        GET_CONFIG(bool, rtsp_demand, General::kRtspDemand);
        _enabled = rtsp_demand ? size : true;
        if (!size && rtsp_demand) {
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        GET_CONFIG(bool, rtsp_demand, General::kRtspDemand);
        if (_clear_cache && rtsp_demand) {
            _clear_cache = false;
            _media_src->clearCache();
        }
        if (_enabled || !rtsp_demand) {
            return RtspMuxer::inputFrame(frame);
        }
        return false;
    }

    bool isEnabled() {
        GET_CONFIG(bool, rtsp_demand, General::kRtspDemand);
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return rtsp_demand ? (_clear_cache ? true : _enabled) : true;
    }

private:
    bool _enabled = true;
    bool _clear_cache = false;
    RtspMediaSource::Ptr _media_src;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H
