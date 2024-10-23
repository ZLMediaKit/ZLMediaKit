#ifndef ZLMEDIAKIT_VP9_H
#define ZLMEDIAKIT_VP9_H

#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "webm-vpx.h"
namespace mediakit {
template <typename Parent>
class VP9FrameHelper : public Parent {
public:
    friend class FrameImp;
    //friend class toolkit::ResourcePool_l<VP9FrameHelper>;
    using Ptr = std::shared_ptr<VP9FrameHelper>;

    template <typename... ARGS>
    VP9FrameHelper(ARGS &&...args)
        : Parent(std::forward<ARGS>(args)...) {
        this->_codec_id = CodecVP9;
    }

    bool keyFrame() const override {
        auto ptr = (uint8_t *) this->data() + this->prefixSize();
        return (*ptr & 0x80);
    }
    bool configFrame() const override { return false; }
    bool dropAble() const override { return false; }
    bool decodeAble() const override { return true; }
};

/// VP9 帧类
using VP9Frame = VP9FrameHelper<FrameImp>;
using VP9FrameNoCacheAble = VP9FrameHelper<FrameFromPtr>;

class VP9Track : public VideoTrackImp {
public:
    VP9Track() : VideoTrackImp(CodecVP9) {};

    Track::Ptr clone() const override { return std::make_shared<VP9Track>(*this); }

    bool inputFrame(const Frame::Ptr &frame) override;
    toolkit::Buffer::Ptr getExtraData() const override;
    void setExtraData(const uint8_t *data, size_t size) override;
private:
    webm_vpx_t _vpx = {0};
};

} // namespace mediakit

#endif