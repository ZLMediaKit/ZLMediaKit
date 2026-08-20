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
        int size = this->size() - this->prefixSize();
        if (size < 4) {
            return false;
        }
        return (*ptr & 0xCC) == 0x80 && ptr[1] == 0x49 && ptr[2] == 0x83 && ptr[3] == 0x42;
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
    webm_vpx_t _vpx {};
};

} // namespace mediakit

#endif