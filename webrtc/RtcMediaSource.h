#ifndef ZLMEDIAKIT_RTCMEDIASOURCE_H
#define ZLMEDIAKIT_RTCMEDIASOURCE_H

#include "Rtsp/RtspMediaSourceMuxer.h"
#include "Rtsp/RtspMediaSourceImp.h"
#include "Codec/Transcode.h"
#include "Extension/AAC.h"
#include "Extension/Opus.h"

namespace mediakit {

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
    bool addTrack(const Track::Ptr &track) override {
        if (_muxer) {
            Track::Ptr newTrack = track;
            if (_option.transcode_rtc_audio && track->getCodecId() == CodecOpus) {
                newTrack.reset(new AACTrack(44100, 2));
                _audio_dec.reset(new FFmpegDecoder(track));
                _audio_enc.reset(new FFmpegEncoder(newTrack));
                // hook data to newTack
                track->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([this](const Frame::Ptr &frame) -> bool {
                    if (_all_track_ready && 0 == _muxer->totalReaderCount()) 
                        return true;
                    if (_audio_dec)
                        _audio_dec->inputFrame(frame, true, false);
                    return true;
                }));
                _audio_dec->setOnDecode([this](const FFmpegFrame::Ptr & frame) {
                    _audio_enc->inputFrame(frame, false);
                });
                _audio_enc->setOnEncode([newTrack](const Frame::Ptr& frame) {
                    newTrack->inputFrame(frame);
                });
            }

            if (_muxer->addTrack(newTrack)) {
                newTrack->addDelegate(_muxer);
                return true;
            }
        }
        return false;
    }

    void resetTracks() override {
        RtspMediaSourceImp::resetTracks();
        _audio_dec = nullptr;
        _audio_enc = nullptr;
    }
private:
    FFmpegDecoder::Ptr _audio_dec;
    FFmpegEncoder::Ptr _audio_enc;
#endif
};

class RtcMediaSourceMuxer : public RtspMediaSourceMuxer {
public:
    typedef std::shared_ptr<RtcMediaSourceMuxer> Ptr;

    RtcMediaSourceMuxer(const std::string &vhost,
                         const std::string &strApp,
                         const std::string &strId,
                         const TitleSdp::Ptr &title = nullptr,
                         bool transcode = false) : RtspMediaSourceMuxer(vhost, strApp, strId, title, RTC_SCHEMA){
        _on_demand = ::toolkit::mINI::Instance()[RTC::kRtcDemand];
#if defined(ENABLE_FFMPEG)
        _transcode = transcode;
#else
        if(transcode) {
            WarnL << "without ffmpeg, skip transcode setting";
        }
#endif 
    }


    bool inputFrame(const Frame::Ptr &frame) override {
        if (_clear_cache && _on_demand) {
            _clear_cache = false;
            _media_src->clearCache();
        }
        if (_enabled || !_on_demand) {
#if defined(ENABLE_FFMPEG)
            if (_transcode && frame->getCodecId() == CodecAAC) {
                if (!_audio_dec) { // addTrack可能没调, 这边根据情况再调一次
                    Track::Ptr track;
                    if (frame->prefixSize()) {
                        std::string cfg = makeAacConfig((uint8_t *) (frame->data()), frame->prefixSize());
                        track = std::make_shared<AACTrack>(cfg);
                    }
                    else {
                        track = std::make_shared<AACTrack>(44100, 2);
                    }
                    addTrack(track);
                    if(!_audio_dec) return false;
                }
                if (readerCount()) {
                    _audio_dec->inputFrame(frame, true, false);
                }
                return true;
            }
#endif
            return RtspMuxer::inputFrame(frame);
        }
        return false;
    }

#if defined(ENABLE_FFMPEG)
    ~RtcMediaSourceMuxer() override{resetTracks();}

    bool addTrack(const Track::Ptr & track) override {
        Track::Ptr newTrack = track;
        if (_transcode && track->getCodecId() == CodecAAC) {
            newTrack = std::make_shared<OpusTrack>();
            _audio_dec.reset(new FFmpegDecoder(track));
            _audio_enc.reset(new FFmpegEncoder(newTrack));
            // aac to opus
            _audio_dec->setOnDecode([this](const FFmpegFrame::Ptr & frame) {
                _audio_enc->inputFrame(frame, false);
            });
            _audio_enc->setOnEncode([this](const Frame::Ptr& frame) {
                if (_enabled || !_on_demand) {
                    RtspMuxer::inputFrame(frame);
                }
            });
        }
        return RtspMuxer::addTrack(newTrack);
    }

    void resetTracks() override {
        RtspMuxer::resetTracks();
        _audio_dec = nullptr;
        _audio_enc = nullptr;
    }

private:
    bool _transcode = false;
    FFmpegDecoder::Ptr _audio_dec;
    FFmpegEncoder::Ptr _audio_enc;
#endif
};

}
#endif