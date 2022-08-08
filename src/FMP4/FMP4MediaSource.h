/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FMP4MEDIASOURCE_H
#define ZLMEDIAKIT_FMP4MEDIASOURCE_H

#include "Common/MediaSource.h"

#define FMP4_GOP_SIZE 512

namespace mediakit {

//FMP4直播数据包
class FMP4Packet : public toolkit::BufferString{
public:
    using Ptr = std::shared_ptr<FMP4Packet>;

    template<typename ...ARGS>
    FMP4Packet(ARGS && ...args) : toolkit::BufferString(std::forward<ARGS>(args)...) {};
    ~FMP4Packet() override = default;

public:
    uint64_t time_stamp = 0;
};

//FMP4直播源
class FMP4MediaSource : public MediaSource, public toolkit::RingDelegate<FMP4Packet::Ptr>, private PacketCache<FMP4Packet>{
public:
    using Ptr = std::shared_ptr<FMP4MediaSource>;
    using RingDataType = std::shared_ptr<toolkit::List<FMP4Packet::Ptr> >;
    using RingType = toolkit::RingBuffer<RingDataType>;

    FMP4MediaSource(const std::string &vhost,
                    const std::string &app,
                    const std::string &stream_id,
                    int ring_size = FMP4_GOP_SIZE) : MediaSource(FMP4_SCHEMA, vhost, app, stream_id), _ring_size(ring_size) {}

    ~FMP4MediaSource() override = default;

    /**
     * 获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const {
        return _ring;
    }

    /**
     * 获取fmp4 init segment
     */
    const std::string &getInitSegment() const{
        return _init_segment;
    }

    /**
     * 设置fmp4 init segment
     * @param str init segment
     */
    void setInitSegment(std::string str) {
        _init_segment = std::move(str);
        createRing();
    }

    /**
     * 获取播放器个数
     */
    int readerCount() override {
        return _ring ? _ring->readerCount() : 0;
    }

    /**
     * 输入FMP4包
     * @param packet FMP4包
     * @param key 是否为关键帧第一个包
     */
    void onWrite(FMP4Packet::Ptr packet, bool key) override {
        if (!_ring) {
            createRing();
        }
        if (key) {
            _have_video = true;
        }
        _speed[TrackVideo] += packet->size();
        auto stamp = packet->time_stamp;
        PacketCache<FMP4Packet>::inputPacket(stamp, true, std::move(packet), key);
    }

    /**
     * 情况GOP缓存
     */
    void clearCache() override {
        PacketCache<FMP4Packet>::clearCache();
        _ring->clearCache();
    }

private:
    void createRing(){
        std::weak_ptr<FMP4MediaSource> weak_self = std::dynamic_pointer_cast<FMP4MediaSource>(shared_from_this());
        _ring = std::make_shared<RingType>(_ring_size, [weak_self](int size) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->onReaderChanged(size);
        });
        onReaderChanged(0);
        if (!_init_segment.empty()) {
            regist();
        }
    }

    /**
     * 合并写回调
     * @param packet_list 合并写缓存列队
     * @param key_pos 是否包含关键帧
     */
    void onFlush(std::shared_ptr<toolkit::List<FMP4Packet::Ptr> > packet_list, bool key_pos) override {
        //如果不存在视频，那么就没有存在GOP缓存的意义，所以确保一直清空GOP缓存
        _ring->write(std::move(packet_list), _have_video ? key_pos : true);
    }

private:
    bool _have_video = false;
    int _ring_size;
    std::string _init_segment;
    RingType::Ptr _ring;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_FMP4MEDIASOURCE_H
