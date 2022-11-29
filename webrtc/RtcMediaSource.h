#ifndef ZLMEDIAKIT_RTCMEDIASOURCE_H
#define ZLMEDIAKIT_RTCMEDIASOURCE_H

#include "Rtsp/RtspMediaSourceMuxer.h"
#include "Rtsp/RtspMediaSourceImp.h"
namespace mediakit {
class FFmpegDecoder;
class FFmpegEncoder;
bool needTransToOpus(CodecId codec);
bool needTransToAac(CodecId codec);

class RtcMediaSourceImp : public RtspMediaSourceImp {
public:
    typedef std::shared_ptr<RtcMediaSourceImp> Ptr;

    RtcMediaSourceImp(const std::string &vhost, const std::string &app, const std::string &id, int ringSize = RTP_GOP_SIZE)
        : RtspMediaSourceImp(vhost, app, id, RTC_SCHEMA, ringSize) {
    }
#if defined(ENABLE_FFMPEG)
    ~RtcMediaSourceImp() override { resetTracks(); }
    /**
     * _demuxer触发的添加Track事件
     */
    bool addTrack(const Track::Ptr &track) override;
    void resetTracks() override;
private:
    int _count = 0;
    std::shared_ptr<FFmpegDecoder> _audio_dec;
    std::shared_ptr<FFmpegEncoder> _audio_enc;
#endif
};

class RtcMediaSourceMuxer : public RtspMediaSourceMuxer {
public:
    typedef std::shared_ptr<RtcMediaSourceMuxer> Ptr;

    RtcMediaSourceMuxer(const std::string &vhost,
                         const std::string &strApp,
                         const std::string &strId,
                         const ProtocolOption &option,
                         const TitleSdp::Ptr &title = nullptr);


    bool inputFrame(const Frame::Ptr &frame) override;

#if defined(ENABLE_FFMPEG)
    ~RtcMediaSourceMuxer() override{resetTracks();}

    bool addTrack(const Track::Ptr & track) override;
    void resetTracks() override;

private:
    int _count = 0;
    std::shared_ptr<FFmpegDecoder> _audio_dec;
    std::shared_ptr<FFmpegEncoder> _audio_enc;
#endif
};

}
#endif