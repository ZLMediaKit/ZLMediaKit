/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP2A_H
#define ZLMEDIAKIT_MP2A_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

/**
 * MPEG-1/2 Audio (Layer I/II) 帧辅助类模板
 * MPEG-1/2 Audio (Layer I/II) frame helper class template
 */
template <typename Parent>
class MP2AFrameHelper : public Parent {
public:
    using Ptr = std::shared_ptr<MP2AFrameHelper>;

    template <typename... ARGS>
    MP2AFrameHelper(ARGS &&...args)
        : Parent(std::forward<ARGS>(args)...) {
        this->_codec_id = CodecMP2A;
    }

    bool keyFrame() const override { return false; }
    bool configFrame() const override { return false; }
};

/// MPEG-1/2 Audio 帧类
using MP2AFrame = MP2AFrameHelper<FrameImp>;
using MP2AFrameNoCacheAble = MP2AFrameHelper<FrameFromPtr>;

// MPEG Audio 帧头解析工具
// MPEG Audio frame header parsing utility
struct MpegAudioFrameInfo {
    int version = 0;      // 1: MPEG-1, 2: MPEG-2, 3: MPEG-2.5
    int layer = 0;        // 1: Layer I, 2: Layer II, 3: Layer III
    int bitrate = 0;      // kbps
    int sample_rate = 0;  // Hz
    int channels = 0;     // 1: mono, 2: stereo
    int frame_size = 0;   // bytes per frame
    int samples_per_frame = 0;

    /**
     * 从 MPEG Audio sync word 解析帧头信息
     * Parse frame header info from MPEG Audio sync word
     * @param data 数据指针，至少4字节
     * @param size 数据大小
     * @return 是否解析成功
     */
    static bool parse(const uint8_t *data, size_t size, MpegAudioFrameInfo &info);
};

/**
 * MPEG-1/2 Audio (Layer I/II) Track
 * 对应 CodecMP2A
 */
class MP2ATrack : public AudioTrackImp {
public:
    using Ptr = std::shared_ptr<MP2ATrack>;

    MP2ATrack(int sample_rate = 44100, int channels = 2)
        : AudioTrackImp(CodecMP2A, sample_rate, channels, 16) {}

    bool inputFrame(const Frame::Ptr &frame) override;

private:
    /**
     * RFC 2250/3551 规定 MPA 的 RTP 时钟频率固定为 90000
     * RFC 2250/3551 specifies MPA RTP clock rate is fixed at 90000
     */
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
    Track::Ptr clone() const override;

private:
    bool _info_parsed = false;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_MP2A_H
