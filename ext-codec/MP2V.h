/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP2V_H
#define ZLMEDIAKIT_MP2V_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

/**
 * MPEG-2 Video 帧辅助类模板
 * MPEG-2 Video frame helper class template
 */
template <typename Parent>
class MP2VFrameHelper : public Parent {
public:
    using Ptr = std::shared_ptr<MP2VFrameHelper>;

    template <typename... ARGS>
    MP2VFrameHelper(ARGS &&...args)
        : Parent(std::forward<ARGS>(args)...) {
        this->_codec_id = CodecMP2V;
    }

    /**
     * MPEG-2 视频起始码: 00 00 01 00 (picture_start_code)
     * I帧判断：picture_coding_type == 1 (I-Picture)
     * picture_coding_type 位于 picture header 的第 11-12 bit (从 temporal_reference 之后)
     *
     * MPEG-2 video start code: 00 00 01 00 (picture_start_code)
     * I-frame detection: picture_coding_type == 1 (I-Picture)
     */
    bool keyFrame() const override {
        auto data = (const uint8_t *)this->data() + this->prefixSize();
        auto size = this->size() - this->prefixSize();
        return isMP2VKeyFrame(data, size);
    }

    bool configFrame() const override { return false; }

    static bool isMP2VKeyFrame(const uint8_t *data, size_t size) {
        // 查找 picture start code (00 00 01 00)，然后检查 picture_coding_type
        // Look for picture start code (00 00 01 00), then check picture_coding_type
        for (size_t i = 0; i + 5 < size; ++i) {
            if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01 && data[i + 3] == 0x00) {
                // picture header: temporal_reference(10bits) + picture_coding_type(3bits)
                // picture_coding_type: 001 = I, 010 = P, 011 = B
                uint8_t picture_coding_type = (data[i + 5] >> 3) & 0x07;
                return picture_coding_type == 1;
            }
        }
        return false;
    }
};

/// MPEG-2 Video 帧类
using MP2VFrame = MP2VFrameHelper<FrameImp>;
using MP2VFrameNoCacheAble = MP2VFrameHelper<FrameFromPtr>;

/**
 * MPEG-2 Video Track
 */
class MP2VTrack : public VideoTrackImp {
public:
    using Ptr = std::shared_ptr<MP2VTrack>;

    MP2VTrack() : VideoTrackImp(CodecMP2V) {}

    Track::Ptr clone() const override { return std::make_shared<MP2VTrack>(*this); }

    bool inputFrame(const Frame::Ptr &frame) override;

private:
    Sdp::Ptr getSdp(uint8_t payload_type) const override;

    /**
     * 从 sequence header 中解析宽高和帧率
     * Parse width, height and fps from sequence header
     */
    void parseSequenceHeader(const uint8_t *data, size_t size);

private:
    bool _seq_header_parsed = false;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_MP2V_H
