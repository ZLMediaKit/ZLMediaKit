/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTSP_RTSPMEDIASOURCE_H_
#define SRC_RTSP_RTSPMEDIASOURCE_H_

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include "Common/MediaSource.h"
#include "Common/PacketCache.h"
#include "Util/RingBuffer.h"

#define RTP_GOP_SIZE 512

namespace mediakit {

/**
 * rtsp媒体源的数据抽象
 * rtsp有关键的两要素，分别是sdp、rtp包
 * 只要生成了这两要素，那么要实现rtsp推流、rtsp服务器就很简单了
 * rtsp推拉流协议中，先传递sdp，然后再协商传输方式(tcp/udp/组播)，最后一直传递rtp
 */
class RtspMediaSource : public MediaSource, public toolkit::RingDelegate<RtpPacket::Ptr>, private PacketCache<RtpPacket> {
public:
    using Ptr = std::shared_ptr<RtspMediaSource>;
    using RingDataType = std::shared_ptr<toolkit::List<RtpPacket::Ptr> >;
    using RingType = toolkit::RingBuffer<RingDataType>;

    /**
     * 构造函数
     * @param vhost 虚拟主机名
     * @param app 应用名
     * @param stream_id 流id
     * @param ring_size 可以设置固定的环形缓冲大小，0则自适应
     */
    RtspMediaSource(const MediaTuple& tuple, int ring_size = RTP_GOP_SIZE): MediaSource(RTSP_SCHEMA, tuple), _ring_size(ring_size) {}

    ~RtspMediaSource() override { flush(); }

    /**
     * 获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const {
        return _ring;
    }

    void getPlayerList(const std::function<void(const std::list<toolkit::Any> &info_list)> &cb,
                       const std::function<toolkit::Any(toolkit::Any &&info)> &on_change) override {
        assert(_ring);
        _ring->getInfoList(cb, on_change);
    }

    bool broadcastMessage(const toolkit::Any &data) override {
        assert(_ring);
        _ring->sendMessage(data);
        return true;
    }

    /**
     * 获取播放器个数
     */
    int readerCount() override {
        return _ring ? _ring->readerCount() : 0;
    }

    /**
     * 获取该源的sdp
     */
    const std::string &getSdp() const {
        return _sdp;
    }

    virtual RtspMediaSource::Ptr clone(const std::string& stream) {
        return nullptr;
    }

    /**
     * 获取相应轨道的ssrc
     */
    virtual uint32_t getSsrc(TrackType trackType) {
        assert(trackType >= 0 && trackType < TrackMax);
        auto &track = _tracks[trackType];
        if (!track) {
            return 0;
        }
        return track->_ssrc;
    }

    /**
     * 获取相应轨道的seqence
     */
    virtual uint16_t getSeqence(TrackType trackType) {
        assert(trackType >= 0 && trackType < TrackMax);
        auto &track = _tracks[trackType];
        if (!track) {
            return 0;
        }
        return track->_seq;
    }

    /**
     * 获取相应轨道的时间戳，单位毫秒
     */
    uint32_t getTimeStamp(TrackType trackType) override;

    /**
     * 更新时间戳
     */
    void setTimeStamp(uint32_t stamp) override;

    /**
     * 设置sdp
     */
    virtual void setSdp(const std::string &sdp);

    /**
     * 输入rtp
     * @param rtp rtp包
     * @param keyPos 该包是否为关键帧的第一个包
     */
    void onWrite(RtpPacket::Ptr rtp, bool keyPos) override;

    void clearCache() override{
        PacketCache<RtpPacket>::clearCache();
        _ring->clearCache();
    }

private:
    /**
     * 批量flush rtp包时触发该函数
     * @param rtp_list rtp包列表
     * @param key_pos 是否包含关键帧
     */
    void onFlush(std::shared_ptr<toolkit::List<RtpPacket::Ptr> > rtp_list, bool key_pos) override {
        //如果不存在视频，那么就没有存在GOP缓存的意义，所以is_key一直为true确保一直清空GOP缓存
        _ring->write(std::move(rtp_list), _have_video ? key_pos : true);
    }

private:
    bool _have_video = false;
    int _ring_size;
    std::string _sdp;
    RingType::Ptr _ring;
    SdpTrack::Ptr _tracks[TrackMax];
};

} /* namespace mediakit */

#endif /* SRC_RTSP_RTSPMEDIASOURCE_H_ */
