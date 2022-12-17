#pagma once

#include "Frame.h"
#include "Track.h"

namespace mediakit {

class JPEGTrack : public VideoTrack {
public:
  using Ptr = std::shared_ptr<H264Track>;
  using VideoTrack::VideoTrack;
  CodecId getCodecId() const override { return CodecJPEG; }
  int getVideoHeight() const override { return _height; }
  int getVideoWidth() const override { return _width; }
  float getVideoFps() const override { return _fps; }
  bool ready() override { return true; }
  bool inputFrame(const Frame::Ptr &frame) override { return VideoTrack::inputFrame(frame); }

private:
  Sdp::Ptr getSdp() override { return std::make_shared<MJPEGSdp>(getBitRate() / 1024); };
  Track::Ptr clone() override {
    return std::make_shared<std::remove_reference<decltype(*this)>::type>(*this);
  }

private:
  int _width = 0;
  int _height = 0;
  float _fps  = 0;
};

}//namespace mediakit
