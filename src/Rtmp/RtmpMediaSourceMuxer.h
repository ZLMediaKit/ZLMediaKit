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

class RtmpMediaSourceMuxer final : public RtmpMuxer, public MediaSourceEventInterceptor,
                                   public std::enable_shared_from_this<RtmpMediaSourceMuxer> {
public:
    using Ptr = std::shared_ptr<RtmpMediaSourceMuxer>;

    RtmpMediaSourceMuxer(const MediaTuple& tuple,
                         const ProtocolOption &option,
                         const TitleMeta::Ptr &title = nullptr) : RtmpMuxer(title) {
        _option = option;
        _media_src = std::make_shared<RtmpMediaSource>(tuple);
        getRtmpRing()->setDelegate(_media_src);
    }

    ~RtmpMediaSourceMuxer() override { RtmpMuxer::flush(); }

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
        _enabled = _option.rtmp_demand ? size : true;
        if (!size && _option.rtmp_demand) {
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        if (_clear_cache && _option.rtmp_demand) {
            _clear_cache = false;
            _media_src->clearCache();
        }
        if (_enabled || !_option.rtmp_demand) {
            return RtmpMuxer::inputFrame(frame);
        }
        return false;
    }

    bool isEnabled() {
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return _option.rtmp_demand ? (_clear_cache ? true : _enabled) : true;
    }

private:
    bool _enabled = true;
    bool _clear_cache = false;
    ProtocolOption _option;
    RtmpMediaSource::Ptr _media_src;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_RTMPMEDIASOURCEMUXER_H
