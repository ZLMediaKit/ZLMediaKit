/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
    RtmpMediaSource(const std::string &vhost,
                    const std::string &app,
                    const std::string &stream_id,
                    int ring_size = RTMP_GOP_SIZE) :
            MediaSource(RTMP_SCHEMA, vhost, app, stream_id), _ring_size(ring_size) {
    }

    ~RtmpMediaSource() override{}

    /**
     * 	获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const {
        return _ring;
    }

    void getPlayerList(const std::function<void(const std::list<std::shared_ptr<void>> &info_list)> &cb,
                       const std::function<std::shared_ptr<void>(std::shared_ptr<void> &&info)> &on_change) override {
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
    const AMFValue &getMetaData() const {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        return _metadata;
    }

    /**
     * 获取所有的config帧
     */
    template<typename FUNC>
    void getConfigFrame(const FUNC &f) {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        for (auto &pr : _config_frame_map) {
            f(pr.second);
        }
    }

    /**
     * 设置metadata
     */
    virtual void setMetaData(const AMFValue &metadata) {
        _metadata = metadata;
        _metadata.set("server", kServerName);
        _have_video = _metadata["videocodecid"];
        _have_audio = _metadata["audiocodecid"];
        if (_ring) {
            regist();
        }
    }

    /**
     * 更新metadata
     */
    void updateMetaData(const AMFValue &metadata) {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        _metadata = metadata;
    }

    /**
     * 输入rtmp包
     * @param pkt rtmp包
     */
    void onWrite(RtmpPacket::Ptr pkt, bool = true) override {
        bool is_video = pkt->type_id == MSG_VIDEO;
        _speed[is_video ? TrackVideo : TrackAudio] += pkt->size();
        //保存当前时间戳
        switch (pkt->type_id) {
            case MSG_VIDEO : _track_stamps[TrackVideo] = pkt->time_stamp, _have_video = true; break;
            case MSG_AUDIO : _track_stamps[TrackAudio] = pkt->time_stamp, _have_audio = true; break;
            default :  break;
        }

        if (pkt->isCfgFrame()) {
            std::lock_guard<std::recursive_mutex> lock(_mtx);
            _config_frame_map[pkt->type_id] = pkt;
            if (!_ring) {
                //注册后收到config帧更新到各播放器
                return;
            }
        }

        if (!_ring) {
            std::weak_ptr<RtmpMediaSource> weakSelf = std::dynamic_pointer_cast<RtmpMediaSource>(shared_from_this());
            auto lam = [weakSelf](int size) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                strongSelf->onReaderChanged(size);
            };

            //GOP默认缓冲512组RTMP包，每组RTMP包时间戳相同(如果开启合并写了，那么每组为合并写时间内的RTMP包),
            //每次遇到关键帧第一个RTMP包，则会清空GOP缓存(因为有新的关键帧了，同样可以实现秒开)
            _ring = std::make_shared<RingType>(_ring_size,std::move(lam));
            if(_metadata){
                regist();
            }
        }
        bool key = pkt->isVideoKeyFrame();
        auto stamp  = pkt->time_stamp;
        PacketCache<RtmpPacket>::inputPacket(stamp, is_video, std::move(pkt), key);
    }

    /**
     * 获取当前时间戳
     */
    uint32_t getTimeStamp(TrackType trackType) override {
        assert(trackType >= TrackInvalid && trackType < TrackMax);
        if (trackType != TrackInvalid) {
            //获取某track的时间戳
            return _track_stamps[trackType];
        }

        //获取所有track的最小时间戳
        uint32_t ret = UINT32_MAX;
        for (auto &stamp : _track_stamps) {
            if (stamp > 0 && stamp < ret) {
                ret = stamp;
            }
        }
        return ret;
    }

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
