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

    RtcMediaSourceImp(const MediaTuple& tuple, int ringSize = RTP_GOP_SIZE)
        : RtspMediaSourceImp(tuple, RTC_SCHEMA, ringSize) {
    }

    RtspMediaSource::Ptr clone(const std::string &stream);
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

    RtcMediaSourceMuxer( const MediaTuple& tuple,
                         const ProtocolOption &option,
                         const TitleSdp::Ptr &title = nullptr);


    bool inputFrame(const Frame::Ptr &frame) override;

#if defined(ENABLE_FFMPEG)
    ~RtcMediaSourceMuxer() override{resetTracks();}

    void onRegist(MediaSource &sender, bool regist) override;
    bool addTrack(const Track::Ptr & track) override;
    void resetTracks() override;

private:
    int _count = 0;
    bool _regist = false;
    std::shared_ptr<FFmpegDecoder> _audio_dec;
    std::shared_ptr<FFmpegEncoder> _audio_enc;
#endif
};

}
#endif