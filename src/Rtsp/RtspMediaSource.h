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
#include <unordered_map>
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "RtpCodec.h"
#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Util/NoticeCenter.h"
#include "Thread/ThreadPool.h"

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
    using PoolType = toolkit::ResourcePool<RtpPacket>;
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
    RtspMediaSource(const std::string &vhost,
                    const std::string &app,
                    const std::string &stream_id,
                    int ring_size = RTP_GOP_SIZE) :
            MediaSource(RTSP_SCHEMA, vhost, app, stream_id), _ring_size(ring_size) {}

    ~RtspMediaSource() override{}

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
     * 获取该源的sdp
     */
    const std::string &getSdp() const {
        return _sdp;
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
    uint32_t getTimeStamp(TrackType trackType) override {
        assert(trackType >= TrackInvalid && trackType < TrackMax);
        if (trackType != TrackInvalid) {
            //获取某track的时间戳
            auto &track = _tracks[trackType];
            if (track) {
                return track->_time_stamp;
            }
        }

        //获取所有track的最小时间戳
        uint32_t ret = UINT32_MAX;
        for (auto &track : _tracks) {
            if (track && track->_time_stamp < ret) {
                ret = track->_time_stamp;
            }
        }
        return ret;
    }

    /**
     * 更新时间戳
     */
     void setTimeStamp(uint32_t stamp) override {
        for (auto &track : _tracks) {
            if (track) {
                track->_time_stamp = stamp;
            }
        }
    }

    /**
     * 设置sdp
     */
    virtual void setSdp(const std::string &sdp) {
        SdpParser sdp_parser(sdp);
        _tracks[TrackVideo] = sdp_parser.getTrack(TrackVideo);
        _tracks[TrackAudio] = sdp_parser.getTrack(TrackAudio);
        _have_video = (bool) _tracks[TrackVideo];
        _sdp = sdp_parser.toString();
        if (_ring) {
            regist();
        }
    }

    /**
     * 输入rtp
     * @param rtp rtp包
     * @param keyPos 该包是否为关键帧的第一个包
     */
    void onWrite(RtpPacket::Ptr rtp, bool keyPos) override {
        _speed[rtp->type] += rtp->size();
        assert(rtp->type >= 0 && rtp->type < TrackMax);
        auto &track = _tracks[rtp->type];
        auto stamp = rtp->getStampMS();
        if (track) {
            track->_seq = rtp->getSeq();
            track->_time_stamp = rtp->getStamp() * uint64_t(1000) / rtp->sample_rate;
            track->_ssrc = rtp->getSSRC();
        }
        if (!_ring) {
            std::weak_ptr<RtspMediaSource> weakSelf = std::dynamic_pointer_cast<RtspMediaSource>(shared_from_this());
            auto lam = [weakSelf](int size) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                strongSelf->onReaderChanged(size);
            };
            //GOP默认缓冲512组RTP包，每组RTP包时间戳相同(如果开启合并写了，那么每组为合并写时间内的RTP包),
            //每次遇到关键帧第一个RTP包，则会清空GOP缓存(因为有新的关键帧了，同样可以实现秒开)
            _ring = std::make_shared<RingType>(_ring_size, std::move(lam));
            onReaderChanged(0);
            if (!_sdp.empty()) {
                regist();
            }
        }
        bool is_video = rtp->type == TrackVideo;
        PacketCache<RtpPacket>::inputPacket(stamp, is_video, std::move(rtp), keyPos);
    }

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
