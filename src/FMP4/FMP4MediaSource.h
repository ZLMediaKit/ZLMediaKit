/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FMP4MEDIASOURCE_H
#define ZLMEDIAKIT_FMP4MEDIASOURCE_H

#include "Common/MediaSource.h"
#include "Common/PacketCache.h"
#include "Util/RingBuffer.h"

#define FMP4_GOP_SIZE 512

namespace mediakit {

// FMP4直播数据包  [AUTO-TRANSLATED:64f8a1d1]
// FMP4 Live Data Packet
class FMP4Packet : public toolkit::BufferString{
public:
    using Ptr = std::shared_ptr<FMP4Packet>;

    template<typename ...ARGS>
    FMP4Packet(ARGS && ...args) : toolkit::BufferString(std::forward<ARGS>(args)...) {};

public:
    uint64_t time_stamp = 0;
};

// FMP4直播源  [AUTO-TRANSLATED:15c43604]
// FMP4 Live Source
class FMP4MediaSource final : public MediaSource, public toolkit::RingDelegate<FMP4Packet::Ptr>, private PacketCache<FMP4Packet>{
public:
    using Ptr = std::shared_ptr<FMP4MediaSource>;
    using RingDataType = std::shared_ptr<toolkit::List<FMP4Packet::Ptr> >;
    using RingType = toolkit::RingBuffer<RingDataType>;

    FMP4MediaSource(const MediaTuple& tuple,
                    int ring_size = FMP4_GOP_SIZE) : MediaSource(FMP4_SCHEMA, tuple), _ring_size(ring_size) {}

    ~FMP4MediaSource() override {
        try {
            flush();
        } catch (std::exception &ex) {
            WarnL << ex.what();
        }
    }

    /**
     * 获取媒体源的环形缓冲
     * Get the circular buffer of the media source
     
     * [AUTO-TRANSLATED:91a762bc]
     */
    const RingType::Ptr &getRing() const {
        return _ring;
    }

    void getPlayerList(const std::function<void(const std::list<toolkit::Any> &info_list)> &cb,
                       const std::function<toolkit::Any(toolkit::Any &&info)> &on_change) override {
        _ring->getInfoList(cb, on_change);
    }

    /**
     * 获取fmp4 init segment
     * Get the fmp4 init segment
     
     * [AUTO-TRANSLATED:6c704ec9]
     */
    const std::string &getInitSegment() const{
        return _init_segment;
    }

    /**
     * 设置fmp4 init segment
     * @param str init segment
     * Set the fmp4 init segment
     * @param str init segment
     
     * [AUTO-TRANSLATED:3f41879f]
     */
    void setInitSegment(std::string str) {
        _init_segment = std::move(str);
        createRing();
    }

    /**
     * 获取播放器个数
     * Get the number of players
     
     * [AUTO-TRANSLATED:a451c846]
     */
    int readerCount() override {
        return _ring ? _ring->readerCount() : 0;
    }

    /**
     * 输入FMP4包
     * @param packet FMP4包
     * @param key 是否为关键帧第一个包
     * Input FMP4 packet
     * @param packet FMP4 packet
     * @param key Whether it is the first packet of the key frame
     
     * [AUTO-TRANSLATED:3b310b27]
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
     * Clear GOP cache
     
     * [AUTO-TRANSLATED:d863f8c9]
     */
    void clearCache() override {
        PacketCache<FMP4Packet>::clearCache();
        _ring->clearCache();
    }

private:
    void createRing(){
        std::weak_ptr<FMP4MediaSource> weak_self = std::static_pointer_cast<FMP4MediaSource>(shared_from_this());
        _ring = std::make_shared<RingType>(_ring_size, [weak_self](int size) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->onReaderChanged(size);
        });
        if (!_init_segment.empty()) {
            regist();
        }
    }

    /**
     * 合并写回调
     * @param packet_list 合并写缓存列队
     * @param key_pos 是否包含关键帧
     * Merge write callback
     * @param packet_list Merge write cache queue
     * @param key_pos Whether it contains a key frame
     
     * [AUTO-TRANSLATED:6e93913e]
     */
    void onFlush(std::shared_ptr<toolkit::List<FMP4Packet::Ptr> > packet_list, bool key_pos) override {
        // 如果不存在视频，那么就没有存在GOP缓存的意义，所以确保一直清空GOP缓存  [AUTO-TRANSLATED:66208f94]
        // If there is no video, then there is no meaning to the existence of GOP cache, so make sure to clear the GOP cache all the time
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
