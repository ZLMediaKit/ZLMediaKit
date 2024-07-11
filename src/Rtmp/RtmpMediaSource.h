﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RTMPMEDIASOURCE_H_
#define SRC_RTMP_RTMPMEDIASOURCE_H_

#include <mutex>
#include <memory>
#include <string>
#include <functional>
#include <unordered_map>
#include "amf.h"
#include "Rtmp.h"
#include "Common/MediaSource.h"
#include "Common/PacketCache.h"
#include "Util/RingBuffer.h"

#define RTMP_GOP_SIZE 512

namespace mediakit {

/**
 * rtmp媒体源的数据抽象
 * rtmp有关键的三要素，分别是metadata、config帧，普通帧
 * 其中metadata是非必须的，有些编码格式也没有config帧(比如MP3)
 * 只要生成了这三要素，那么要实现rtmp推流、rtmp服务器就很简单了
 * rtmp推拉流协议中，先传递metadata，然后传递config帧，然后一直传递普通帧
 */
class RtmpMediaSource : public MediaSource, public toolkit::RingDelegate<RtmpPacket::Ptr>, private PacketCache<RtmpPacket>{
public:
    using Ptr = std::shared_ptr<RtmpMediaSource>;
    using RingDataType = std::shared_ptr<toolkit::List<RtmpPacket::Ptr> >;
    using RingType = toolkit::RingBuffer<RingDataType>;

    /**
     * 构造函数
     * @param vhost 虚拟主机名
     * @param app 应用名
     * @param stream_id 流id
     * @param ring_size 可以设置固定的环形缓冲大小，0则自适应
     */
    RtmpMediaSource(const MediaTuple& tuple, int ring_size = RTMP_GOP_SIZE): MediaSource(RTMP_SCHEMA, tuple), _ring_size(ring_size) {}

    ~RtmpMediaSource() override {
        try {
            flush();
        } catch (std::exception &ex) {
            WarnL << ex.what();
        }
    }

    /**
     * 	获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const {
        return _ring;
    }

    void getPlayerList(const std::function<void(const std::list<toolkit::Any> &info_list)> &cb,
                       const std::function<toolkit::Any(toolkit::Any &&info)> &on_change) override {
        _ring->getInfoList(cb, on_change);
    }

    /**
     * 获取播放器个数
     * @return
     */
    int readerCount() override {
        return _ring ? _ring->readerCount() : 0;
    }

    /**
     * 获取metadata
     */
    template <typename FUNC>
    void getMetaData(const FUNC &func) const {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        if (_metadata) {
            func(_metadata);
        }
    }

    /**
     * 获取所有的config帧
     */
    template <typename FUNC>
    void getConfigFrame(const FUNC &func) {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        for (auto &pr : _config_frame_map) {
            func(pr.second);
        }
    }

    /**
     * 设置metadata
     */
    virtual void setMetaData(const AMFValue &metadata);

    /**
     * 输入rtmp包
     * @param pkt rtmp包
     */
    void onWrite(RtmpPacket::Ptr pkt, bool = true) override;

    /**
     * 获取当前时间戳
     */
    uint32_t getTimeStamp(TrackType trackType) override;

    void clearCache() override{
        PacketCache<RtmpPacket>::clearCache();
        _ring->clearCache();
    }

    bool haveVideo() const {
        return _have_video;
    }

    bool haveAudio() const {
        return _have_audio;
    }

private:
    /**
    * 批量flush rtmp包时触发该函数
    * @param rtmp_list rtmp包列表
    * @param key_pos 是否包含关键帧
    */
    void onFlush(std::shared_ptr<toolkit::List<RtmpPacket::Ptr> > rtmp_list, bool key_pos) override {
        //如果不存在视频，那么就没有存在GOP缓存的意义，所以is_key一直为true确保一直清空GOP缓存
        _ring->write(std::move(rtmp_list), _have_video ? key_pos : true);
    }

private:
    bool _have_video = false;
    bool _have_audio = false;
    int _ring_size;
    uint32_t _track_stamps[TrackMax] = {0};
    AMFValue _metadata;
    RingType::Ptr _ring;

    mutable std::recursive_mutex _mtx;
    std::unordered_map<int, RtmpPacket::Ptr> _config_frame_map;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPMEDIASOURCE_H_ */
