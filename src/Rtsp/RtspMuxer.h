/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTSPMUXER_H
#define ZLMEDIAKIT_RTSPMUXER_H

#include "Extension/Frame.h"
#include "Common/MediaSink.h"
#include "Common/Stamp.h"
#include "RtpCodec.h"

namespace mediakit{
/**
* rtsp生成器
*/
class RtspMuxer : public MediaSinkInterface{
public:
    friend class RingDelegateHelper;
    using Ptr = std::shared_ptr<RtspMuxer>;

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
    void onRtp(RtpPacket::Ptr in, bool is_key);
    void computeNtp(const Frame::Ptr &frame);
    void trySyncTrack();

private:
    uint32_t _rtp_stamp[TrackMax]{0};
    uint64_t _ntp_stamp[TrackMax]{0};
    uint64_t _ntp_stamp_start;
    string _sdp;
    Stamp _stamp[TrackMax];
    RtpCodec::Ptr _encoder[TrackMax];
    RtpRing::RingType::Ptr _rtpRing;
    RtpRing::RingType::Ptr _rtpInterceptor;
};


} /* namespace mediakit */

#endif //ZLMEDIAKIT_RTSPMUXER_H
