/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_AV1_H
#define ZLMEDIAKIT_AV1_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

class AV1Track : public VideoTrack {
public:
    using Ptr = std::shared_ptr<AV1Track>;

    AV1Track();

    static uint64_t leb128(const uint8_t *ptr, size_t len, size_t &consumed);
    static size_t write_leb128(uint64_t value, uint8_t *ptr, size_t len);
    static bool isConfigFrame(const char *data);

    CodecId getCodecId() const override { return CodecAV1; }
    int getVideoHeight() const override { return _width; }
    int getVideoWidth() const override { return _height; }
    float getVideoFps() const override { return _fps; }
    bool ready() const override { return _width; }
    bool update() override;
    bool inputFrame(const Frame::Ptr &frame) override;
    toolkit::Buffer::Ptr getExtraData() const override;
    void setExtraData(const uint8_t *data, size_t size) override;
    Track::Ptr clone() const override { return std::make_shared<AV1Track>(*this); }
    Sdp::Ptr getSdp(uint8_t payload_type) const override;

private:
    bool inputFrame_l(const Frame::Ptr &frame);

private:
    int _width = 0;
    int _height = 0;
    float _fps = 0;
    toolkit::Buffer::Ptr _config;
    std::shared_ptr<struct AV1FrameOBUContext> _context;
};

template <typename Parent>
class AV1FrameHelper : public Parent {
public:
    friend class FrameImp;
    friend class toolkit::ResourcePool_l<AV1FrameHelper>;
    using Ptr = std::shared_ptr<AV1FrameHelper>;

    template <typename... ARGS>
    AV1FrameHelper(ARGS &&...args)
        : Parent(std::forward<ARGS>(args)...) {
        this->_codec_id = CodecAV1;
    }

    bool configFrame() const override {
        return AV1Track::isConfigFrame(this->data() + this->prefixSize());
    }

    bool keyFrame() const override {
        return configFrame();
    }
};

using AV1Frame = AV1FrameHelper<FrameImp>;
using AV1FrameNoCacheAble = AV1FrameHelper<FrameFromPtr>;

} // namespace mediakit

#endif