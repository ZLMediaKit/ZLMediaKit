#include "RtcMediaSource.h"
#include "Common/config.h"
#include "Codec/Transcode.h"
#include "Extension/AAC.h"
#include "Extension/Opus.h"
#include "Extension/G711.h"
// for RTC configure
#include "WebRtcTransport.h"
namespace mediakit {

bool needTransToOpus(CodecId codec) {
  GET_CONFIG(int, transG711, Rtc::kTranscodeG711);
  switch (codec)
  {
  case CodecG711U:
  case CodecG711A:
      return transG711;
  case CodecAAC:
      return true;
  default:
      return false;
  }
}

bool needTransToAac(CodecId codec) {
    GET_CONFIG(int, transG711, Rtc::kTranscodeG711);
    switch (codec)
    {
    case CodecG711U:
    case CodecG711A:
        return transG711;
    case CodecOpus:
        return true;
    default:
        return false;
    }
}

RtcMediaSourceMuxer::RtcMediaSourceMuxer(const std::string &vhost, const std::string &strApp, const std::string &strId, const ProtocolOption &option, const TitleSdp::Ptr &title) 
  : RtspMediaSourceMuxer(vhost, strApp, strId, option, title, RTC_SCHEMA)
{
  if (_option.audio_transcode) {
#ifndef ENABLE_FFMPEG
      WarnL << "without ffmpeg, skip transcode setting";
      _option.audio_transcode = false;
#endif
  }
}

bool RtcMediaSourceMuxer::inputFrame(const Frame::Ptr &frame)
{
  if (_clear_cache && _on_demand) {
    _clear_cache = false;
    _media_src->clearCache();
  }
  if (_enabled || !_on_demand) {
#if defined(ENABLE_FFMPEG)
    if (_option.audio_transcode && needTransToOpus(frame->getCodecId())) {
      if (!_audio_dec) { // addTrack可能没调, 这边根据情况再调一次
        Track::Ptr track;
        switch (frame->getCodecId())
        {
        case CodecAAC:
          if (frame->prefixSize()) {
            std::string cfg = makeAacConfig((uint8_t *)(frame->data()), frame->prefixSize());
            track = std::make_shared<AACTrack>(cfg);
          }
          else {
            track = std::make_shared<AACTrack>(44100, 2);
          }
          break;
        case CodecG711A:
        case CodecG711U:
          track.reset(new G711Track(frame->getCodecId()));
          break;
        default:
          break;
        }
        if (track)
          addTrack(track);
        if (!_audio_dec) return false;
      }
      if (readerCount()) {
        _audio_dec->inputFrame(frame, true, false);
        if (!_count)
          InfoL << "start transcode " << frame->getCodecName() << "," << frame->pts() << "->Opus";
        _count++;
      }
      else if (_count) {
        InfoL << "stop transcode with " << _count << " items";
        _count = 0;
      }
      return true;
    }
#endif
    return RtspMuxer::inputFrame(frame);
  }
  return false;
}

#if defined(ENABLE_FFMPEG)

bool RtcMediaSourceMuxer::addTrack(const Track::Ptr & track)
{
  Track::Ptr newTrack = track;
  if (_option.audio_transcode && needTransToOpus(track->getCodecId())) {
    newTrack = std::make_shared<OpusTrack>();
    GET_CONFIG(int, bitrate, General::kOpusBitrate);
    newTrack->setBitRate(bitrate);
    _audio_dec.reset(new FFmpegDecoder(track));
    _audio_enc.reset(new FFmpegEncoder(newTrack));
    // aac to opus
    _audio_dec->setOnDecode([this](const FFmpegFrame::Ptr & frame) {
      _audio_enc->inputFrame(frame, false);
    });
    _audio_enc->setOnEncode([this](const Frame::Ptr& frame) {
        RtspMuxer::inputFrame(frame);
    });
  }
  return RtspMuxer::addTrack(newTrack);
}


void RtcMediaSourceMuxer::resetTracks()
{
  RtspMuxer::resetTracks();
  _audio_dec = nullptr;
  _audio_enc = nullptr;
  if (_count) {
    InfoL << "stop transcode with " << _count << " items";
    _count = 0;
  }
}

bool RtcMediaSourceImp::addTrack(const Track::Ptr &track)
{
  if (_muxer) {
    Track::Ptr newTrack = track;
    if (_option.audio_transcode && needTransToAac(track->getCodecId())) {
      newTrack.reset(new AACTrack(44100, 2));
      GET_CONFIG(int, bitrate, General::kAacBitrate);
      newTrack->setBitRate(bitrate);
      _audio_dec.reset(new FFmpegDecoder(track));
      _audio_enc.reset(new FFmpegEncoder(newTrack));
      // hook data to newTack
      track->addDelegate([this](const Frame::Ptr &frame) -> bool {
        if (_all_track_ready && 0 == _muxer->totalReaderCount()) {
          if (_count) {
            InfoL << "stop transcode with " << _count << " items";
            _count = 0;
          }
          return true;
        }
        if (_audio_dec) {
          if (!_count)
            InfoL << "start transcode " << frame->getCodecName() << "," << frame->pts() << "->AAC";
          _count++;
          _audio_dec->inputFrame(frame, true, false);
        }
        return true;
      });
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

void RtcMediaSourceImp::resetTracks()
{
  RtspMediaSourceImp::resetTracks();
  _audio_dec = nullptr;
  _audio_enc = nullptr;
  if (_count) {
    InfoL << "stop transcode with " << _count << " items";
    _count = 0;
  }
}

#endif
}
