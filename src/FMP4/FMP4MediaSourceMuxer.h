/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FMP4MEDIASOURCEMUXER_H
#define ZLMEDIAKIT_FMP4MEDIASOURCEMUXER_H

#if defined(ENABLE_MP4)

#include "FMP4MediaSource.h"
#include "Record/MP4Muxer.h"

namespace mediakit {

class FMP4MediaSourceMuxer : public MP4MuxerMemory, public MediaSourceEventInterceptor,
                             public std::enable_shared_from_this<FMP4MediaSourceMuxer> {
public:
    using Ptr = std::shared_ptr<FMP4MediaSourceMuxer>;

    FMP4MediaSourceMuxer(const std::string &vhost,
                         const std::string &app,
                         const std::string &stream_id) {
        _media_src = std::make_shared<FMP4MediaSource>(vhost, app, stream_id);
    }

    ~FMP4MediaSourceMuxer() override = default;

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        setDelegate(listener);
        _media_src->setListener(shared_from_this());
    }

    int readerCount() const{
        return _media_src->readerCount();
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        GET_CONFIG(bool, fmp4_demand, General::kFMP4Demand);
        _enabled = fmp4_demand ? size : true;
        if (!size && fmp4_demand) {
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        GET_CONFIG(bool, fmp4_demand, General::kFMP4Demand);
        if (_clear_cache && fmp4_demand) {
            _clear_cache = false;
            _media_src->clearCache();
        }
        if (_enabled || !fmp4_demand) {
            return MP4MuxerMemory::inputFrame(frame);
        }
        return false;
    }

    bool isEnabled() {
        GET_CONFIG(bool, fmp4_demand, General::kFMP4Demand);
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return fmp4_demand ? (_clear_cache ? true : _enabled) : true;
    }

    void onAllTrackReady() {
        _media_src->setInitSegment(getInitSegment());
    }

protected:
    void onSegmentData(std::string string, uint64_t stamp, bool key_frame) override {
        if (string.empty()) {
            return;
        }
        FMP4Packet::Ptr packet = std::make_shared<FMP4Packet>(std::move(string));
        packet->time_stamp = stamp;
        _media_src->onWrite(std::move(packet), key_frame);
    }

private:
    bool _enabled = true;
    bool _clear_cache = false;
    FMP4MediaSource::Ptr _media_src;
};

}//namespace mediakit

#endif// defined(ENABLE_MP4)
#endif //ZLMEDIAKIT_FMP4MEDIASOURCEMUXER_H
