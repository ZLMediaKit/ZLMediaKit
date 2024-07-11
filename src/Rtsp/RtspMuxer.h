﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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

class RingDelegateHelper : public toolkit::RingDelegate<RtpPacket::Ptr> {
public:
    using onRtp = std::function<void(RtpPacket::Ptr in, bool is_key)> ;

    RingDelegateHelper(onRtp on_rtp) {
        _on_rtp = std::move(on_rtp);
    }

    void onWrite(RtpPacket::Ptr in, bool is_key) override {
        _on_rtp(std::move(in), is_key);
    }

private:
    onRtp _on_rtp;
};

/**
* rtsp生成器
*/
class RtspMuxer : public MediaSinkInterface {
public:
    using Ptr = std::shared_ptr<RtspMuxer>;

    /**
     * 构造函数
     */
    RtspMuxer(const TitleSdp::Ptr &title = nullptr);

    /**
     * 获取完整的SDP字符串
     * @return SDP字符串
     */
    std::string getSdp() ;

    /**
     * 获取rtp环形缓存
     * @return
     */
    RtpRing::RingType::Ptr getRtpRing() const;

    /**
     * 添加ready状态的track
     */
    bool addTrack(const Track::Ptr & track) override;

    /**
     * 写入帧数据
     * @param frame 帧
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 刷新输出所有frame缓存
     */
    void flush() override;

    /**
     * 重置所有track
     */
    void resetTracks() override ;

private:
    void onRtp(RtpPacket::Ptr in, bool is_key);
    void trySyncTrack();

private:
    bool _live = true;
    bool _track_existed[2] = { false, false };

    uint8_t _index {0};
    uint64_t _ntp_stamp_start;
    std::string _sdp;

    struct TrackInfo {
        Stamp stamp;
        uint32_t rtp_stamp { 0 };
        uint64_t ntp_stamp { 0 };
        RtpCodec::Ptr encoder;
    };

    std::unordered_map<int, TrackInfo> _tracks;
    RtpRing::RingType::Ptr _rtpRing;
    RtpRing::RingType::Ptr _rtpInterceptor;
};


} /* namespace mediakit */

#endif //ZLMEDIAKIT_RTSPMUXER_H
