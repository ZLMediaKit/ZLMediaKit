/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTSPMUXER_H
#define ZLMEDIAKIT_RTSPMUXER_H

#include "Extension/Frame.h"
#include "Common/MediaSink.h"
#include "RtpCodec.h"

namespace mediakit{
/**
* rtsp生成器
*/
class RtspMuxer : public MediaSinkInterface{
public:
    typedef std::shared_ptr<RtspMuxer> Ptr;

    /**
     * 构造函数
     */
    RtspMuxer(const TitleSdp::Ptr &title = nullptr);

    virtual ~RtspMuxer(){}

    /**
     * 获取完整的SDP字符串
     * @return SDP字符串
     */
    string getSdp() ;

    /**
     * 获取rtp环形缓存
     * @return
     */
    RtpRing::RingType::Ptr getRtpRing() const;

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
private:
    string _sdp;
    RtpCodec::Ptr _encoder[TrackMax];
    RtpRing::RingType::Ptr _rtpRing;
};


} /* namespace mediakit */

#endif //ZLMEDIAKIT_RTSPMUXER_H
