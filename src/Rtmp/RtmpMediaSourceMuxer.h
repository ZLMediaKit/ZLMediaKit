/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

class RtmpMediaSourceMuxer : public RtmpMuxer, public MediaSourceEventInterceptor,
                             public std::enable_shared_from_this<RtmpMediaSourceMuxer> {
public:
    typedef std::shared_ptr<RtmpMediaSourceMuxer> Ptr;

    RtmpMediaSourceMuxer(const std::string &vhost,
                         const std::string &strApp,
                         const std::string &strId,
                         const TitleMeta::Ptr &title = nullptr) : RtmpMuxer(title){
        _media_src = std::make_shared<RtmpMediaSource>(vhost, strApp, strId);
        getRtmpRing()->setDelegate(_media_src);
    }

    ~RtmpMediaSourceMuxer() override{}

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        setDelegate(listener);
        _media_src->setListener(shared_from_this());
    }

    void setTimeStamp(uint32_t stamp){
        _media_src->setTimeStamp(stamp);
    }

    int readerCount() const{
        return _media_src->readerCount();
    }

    void onAllTrackReady(){
        makeConfigPacket();
        _media_src->setMetaData(getMetadata());
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        GET_CONFIG(bool, rtmp_demand, General::kRtmpDemand);
        _enabled = rtmp_demand ? size : true;
        if (!size && rtmp_demand) {
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        GET_CONFIG(bool, rtmp_demand, General::kRtmpDemand);
        if (_clear_cache && rtmp_demand) {
            _clear_cache = false;
            _media_src->clearCache();
        }
        if (_enabled || !rtmp_demand) {
            return RtmpMuxer::inputFrame(frame);
        }
        return false;
    }

    bool isEnabled() {
        GET_CONFIG(bool, rtmp_demand, General::kRtmpDemand);
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return rtmp_demand ? (_clear_cache ? true : _enabled) : true;
    }

private:
    bool _enabled = true;
    bool _clear_cache = false;
    RtmpMediaSource::Ptr _media_src;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_RTMPMEDIASOURCEMUXER_H
