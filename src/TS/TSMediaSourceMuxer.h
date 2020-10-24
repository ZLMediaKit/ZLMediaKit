/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TSMEDIASOURCEMUXER_H
#define ZLMEDIAKIT_TSMEDIASOURCEMUXER_H

#include "TSMediaSource.h"
#include "Record/TsMuxer.h"

namespace mediakit {

class TSMediaSourceMuxer : public TsMuxer, public MediaSourceEventInterceptor,
                           public std::enable_shared_from_this<TSMediaSourceMuxer> {
public:
    using Ptr = std::shared_ptr<TSMediaSourceMuxer>;

    TSMediaSourceMuxer(const string &vhost,
                       const string &app,
                       const string &stream_id) {
        _media_src = std::make_shared<TSMediaSource>(vhost, app, stream_id);
        _pool.setSize(256);
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
        _enabled = size;
        if (!size) {
            _clear_cache = true;
        }
        MediaSourceEventInterceptor::onReaderChanged(sender, size);
    }

    void inputFrame(const Frame::Ptr &frame) override {
        if (_clear_cache) {
            _clear_cache = false;
            _media_src->clearCache();
        }
        if (_enabled) {
            TsMuxer::inputFrame(frame);
        }
    }

    bool isEnabled() {
        //缓存尚未清空时，还允许触发inputFrame函数，以便及时清空缓存
        return _clear_cache ? true : _enabled;
    }

protected:
    void onTs(const void *data, int len,uint32_t timestamp,bool is_idr_fast_packet) override{
        if(!data || !len){
            return;
        }
        TSPacket::Ptr packet = _pool.obtain();
        packet->assign((char *) data, len);
        packet->time_stamp = timestamp;
        _media_src->onWrite(std::move(packet), is_idr_fast_packet);
    }

private:
    bool _enabled = true;
    bool _clear_cache = false;
    TSMediaSource::PoolType _pool;
    TSMediaSource::Ptr _media_src;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_TSMEDIASOURCEMUXER_H
