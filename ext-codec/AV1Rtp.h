/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AV1RTP_H
#define ZLMEDIAKIT_AV1RTP_H

#include "Rtsp/RtpCodec.h"
#include "Extension/Frame.h"
#include "Extension/CommonRtp.h"

namespace mediakit {

/**
 * AV1 RTP编码器
 */
class AV1RtpEncoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<AV1RtpEncoder>;

    AV1RtpEncoder();
    ~AV1RtpEncoder() override = default;

    bool inputFrame(const Frame::Ptr &frame) override;

private:
    // AV1 OBU信息
    struct ObuInfo {
        uint8_t header;
        uint8_t extension_header;
        const uint8_t* payload_data;
        size_t payload_size;
        bool has_extension;
        bool has_size_field;
    };

    std::vector<ObuInfo> parseObus(const uint8_t* data, size_t size);
    void outputRtp(const uint8_t* data, size_t len, bool mark, uint64_t stamp, uint8_t aggregation_header);
    uint8_t makeAggregationHeader(bool first_obu_is_fragment, bool last_obu_is_fragment,
                                  int num_obu_elements, bool starts_new_coded_video_sequence);
    bool sendObu(const ObuInfo& obu, bool is_first_obu, bool is_last_obu,
                 bool starts_new_sequence, uint64_t stamp, size_t max_payload_size);

private:
    bool _got_key_frame = false;
};

/**
 * AV1 RTP解码器
 */
class AV1RtpDecoder : public RtpCodec {
public:
    using Ptr = std::shared_ptr<AV1RtpDecoder>;

    AV1RtpDecoder();
    ~AV1RtpDecoder() override = default;

    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = false) override;

private:
    struct AggregationHeader {
        bool first_obu_is_fragment;     // Z bit
        bool last_obu_is_fragment;      // Y bit
        int num_obu_elements;           // W field (0 = any number)
        bool starts_new_coded_video_sequence;  // N bit
    };

    AggregationHeader parseAggregationHeader(uint8_t header);
    void obtainFrame();
    bool emitObu(const uint8_t* data, size_t size);
    bool processPayload(const AggregationHeader& agg_header, const uint8_t* data,
                        size_t remaining);
    void flushFrame(uint64_t stamp);
    void resetState();

private:
    uint64_t _last_dts = 0;
    FrameImp::Ptr _frame;
    std::vector<uint8_t> _fragment_buffer;
    bool _assembling_fragment = false;
    bool _received_keyframe = false;
    bool _has_last_seq = false;
    uint16_t _last_seq = 0;
    bool _has_last_ssrc = false;
    uint32_t _last_ssrc = 0;
};

}//namespace mediakit
#endif //ZLMEDIAKIT_AV1RTP_H
