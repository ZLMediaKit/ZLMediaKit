#include "RtspMediaSourceImp.h"
#include "RtspDemuxer.h"
#include "Common/config.h"
namespace mediakit {
void RtspMediaSource::setSdp(const std::string &sdp) {
    SdpParser sdp_parser(sdp);
    _tracks[TrackVideo] = sdp_parser.getTrack(TrackVideo);
    _tracks[TrackAudio] = sdp_parser.getTrack(TrackAudio);
    _have_video = (bool)_tracks[TrackVideo];
    _sdp = sdp_parser.toString();
    if (_ring) {
        regist();
    }
}

uint32_t RtspMediaSource::getTimeStamp(TrackType trackType) {
    assert(trackType >= TrackInvalid && trackType < TrackMax);
    if (trackType != TrackInvalid) {
        //获取某track的时间戳
        auto &track = _tracks[trackType];
        if (track) {
            return track->_time_stamp;
        }
    }

    //获取所有track的最小时间戳
    uint32_t ret = UINT32_MAX;
    for (auto &track : _tracks) {
        if (track && track->_time_stamp < ret) {
            ret = track->_time_stamp;
        }
    }
    return ret;
}

/**
    * 更新时间戳
    */
void RtspMediaSource::setTimeStamp(uint32_t stamp) {
    for (auto &track : _tracks) {
        if (track) {
            track->_time_stamp = stamp;
        }
    }
}

void RtspMediaSource::onWrite(RtpPacket::Ptr rtp, bool keyPos) {
    _speed[rtp->type] += rtp->size();
    assert(rtp->type >= 0 && rtp->type < TrackMax);
    auto &track = _tracks[rtp->type];
    auto stamp = rtp->getStampMS();
    if (track) {
        track->_seq = rtp->getSeq();
        track->_time_stamp = rtp->getStamp() * uint64_t(1000) / rtp->sample_rate;
        track->_ssrc = rtp->getSSRC();
    }
    if (!_ring) {
        std::weak_ptr<RtspMediaSource> weakSelf = std::static_pointer_cast<RtspMediaSource>(shared_from_this());
        auto lam = [weakSelf](int size) {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return;
            }
            strongSelf->onReaderChanged(size);
        };
        //GOP默认缓冲512组RTP包，每组RTP包时间戳相同(如果开启合并写了，那么每组为合并写时间内的RTP包),
        //每次遇到关键帧第一个RTP包，则会清空GOP缓存(因为有新的关键帧了，同样可以实现秒开)
        _ring = std::make_shared<RingType>(_ring_size, std::move(lam));
        if (!_sdp.empty()) {
            regist();
        }
    }
    bool is_video = rtp->type == TrackVideo;
    PacketCache<RtpPacket>::inputPacket(stamp, is_video, std::move(rtp), keyPos);
}

RtspMediaSourceImp::RtspMediaSourceImp(const std::string &vhost, const std::string &app, const std::string &id, int ringSize) 
    : RtspMediaSource(vhost, app, id, ringSize)
{
    _demuxer = std::make_shared<RtspDemuxer>();
    _demuxer->setTrackListener(this);
}

void RtspMediaSourceImp::setSdp(const std::string &strSdp)
{
    if (!getSdp().empty()) {
        return;
    }
    _demuxer->loadSdp(strSdp);
    RtspMediaSource::setSdp(strSdp);
}

void RtspMediaSourceImp::onWrite(RtpPacket::Ptr rtp, bool key_pos)
{
    if (_all_track_ready && !_muxer->isEnabled()) {
        //获取到所有Track后，并且未开启转协议，那么不需要解复用rtp
        //在关闭rtp解复用后，无法知道是否为关键帧，这样会导致无法秒开，或者开播花屏
        key_pos = rtp->type == TrackVideo;
    } else {
        //需要解复用rtp
        key_pos = _demuxer->inputRtp(rtp);
    }
    GET_CONFIG(bool, directProxy, Rtsp::kDirectProxy);
    if (directProxy) {
        //直接代理模式才直接使用原始rtp
        RtspMediaSource::onWrite(std::move(rtp), key_pos);
    }
}

void RtspMediaSourceImp::setProtocolOption(const ProtocolOption &option)
{
    GET_CONFIG(bool, direct_proxy, Rtsp::kDirectProxy);
    //开启直接代理模式时，rtsp直接代理，不重复产生；但是有些rtsp推流端，由于sdp中已有sps pps，rtp中就不再包括sps pps,
    //导致rtc无法播放，所以在rtsp推流rtc播放时，建议关闭直接代理模式
    _option = option;
    _option.enable_rtsp = !direct_proxy;
    _muxer = std::make_shared<MultiMediaSourceMuxer>(getVhost(), getApp(), getId(), _demuxer->getDuration(), _option);
    _muxer->setMediaListener(getListener());
    _muxer->setTrackListener(std::static_pointer_cast<RtspMediaSourceImp>(shared_from_this()));
    //让_muxer对象拦截一部分事件(比如说录像相关事件)
    MediaSource::setListener(_muxer);

    for (auto &track : _demuxer->getTracks(false)) {
        _muxer->addTrack(track);
        track->addDelegate(_muxer);
    }
}


}

