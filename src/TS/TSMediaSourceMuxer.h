/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TSMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_TSMEDIASOURCEMUXER_H

#include "TSMediaSource.h"
#include "Record/MPEG.h"

namespace mediakit {

class TSMediaSourceMuxer : public MpegMuxer, public MediaSourceEventInterceptor,
                           public std::enable_shared_from_this<TSMediaSourceMuxer> {
public:
    using Ptr = std::shared_ptr<TSMediaSourceMuxer>;

    TSMediaSourceMuxer(const std::string &vhost,
                       const std::string &app,
                       const std::string &stream_id) : MpegMuxer(false) {
        _media_src = std::make_shared<TSMediaSource>(vhost, app, stream_id);
    }

    ~TSMediaSourceMuxer() override = default;

    void setListener(const std::weak_ptr<MediaSourceEvent> &listener){
        setDelegate(listener);
        _media_src->setListener(shared_from_this());
    }

    int readerCount() const{
        return _media_src->readerCount();
    }

    void onReaderChanged(MediaSource &sender, int size) override {
        GET_CONFIG(bool, ts_demand, General::kTSDemand);
        _enabled = ts_demand ? size : true;
        if (!size && ts_demand) {
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        GET_CONFIG(bool, ts_demand, General::kTSDemand);
        if (_clear_cache && ts_demand) {
            _clear_cache = false;
            _media_src->clearCache();
        }
        if (_enabled || !ts_demand) {
            return MpegMuxer::inputFrame(frame);
        }
        return false;
    }

    bool isEnabled() {
        GET_CONFIG(bool, ts_demand, General::kTSDemand);
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return ts_demand ? (_clear_cache ? true : _enabled) : true;
    }

protected:
    void onWrite(std::shared_ptr<toolkit::Buffer> buffer, uint64_t timestamp, bool key_pos) override {
        if (!buffer) {
            return;
        }
        auto packet = std::make_shared<TSPacket>(std::move(buffer));
        packet->time_stamp = timestamp;
        _media_src->onWrite(std::move(packet), key_pos);
    }

private:
    bool _enabled = true;
    bool _clear_cache = false;
    TSMediaSource::Ptr _media_src;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_TSMEDIASOURCEMUXER_H
