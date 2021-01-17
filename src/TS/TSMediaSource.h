/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TSMEDIASOURCE_H
#define ZLMEDIAKIT_TSMEDIASOURCE_H

#include "Common/MediaSource.h"
using namespace toolkit;
#define TS_GOP_SIZE 512

namespace mediakit {

//TS直播数据包
class TSPacket : public BufferRaw{
public:
    using Ptr = std::shared_ptr<TSPacket>;

    template<typename ...ARGS>
    TSPacket(ARGS && ...args) : BufferRaw(std::forward<ARGS>(args)...) {};
    ~TSPacket() override = default;

public:
    uint32_t time_stamp = 0;
};

//TS直播源
class TSMediaSource : public MediaSource, public RingDelegate<TSPacket::Ptr>, public PacketCache<TSPacket>{
public:
    using PoolType = ResourcePool<TSPacket>;
    using Ptr = std::shared_ptr<TSMediaSource>;
    using RingDataType = std::shared_ptr<List<TSPacket::Ptr> >;
    using RingType = RingBuffer<RingDataType>;

    TSMediaSource(const string &vhost,
                  const string &app,
                  const string &stream_id,
                  int ring_size = TS_GOP_SIZE) : MediaSource(TS_SCHEMA, vhost, app, stream_id), _ring_size(ring_size) {}

    ~TSMediaSource() override = default;

    /**
     * 获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const {
        return _ring;
    }

    /**
     * 获取播放器个数
     */
    int readerCount() override {
        return _ring ? _ring->readerCount() : 0;
    }

    /**
     * 输入TS包
     * @param packet TS包
     * @param key 是否为关键帧第一个包
     */
    void onWrite(TSPacket::Ptr packet, bool key) override {
        _speed[TrackVideo] += packet->size();
        if (!_ring) {
            createRing();
        }
        if (key) {
            _have_video = true;
        }
        auto stamp = packet->time_stamp;
        PacketCache<TSPacket>::inputPacket(stamp, true, std::move(packet), key);
    }

    /**
     * 情况GOP缓存
     */
    void clearCache() override {
        PacketCache<TSPacket>::clearCache();
        _ring->clearCache();
    }

private:
    void createRing(){
        weak_ptr<TSMediaSource> weak_self = dynamic_pointer_cast<TSMediaSource>(shared_from_this());
        _ring = std::make_shared<RingType>(_ring_size, [weak_self](int size) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->onReaderChanged(size);
        });
        onReaderChanged(0);
        //注册媒体源
        regist();
    }

    /**
     * 合并写回调
     * @param packet_list 合并写缓存列队
     * @param key_pos 是否包含关键帧
     */
    void onFlush(std::shared_ptr<List<TSPacket::Ptr> > packet_list, bool key_pos) override {
        //如果不存在视频，那么就没有存在GOP缓存的意义，所以确保一直清空GOP缓存
        _ring->write(std::move(packet_list), _have_video ? key_pos : true);
    }

private:
    bool _have_video = false;
    int _ring_size;
    RingType::Ptr _ring;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_TSMEDIASOURCE_H
