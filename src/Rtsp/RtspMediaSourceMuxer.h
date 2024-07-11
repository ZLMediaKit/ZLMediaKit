﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H

#include "RtspMuxer.h"
#include "Rtsp/RtspMediaSource.h"

namespace mediakit {

class RtspMediaSourceMuxer final : public RtspMuxer, public MediaSourceEventInterceptor,
                                   public std::enable_shared_from_this<RtspMediaSourceMuxer> {
public:
    using Ptr = std::shared_ptr<RtspMediaSourceMuxer>;

    RtspMediaSourceMuxer(const MediaTuple& tuple,
                         const ProtocolOption &option,
                         const TitleSdp::Ptr &title = nullptr) : RtspMuxer(title) {
        _option = option;
        _media_src = std::make_shared<RtspMediaSource>(tuple);
        getRtpRing()->setDelegate(_media_src);
    }

    ~RtspMediaSourceMuxer() override {
        try {
            RtspMuxer::flush();
        } catch (std::exception &ex) {
            WarnL << ex.what();
        }
    }

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

    void addTrackCompleted() override {
        RtspMuxer::addTrackCompleted();
        _media_src->setSdp(getSdp());
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        _enabled = _option.rtsp_demand ? size : true;
        if (!size && _option.rtsp_demand) {
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        if (_clear_cache && _option.rtsp_demand) {
            _clear_cache = false;
            _media_src->clearCache();
        }
        if (_enabled || !_option.rtsp_demand) {
            return RtspMuxer::inputFrame(frame);
        }
        return false;
    }

    bool isEnabled() {
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return _option.rtsp_demand ? (_clear_cache ? true : _enabled) : true;
    }

private:
    bool _enabled = true;
    bool _clear_cache = false;
    ProtocolOption _option;
    RtspMediaSource::Ptr _media_src;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_RTSPMEDIASOURCEMUXER_H
