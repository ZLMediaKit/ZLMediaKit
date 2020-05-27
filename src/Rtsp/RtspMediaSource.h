/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
using namespace std;
using namespace toolkit;
#define RTP_GOP_SIZE 512
namespace mediakit {

/**
 * rtsp媒体源的数据抽象
 * rtsp有关键的两要素，分别是sdp、rtp包
 * 只要生成了这两要素，那么要实现rtsp推流、rtsp服务器就很简单了
 * rtsp推拉流协议中，先传递sdp，然后再协商传输方式(tcp/udp/组播)，最后一直传递rtp
 */
class RtspMediaSource : public MediaSource, public RingDelegate<RtpPacket::Ptr>, public PacketCache<RtpPacket> {
public:
    typedef ResourcePool<RtpPacket> PoolType;
    typedef std::shared_ptr<RtspMediaSource> Ptr;
    typedef std::shared_ptr<List<RtpPacket::Ptr> > RingDataType;
    typedef RingBuffer<RingDataType> RingType;

    /**
     * 构造函数
     * @param vhost 虚拟主机名
     * @param app 应用名
     * @param stream_id 流id
     * @param ring_size 可以设置固定的环形缓冲大小，0则自适应
     */
    RtspMediaSource(const string &vhost,
                    const string &app,
                    const string &stream_id,
                    int ring_size = RTP_GOP_SIZE) :
            MediaSource(RTSP_SCHEMA, vhost, app, stream_id), _ring_size(ring_size) {}

    virtual ~RtspMediaSource() {}

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
    const string &getSdp() const {
        return _sdp;
    }

    /**
     * 获取相应轨道的ssrc
     */
    virtual uint32_t getSsrc(TrackType trackType) {
        auto track = _sdp_parser.getTrack(trackType);
        if (!track) {
            return 0;
        }
        return track->_ssrc;
    }

    /**
     * 获取相应轨道的seqence
     */
    virtual uint16_t getSeqence(TrackType trackType) {
        auto track = _sdp_parser.getTrack(trackType);
        if (!track) {
            return 0;
        }
        return track->_seq;
    }

    /**
     * 获取相应轨道的时间戳，单位毫秒
     */
    uint32_t getTimeStamp(TrackType trackType) override {
        auto track = _sdp_parser.getTrack(trackType);
        if (track) {
            return track->_time_stamp;
        }
        auto tracks = _sdp_parser.getAvailableTrack();
        switch (tracks.size()) {
            case 0:
                return 0;
            case 1:
                return tracks[0]->_time_stamp;
            default:
                return MIN(tracks[0]->_time_stamp, tracks[1]->_time_stamp);
        }
    }

    /**
     * 更新时间戳
     */
     void setTimeStamp(uint32_t uiStamp) override {
        auto tracks = _sdp_parser.getAvailableTrack();
        for (auto &track : tracks) {
            track->_time_stamp = uiStamp;
        }
    }

    /**
     * 设置sdp
     */
    virtual void setSdp(const string &sdp) {
        _sdp = sdp;
        _sdp_parser.load(sdp);
        _have_video = (bool)_sdp_parser.getTrack(TrackVideo);
        if (_ring) {
            regist();
        }
    }

    /**
     * 输入rtp
     * @param rtp rtp包
     * @param keyPos 该包是否为关键帧的第一个包
     */
    void onWrite(const RtpPacket::Ptr &rtp, bool keyPos) override {
        auto track = _sdp_parser.getTrack(rtp->type);
        if (track) {
            track->_seq = rtp->sequence;
            track->_time_stamp = rtp->timeStamp;
            track->_ssrc = rtp->ssrc;
        }
        if (!_ring) {
            weak_ptr<RtspMediaSource> weakSelf = dynamic_pointer_cast<RtspMediaSource>(shared_from_this());
            auto lam = [weakSelf](const EventPoller::Ptr &, int size, bool) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                strongSelf->onReaderChanged(size);
            };
            //rtp包缓存最大允许2048个，大概最多3MB数据
            //但是这个是GOP缓存的上限值，真实的GOP缓存大小等于两个I帧之间的包数的两倍
            //而且每次遇到I帧，则会清空GOP缓存，所以真实的GOP缓存远小于此值
            _ring = std::make_shared<RingType>(_ring_size, std::move(lam));
            onReaderChanged(0);
            if (!_sdp.empty()) {
                regist();
            }
        }
        PacketCache<RtpPacket>::inputPacket(rtp->type == TrackVideo, rtp, keyPos);
    }

private:

    /**
     * 批量flush rtp包时触发该函数
     * @param rtp_list rtp包列表
     * @param key_pos 是否包含关键帧
     */
    void onFlush(std::shared_ptr<List<RtpPacket::Ptr> > &rtp_list, bool key_pos) override {
        //如果不存在视频，那么就没有存在GOP缓存的意义，所以is_key一直为true确保一直清空GOP缓存
        _ring->write(rtp_list, _have_video ? key_pos : true);
    }

    /**
     * 每次增减消费者都会触发该函数
     */
    void onReaderChanged(int size) {
        if (size == 0) {
            onNoneReader();
        }
    }
private:
    int _ring_size;
    bool _have_video = false;
    SdpParser _sdp_parser;
    string _sdp;
    RingType::Ptr _ring;
};

} /* namespace mediakit */

#endif /* SRC_RTSP_RTSPMEDIASOURCE_H_ */
