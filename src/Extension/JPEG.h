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
    bool ready() override { return true; }
    bool inputFrame(const Frame::Ptr &frame) override { return VideoTrack::inputFrame(frame); }

private:
    Sdp::Ptr getSdp() override;
    Track::Ptr clone() override { return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this); }

private:
    int _width = 0;
    int _height = 0;
    float _fps = 0;
};

class JPEGFrame : public Frame {
public:
    JPEGFrame(toolkit::Buffer::Ptr buffer, uint64_t dts) {
        _buffer = std::move(buffer);
        _dts = dts;
    }
    ~JPEGFrame() override = default;

    uint64_t dts() const override { return _dts; }
    size_t prefixSize() const override { return 0; }
    bool keyFrame() const override { return true; }
    bool configFrame() const override { return false; }
    CodecId getCodecId() const override { return CodecJPEG; }

    char *data() const override { return _buffer->data(); }
    size_t size() const override { return _buffer->size(); }

private:
    uint64_t _dts;
    toolkit::Buffer::Ptr _buffer;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_JPEG_H
