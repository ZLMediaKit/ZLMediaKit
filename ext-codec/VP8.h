#ifndef ZLMEDIAKIT_VP8_H
#define ZLMEDIAKIT_VP8_H

#include "Extension/Frame.h"
#include "Extension/Track.h"
#include "webm-vpx.h"
namespace mediakit {
template <typename Parent>
class VP8FrameHelper : public Parent {
public:
    friend class FrameImp;
    //friend class toolkit::ResourcePool_l<VP8FrameHelper>;
    using Ptr = std::shared_ptr<VP8FrameHelper>;

    template <typename... ARGS>
    VP8FrameHelper(ARGS &&...args)
        : Parent(std::forward<ARGS>(args)...) {
        this->_codec_id = CodecVP8;
    }

    bool keyFrame() const override {
        auto ptr = (uint8_t *) this->data() + this->prefixSize();
        return !(*ptr & 0x01);
    }
    bool configFrame() const override { return false; }
    bool dropAble() const override { return false; }
    bool decodeAble() const override { return true; }
};

/// VP8 帧类
using VP8Frame = VP8FrameHelper<FrameImp>;
using VP8FrameNoCacheAble = VP8FrameHelper<FrameFromPtr>;

class VP8Track : public VideoTrackImp {
public:
    VP8Track() : VideoTrackImp(CodecVP8) {}
    
    Track::Ptr clone() const override { return std::make_shared<VP8Track>(*this); }

    bool inputFrame(const Frame::Ptr &frame) override;
    toolkit::Buffer::Ptr getExtraData() const override;
    void setExtraData(const uint8_t *data, size_t size) override;
private:
    webm_vpx_t _vpx = {0};
};

} // namespace mediakit

#endif