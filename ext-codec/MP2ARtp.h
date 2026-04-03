/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP2ARTP_H
#define ZLMEDIAKIT_MP2ARTP_H

#include "MP2A.h"
#include "Rtsp/RtpCodec.h"

namespace mediakit {

// RFC 2250 Section 3.5 MPEG Audio-specific header (4 bytes)
//
//  0               1               2               3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |             MBZ               |          Frag_offset          |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
// MBZ: Must Be Zero (16 bits)
// Frag_offset: Byte offset into the audio frame for the data in this packet (16 bits)

static constexpr size_t kMP2AHeaderSize = 4;

/**
 * MP2A (MPEG-1/2 Audio Layer I/II) RTP 编码器
 * RFC 2250 Section 3.5
 */
class MP2ARtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<MP2ARtpEncoder>;

    /**
     * 输入 MPEG Audio 帧并打包为 RTP
     * @param frame 帧数据
     */
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    /**
     * 输出一个 RTP 包
     * @param data ES 数据
     * @param len 数据长度
     * @param frag_offset 分片在帧内的偏移
     * @param mark 是否为帧最后一个包
     * @param stamp 时间戳(ms)
     */
    void outputRtp(const char *data, size_t len, size_t frag_offset, bool mark, uint64_t stamp);
};

/**
 * MP2A (MPEG-1/2 Audio Layer I/II) RTP 解码器
 * RFC 2250 Section 3.5
 */
class MP2ARtpDecoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<MP2ARtpDecoder>;

    MP2ARtpDecoder();

    /**
     * 输入 MPEG Audio RTP 包并解码
     * @param rtp rtp 数据包
     * @param key_pos 音频帧忽略此参数
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false) override;

private:
    void obtainFrame();
    void flushData();

private:
    uint16_t _last_seq = 0;
    uint32_t _last_stamp = 0;
    FrameImp::Ptr _frame;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_MP2ARTP_H
