/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
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

class RtmpMuxer : public MediaSinkInterface{
public:
    typedef std::shared_ptr<RtmpMuxer> Ptr;

    /**
     * 构造函数
     */
    RtmpMuxer(const TitleMeta::Ptr &title);
    virtual ~RtmpMuxer(){}

    /**
     * 获取完整的SDP字符串
     * @return SDP字符串
     */
    const AMFValue &getMetadata() const ;

    /**
     * 获取rtmp环形缓存
     * @return
     */
    RtmpRing::RingType::Ptr getRtmpRing() const;

    /**
     * 添加ready状态的track
     */
    void addTrack(const Track::Ptr & track) override;

    /**
     * 写入帧数据
     * @param frame 帧
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 重置所有track
     */
    void resetTracks() override ;

    /**
     * 生成config包
     */
     void makeConfigPacket();
private:
    RtmpRing::RingType::Ptr _rtmpRing;
    AMFValue _metadata;
    RtmpCodec::Ptr _encoder[TrackMax];
};


} /* namespace mediakit */

#endif //ZLMEDIAKIT_RTMPMUXER_H
