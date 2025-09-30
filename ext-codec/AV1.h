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
#include "aom-av1.h"
namespace mediakit {

template <typename Parent>
class AV1FrameHelper : public Parent {
public:
    friend class FrameImp;
    //friend class toolkit::ResourcePool_l<Av1FrameHelper>;
    using Ptr = std::shared_ptr<AV1FrameHelper>;

    template <typename... ARGS>
    AV1FrameHelper(ARGS &&...args)
        : Parent(std::forward<ARGS>(args)...) {
        this->_codec_id = CodecAV1;
    }

    bool keyFrame() const override {
        auto ptr = (uint8_t *) this->data() + this->prefixSize();
        return (*ptr & 0x78) >> 3 == 1;
    }
    bool configFrame() const override { return false; }
    bool dropAble() const override { return false; }
    bool decodeAble() const override { return true; }
};

/// Av1 帧类
using AV1Frame = AV1FrameHelper<FrameImp>;
using AV1FrameNoCacheAble = AV1FrameHelper<FrameFromPtr>;

/**
 * AV1视频通道
 */
class AV1Track : public VideoTrackImp {
public:
    using Ptr = std::shared_ptr<AV1Track>;

    AV1Track() : VideoTrackImp(CodecAV1) {}

    Track::Ptr clone() const override;

    bool inputFrame(const Frame::Ptr &frame) override;
    toolkit::Buffer::Ptr getExtraData() const override;
    void setExtraData(const uint8_t *data, size_t size) override;
protected:
    aom_av1_t _context = {0};
};

} // namespace mediakit

#endif