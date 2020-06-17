/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
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
#include "RtmpDemuxer.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Util/ResourcePool.h"
#include "Util/NoticeCenter.h"
#include "Thread/ThreadPool.h"
using namespace toolkit;

#define RTMP_GOP_SIZE 512
namespace mediakit {

/**
 * rtmp媒体源的数据抽象
 * rtmp有关键的三要素，分别是metadata、config帧，普通帧
 * 其中metadata是非必须的，有些编码格式也没有config帧(比如MP3)
 * 只要生成了这三要素，那么要实现rtmp推流、rtmp服务器就很简单了
 * rtmp推拉流协议中，先传递metadata，然后传递config帧，然后一直传递普通帧
 */
class RtmpMediaSource : public MediaSource, public RingDelegate<RtmpPacket::Ptr>, public PacketCache<RtmpPacket>{
public:
    typedef std::shared_ptr<RtmpMediaSource> Ptr;
    typedef std::shared_ptr<List<RtmpPacket::Ptr> > RingDataType;
    typedef RingBuffer<RingDataType> RingType;

    /**
     * 构造函数
     * @param vhost 虚拟主机名
     * @param app 应用名
     * @param stream_id 流id
     * @param ring_size 可以设置固定的环形缓冲大小，0则自适应
     */
    RtmpMediaSource(const string &vhost,
                    const string &app,
                    const string &stream_id,
                    int ring_size = RTMP_GOP_SIZE) :
            MediaSource(RTMP_SCHEMA, vhost, app, stream_id), _ring_size(ring_size) {
    }

    virtual ~RtmpMediaSource() {}

    /**
     * 	获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const {
        return _ring;
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
    const AMFValue &getMetaData() const {
        lock_guard<recursive_mutex> lock(_mtx);
        return _metadata;
    }

    /**
     * 获取所有的config帧
     */
    template<typename FUNC>
    void getConfigFrame(const FUNC &f) {
        lock_guard<recursive_mutex> lock(_mtx);
        for (auto &pr : _config_frame_map) {
            f(pr.second);
        }
    }

    /**
     * 设置metadata
     */
    virtual void setMetaData(const AMFValue &metadata) {
        lock_guard<recursive_mutex> lock(_mtx);
        _metadata = metadata;
        if(_ring){
            regist();
        }
    }

    /**
     * 更新metadata
     */
    void updateMetaData(const AMFValue &metadata) {
        lock_guard<recursive_mutex> lock(_mtx);
        _metadata = metadata;
    }

    /**
     * 输入rtmp包
     * @param pkt rtmp包
     * @param key 是否为关键帧
     */
    void onWrite(const RtmpPacket::Ptr &pkt, bool key = true) override {
        lock_guard<recursive_mutex> lock(_mtx);
        if(pkt->typeId == MSG_VIDEO){
            //有视频，那么启用GOP缓存
            _have_video = true;
        }
        if (pkt->isCfgFrame()) {
            _config_frame_map[pkt->typeId] = pkt;
            return;
        }

        //保存当前时间戳
        _track_stamps_map[pkt->typeId] = pkt->timeStamp;

        if (!_ring) {
            weak_ptr<RtmpMediaSource> weakSelf = dynamic_pointer_cast<RtmpMediaSource>(shared_from_this());
            auto lam = [weakSelf](const EventPoller::Ptr &, int size, bool) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                strongSelf->onReaderChanged(size);
            };

            //rtmp包缓存最大允许512个，如果是纯视频(25fps)大概为20秒数据
            //但是这个是GOP缓存的上限值，真实的GOP缓存大小等于两个I帧之间的包数的两倍
            //而且每次遇到I帧，则会清空GOP缓存，所以真实的GOP缓存远小于此值
            _ring = std::make_shared<RingType>(_ring_size,std::move(lam));
            onReaderChanged(0);

            if(_metadata){
                regist();
            }
        }
        PacketCache<RtmpPacket>::inputPacket(pkt->typeId == MSG_VIDEO, pkt, key);
    }

    /**
     * 获取当前时间戳
     */
    uint32_t getTimeStamp(TrackType trackType) override {
        lock_guard<recursive_mutex> lock(_mtx);
        switch (trackType) {
            case TrackVideo:
                return _track_stamps_map[MSG_VIDEO];
            case TrackAudio:
                return _track_stamps_map[MSG_AUDIO];
            default:
                return MAX(_track_stamps_map[MSG_VIDEO], _track_stamps_map[MSG_AUDIO]);
        }
    }

private:

    /**
    * 批量flush rtmp包时触发该函数
    * @param rtmp_list rtmp包列表
    * @param key_pos 是否包含关键帧
    */
    void onFlush(std::shared_ptr<List<RtmpPacket::Ptr> > &rtmp_list, bool key_pos) override {
        //如果不存在视频，那么就没有存在GOP缓存的意义，所以is_key一直为true确保一直清空GOP缓存
        _ring->write(rtmp_list, _have_video ? key_pos : true);
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
    mutable recursive_mutex _mtx;
    AMFValue _metadata;
    RingType::Ptr _ring;
    unordered_map<int, uint32_t> _track_stamps_map;
    unordered_map<int, RtmpPacket::Ptr> _config_frame_map;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPMEDIASOURCE_H_ */
