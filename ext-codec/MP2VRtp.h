/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP2VRTP_H
#define ZLMEDIAKIT_MP2VRTP_H

#include "MP2V.h"
#include "Common/Stamp.h"
#include "Rtsp/RtpCodec.h"

namespace mediakit {

// RFC 2250 MPEG Video-specific header (4 bytes)
//
//  0                   1                   2                   3
//  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
// |    MBZ  |T|         TR        |N|S|B|E|  P  | | BFC | | FFC |
// +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//                                  AN             FBV     FFV

static constexpr size_t kMP2VHeaderSize = 4;

/**
 * MP2V (MPEG-2 Video) RTP 解码器
 * 将 MPEG-2 Video over RTP 解复用出 MP2V Frame
 * RFC 2250
 */
class MP2VRtpDecoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<MP2VRtpDecoder>;

    MP2VRtpDecoder();

    /**
     * 输入 MPEG-2 Video RTP 包
     * @param rtp rtp包
     * @param key_pos 此参数忽略之
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true) override;

private:
    bool decodeRtp(const RtpPacket::Ptr &rtp);
    void outputFrame(const RtpPacket::Ptr &rtp);
    void obtainFrame();

private:
    bool _gop_dropped = true;
    bool _drop_flag = false;
    uint16_t _last_seq = 0;
    uint8_t _picture_type = 0;
    MP2VFrame::Ptr _frame;
    DtsGenerator _dts_generator;
};

/**
 * MP2V (MPEG-2 Video) RTP 编码器
 * 将 MPEG-2 Video 帧打包为 RTP
 * RFC 2250
 */
class MP2VRtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<MP2VRtpEncoder>;

    /**
     * 输入 MPEG-2 Video 帧
     * @param frame 帧数据
     */
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    /**
     * 构建 RFC 2250 MPEG Video-specific header
     * @param buf 输出缓冲区，至少4字节
     * @param data MPEG-2 ES 数据
     * @param size 数据大小
     * @param is_begin_of_slice 是否为 slice 起始
     * @param is_end_of_slice 是否为 slice 结束
     */
    void buildMpvHeader(uint8_t *buf, const uint8_t *data, size_t size,
                        bool is_begin_of_slice, bool is_end_of_slice);

    /**
     * 解析当前帧信息（picture type, temporal reference 等）
     */
    void parsePictureInfo(const uint8_t *data, size_t size);

    /**
     * 查找 sequence header 是否存在
     */
    bool hasSequenceHeader(const uint8_t *data, size_t size);

private:
    uint16_t _temporal_ref = 0;
    uint8_t _picture_type = 0;
    uint8_t _fbv = 0;
    uint8_t _bfc = 0;
    uint8_t _ffv = 0;
    uint8_t _ffc = 0;
    bool _has_seq_header = false;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_MP2VRTP_H
