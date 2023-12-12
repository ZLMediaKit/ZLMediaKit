#ifndef ZLMEDIAKIT_JPEG_H
#define ZLMEDIAKIT_JPEG_H

#include "Extension/Frame.h"
#include "Extension/Track.h"

namespace mediakit {

class JPEGTrack : public VideoTrack {
public:
    using Ptr = std::shared_ptr<JPEGTrack>;

    CodecId getCodecId() const override { return CodecJPEG; }
    int getVideoHeight() const override { return _height; }
    int getVideoWidth() const override { return _width; }
    float getVideoFps() const override { return _fps; }
    bool ready() const override { return _fps > 0; }
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    Sdp::Ptr getSdp(uint8_t payload_type) const override;
    Track::Ptr clone() const override { return std::make_shared<JPEGTrack>(*this); }
    void getVideoResolution(const uint8_t *buf, int len);

private:
    int _width = 0;
    int _height = 0;
    float _fps = 0;
    uint64_t _tmp = 0;
};

class JPEGFrameType {
public:
    virtual ~JPEGFrameType() = default;
    virtual uint8_t pixType() const = 0;
};

template <typename Parent>
class JPEGFrame : public Parent, public JPEGFrameType {
public:
    static constexpr auto kJFIFSize = 20u;
    /**
     *  JPEG/MJPEG帧
     * @param pix_type pixel format type; AV_PIX_FMT_YUVJ422P || (AVCOL_RANGE_JPEG && AV_PIX_FMT_YUV422P) : 1; AV_PIX_FMT_YUVJ420P || (AVCOL_RANGE_JPEG && AV_PIX_FMT_YUV420P) : 0
     */
    template <typename... ARGS>
    JPEGFrame(uint8_t pix_type, ARGS &&...args) : Parent(std::forward<ARGS>(args)...) {
        _pix_type = pix_type;
        // JFIF头固定20个字节长度
        CHECK(this->size() > kJFIFSize);
    }
    size_t prefixSize() const override { return 0; }
    bool keyFrame() const override { return true; }
    bool configFrame() const override { return false; }
    CodecId getCodecId() const override { return CodecJPEG; }
    uint8_t pixType() const override { return _pix_type; }

private:
    uint8_t _pix_type;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_JPEG_H
