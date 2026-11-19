/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_MP1V_H
#define ZLMEDIAKIT_MP1V_H

#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "MpegVideoSequence.h"

namespace mediakit {

/**
 * MPEG-1 Video 帧辅助类模板
 * MPEG-1 Video frame helper class template
 */
template <typename Parent>
class MP1VFrameHelper : public Parent {
public:
    using Ptr = std::shared_ptr<MP1VFrameHelper>;

    template <typename... ARGS>
    MP1VFrameHelper(ARGS &&...args)
        : Parent(std::forward<ARGS>(args)...) {
        this->_codec_id = CodecMP1V;
    }

    /**
     * MPEG-1 picture start code 为 00 00 01 00，picture_coding_type 为 1 时是 I-Picture。
     * MPEG-1 uses 00 00 01 00 as picture start code; picture_coding_type 1 denotes an I-Picture.
     */
    bool keyFrame() const override {
        auto size = this->size();
        auto prefix_size = this->prefixSize();
        auto data = this->data();
        if (!data || prefix_size > size) {
            return false;
        }
        return isMP1VKeyFrame((const uint8_t *)data + prefix_size, size - prefix_size);
    }

    bool configFrame() const override { return false; }

    static bool isMP1VKeyFrame(const uint8_t *data, size_t size) {
        if (!data) {
            return false;
        }
        // 查找 picture start code，再读取 picture_coding_type；循环条件保证可读取到 i + 5。
        // Locate the picture start code and read picture_coding_type; the loop bounds protect i + 5.
        for (size_t i = 0; i + 5 < size; ++i) {
            if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01 && data[i + 3] == 0x00) {
                auto picture_coding_type = (data[i + 5] >> 3) & 0x07;
                return picture_coding_type == 1;
            }
        }
        return false;
    }
};

using MP1VFrame = MP1VFrameHelper<FrameImp>;
using MP1VFrameNoCacheAble = MP1VFrameHelper<FrameFromPtr>;

/**
 * MPEG-1 Video Track
 */
class MP1VTrack : public VideoTrackImp {
public:
    using Ptr = std::shared_ptr<MP1VTrack>;

    MP1VTrack()
        : VideoTrackImp(CodecMP1V, 0, 0, 0) {}

    Track::Ptr clone() const override { return std::make_shared<MP1VTrack>(*this); }

    bool inputFrame(const Frame::Ptr &frame) override;

private:
    Sdp::Ptr getSdp(uint8_t payload_type) const override;

private:
    MpegVideoSequenceParser _sequence_parser;
};

} // namespace mediakit

#endif // ZLMEDIAKIT_MP1V_H
