/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_
#define SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "amf.h"
#include "Rtmp.h"
#include "RtmpDemuxer.h"
#include "RtmpMediaSource.h"
#include "Common/MultiMediaSourceMuxer.h"

namespace mediakit {

class RtmpMediaSourceImp final : public RtmpMediaSource, private TrackListener, public MultiMediaSourceMuxer::Listener {
public:
    using Ptr = std::shared_ptr<RtmpMediaSourceImp>;

    /**
     * 构造函数
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param id 流id
     * @param ringSize 环形缓存大小
     */
    RtmpMediaSourceImp(const MediaTuple& tuple, int ringSize = RTMP_GOP_SIZE);

    ~RtmpMediaSourceImp() override = default;

    /**
     * 设置metadata
     */
    void setMetaData(const AMFValue &metadata) override;

    /**
     * 输入rtmp并解析
     */
    void onWrite(RtmpPacket::Ptr pkt, bool = true) override;

    /**
     * 获取观看总人数，包括(hls/rtsp/rtmp)
     */
    int totalReaderCount() override;

    /**
     * 设置协议转换
     */
    void setProtocolOption(const ProtocolOption &option);

    const ProtocolOption &getProtocolOption() const {
        return _option;
    }

    /**
     * _demuxer触发的添加Track事件
     */
    bool addTrack(const Track::Ptr &track) override;

    /**
     * _demuxer触发的Track添加完毕事件
     */
    void addTrackCompleted() override;

    void resetTracks() override;

    /**
     * _muxer触发的所有Track就绪的事件
     */
    void onAllTrackReady() override;

    /**
     * 设置事件监听器
     * @param listener 监听器
     */
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override;

private:
    bool _all_track_ready = false;
    bool _recreate_metadata = false;
    ProtocolOption _option;
    AMFValue _metadata;
    RtmpDemuxer::Ptr _demuxer;
    MultiMediaSourceMuxer::Ptr _muxer;

};
} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_ */
