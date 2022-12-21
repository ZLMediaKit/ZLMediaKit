#ifndef ZLMEDIAKIT_JPEG_H
#define ZLMEDIAKIT_JPEG_H

#include "Frame.h"
#include "Track.h"

namespace mediakit {

class JPEGTrack : public VideoTrack {
public:
    using Ptr = std::shared_ptr<JPEGTrack>;

    CodecId getCodecId() const override { return CodecJPEG; }
    int getVideoHeight() const override { return _height; }
    int getVideoWidth() const override { return _width; }
    float getVideoFps() const override { return _fps; }
    bool ready() override { return _fps > 0; }
    bool inputFrame(const Frame::Ptr &frame) override;

private:
    Sdp::Ptr getSdp() override;
    Track::Ptr clone() override { return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this); }
    void getVideoResolution(const uint8_t *buf, int len);

private:
    int _width = 0;
    int _height = 0;
    float _fps = 0;
    uint64_t _tmp = 0;
};

class JPEGFrame : public Frame {
public:
    /**
     *  JPEG/MJPEG帧
     * @param buffer 帧数据
     * @param dts 时间戳,单位毫秒
     * @param pix_type pixel format type; AV_PIX_FMT_YUVJ422P || (AVCOL_RANGE_JPEG && AV_PIX_FMT_YUV422P) : 1; AV_PIX_FMT_YUVJ420P || (AVCOL_RANGE_JPEG && AV_PIX_FMT_YUV420P) : 0
     * @param prefix_size JFIF头大小
     */
    JPEGFrame(toolkit::Buffer::Ptr buffer, uint64_t dts, uint8_t pix_type = 0, size_t prefix_size = 0) {
        _buffer = std::move(buffer);
        _dts = dts;
        _pix_type = pix_type;
        _prefix_size = prefix_size;
    }
    ~JPEGFrame() override = default;

    uint64_t dts() const override { return _dts; }
    size_t prefixSize() const override { return _prefix_size; }
    bool keyFrame() const override { return true; }
    bool configFrame() const override { return false; }
    CodecId getCodecId() const override { return CodecJPEG; }

    char *data() const override { return _buffer->data(); }
    size_t size() const override { return _buffer->size(); }

    uint8_t pixType() const {return _pix_type; }

private:
    uint8_t _pix_type;
    size_t _prefix_size;
    uint64_t _dts;
    toolkit::Buffer::Ptr _buffer;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_JPEG_H
