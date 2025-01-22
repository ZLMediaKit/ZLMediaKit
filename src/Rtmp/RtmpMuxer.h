/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTMPMUXER_H
#define ZLMEDIAKIT_RTMPMUXER_H

#include "Rtmp/Rtmp.h"
#include "Extension/Frame.h"
#include "Common/MediaSink.h"
#include "RtmpCodec.h"

namespace mediakit{

class RtmpMuxer : public MediaSinkInterface {
public:
    using Ptr = std::shared_ptr<RtmpMuxer>;

    /**
     * 构造函数
     * Constructor
     
     * [AUTO-TRANSLATED:41469869]
     */
    RtmpMuxer(const TitleMeta::Ptr &title);

    /**
     * 获取完整的SDP字符串
     * @return SDP字符串
     * Get the complete SDP string
     * @return SDP string
     
     * [AUTO-TRANSLATED:f5d1b0a6]
     */
    const AMFValue &getMetadata() const ;

    /**
     * 获取rtmp环形缓存
     * @return
     * Get the rtmp ring buffer
     * @return
     
     * [AUTO-TRANSLATED:81679f3c]
     */
    RtmpRing::RingType::Ptr getRtmpRing() const;

    /**
     * 添加ready状态的track
     * Add a ready state track
     
     * [AUTO-TRANSLATED:2d8138b3]
     */
    bool addTrack(const Track::Ptr & track) override;

    /**
     * 写入帧数据
     * @param frame 帧
     * Write frame data
     * @param frame frame
     
     * [AUTO-TRANSLATED:b7c92013]
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 刷新输出所有frame缓存
     * Flush all frame buffers
     
     * [AUTO-TRANSLATED:adaea568]
     */
    void flush() override;

    /**
     * 重置所有track
     * Reset all tracks
     
     * [AUTO-TRANSLATED:f203fa3e]
     */
    void resetTracks() override ;

    /**
     * 生成config包
     * Generate config package
     
     
     * [AUTO-TRANSLATED:8f851364]
     */
     void makeConfigPacket();

private:
    bool _track_existed[2] = { false, false };

    AMFValue _metadata;
    RtmpRing::RingType::Ptr _rtmp_ring;
    std::unordered_map<int, RtmpCodec::Ptr> _encoders;
};


} /* namespace mediakit */

#endif //ZLMEDIAKIT_RTMPMUXER_H
