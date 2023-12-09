#include "RtmpDemuxer.h"
#include "RtmpMediaSourceImp.h"

namespace mediakit {

uint32_t RtmpMediaSource::getTimeStamp(TrackType trackType) {
    assert(trackType >= TrackInvalid && trackType < TrackMax);
    if (trackType != TrackInvalid) {
        // 获取某track的时间戳
        return _track_stamps[trackType];
    }

    // 获取所有track的最小时间戳
    uint32_t ret = UINT32_MAX;
    for (auto &stamp : _track_stamps) {
        if (stamp > 0 && stamp < ret) {
            ret = stamp;
        }
    }
    return ret;
}

void RtmpMediaSource::setMetaData(const AMFValue &metadata) {
    {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        _metadata = metadata;
        _metadata.set("title", std::string("Streamed by ") + kServerName);
    }

    _have_video = _metadata["videocodecid"];
    _have_audio = _metadata["audiocodecid"];
    if (_ring) {
        regist();

        AMFEncoder enc;
        enc << "onMetaData" << _metadata;
        RtmpPacket::Ptr packet = RtmpPacket::create();
        packet->buffer = enc.data();
        packet->type_id = MSG_DATA;
        packet->time_stamp = 0;
        packet->chunk_id = CHUNK_CLIENT_REQUEST_AFTER;
        packet->stream_index = STREAM_MEDIA;
        onWrite(std::move(packet));
    }
}

void RtmpMediaSource::onWrite(RtmpPacket::Ptr pkt, bool /*= true*/) {
    bool is_video = pkt->type_id == MSG_VIDEO;
    _speed[is_video ? TrackVideo : TrackAudio] += pkt->size();
    // 保存当前时间戳
    switch (pkt->type_id) {
        case MSG_VIDEO: _track_stamps[TrackVideo] = pkt->time_stamp, _have_video = true; break;
        case MSG_AUDIO: _track_stamps[TrackAudio] = pkt->time_stamp, _have_audio = true; break;
        default: break;
    }

    if (pkt->isConfigFrame()) {
        std::lock_guard<std::recursive_mutex> lock(_mtx);
        _config_frame_map[pkt->type_id] = pkt;
        if (!_ring) {
            // 注册后收到config帧更新到各播放器
            return;
        }
    }

    if (!_ring) {
        std::weak_ptr<RtmpMediaSource> weak_self = std::static_pointer_cast<RtmpMediaSource>(shared_from_this());
        auto lam = [weak_self](int size) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            strong_self->onReaderChanged(size);
        };

        // GOP默认缓冲512组RTMP包，每组RTMP包时间戳相同(如果开启合并写了，那么每组为合并写时间内的RTMP包),
        // 每次遇到关键帧第一个RTMP包，则会清空GOP缓存(因为有新的关键帧了，同样可以实现秒开)
        _ring = std::make_shared<RingType>(_ring_size, std::move(lam));
        if (_metadata) {
            regist();
        }
    }
    bool key = pkt->isVideoKeyFrame();
    auto stamp = pkt->time_stamp;
    PacketCache<RtmpPacket>::inputPacket(stamp, is_video, std::move(pkt), key);
}

RtmpMediaSourceImp::RtmpMediaSourceImp(const MediaTuple &tuple, int ringSize)
    : RtmpMediaSource(tuple, ringSize) {
    _demuxer = std::make_shared<RtmpDemuxer>();
    _demuxer->setTrackListener(this);
}

void RtmpMediaSourceImp::setMetaData(const AMFValue &metadata) {
    if (!_demuxer->loadMetaData(metadata)) {
        // 该metadata无效，需要重新生成
        _metadata = metadata;
        _recreate_metadata = true;
    }
    RtmpMediaSource::setMetaData(metadata);
}

void RtmpMediaSourceImp::onWrite(RtmpPacket::Ptr pkt, bool /*= true*/) {
    if (!_all_track_ready || _muxer->isEnabled()) {
        // 未获取到所有Track后，或者开启转协议，那么需要解复用rtmp
        _demuxer->inputRtmp(pkt);
    }
    GET_CONFIG(bool, directProxy, Rtmp::kDirectProxy);
    if (directProxy) {
        //直接代理模式才直接使用原始rtmp
        RtmpMediaSource::onWrite(std::move(pkt));
    }
}

int RtmpMediaSourceImp::totalReaderCount() {
    return readerCount() + (_muxer ? _muxer->totalReaderCount() : 0);
}

void RtmpMediaSourceImp::setProtocolOption(const ProtocolOption &option) {
    GET_CONFIG(bool, direct_proxy, Rtmp::kDirectProxy);
    _option = option;
    _option.enable_rtmp = !direct_proxy;
    _muxer = std::make_shared<MultiMediaSourceMuxer>(_tuple, _demuxer->getDuration(), _option);
    _muxer->setMediaListener(getListener());
    _muxer->setTrackListener(std::static_pointer_cast<RtmpMediaSourceImp>(shared_from_this()));
    // 让_muxer对象拦截一部分事件(比如说录像相关事件)
    MediaSource::setListener(_muxer);

    for (auto &track : _demuxer->getTracks(false)) {
        _muxer->addTrack(track);
        track->addDelegate(_muxer);
    }
}

bool RtmpMediaSourceImp::addTrack(const Track::Ptr &track) {
    if (_muxer) {
        if (_muxer->addTrack(track)) {
            track->addDelegate(_muxer);
            return true;
        }
    }
    return false;
}

void RtmpMediaSourceImp::addTrackCompleted() {
    if (_muxer) {
        _muxer->addTrackCompleted();
    }
}

void RtmpMediaSourceImp::resetTracks() {
    if (_muxer) {
        _muxer->resetTracks();
    }
}

void RtmpMediaSourceImp::onAllTrackReady() {
    _all_track_ready = true;

    if (_recreate_metadata) {
        // 更新metadata
        for (auto &track : _muxer->getTracks()) {
            Metadata::addTrack(_metadata, track);
        }
        RtmpMediaSource::setMetaData(_metadata);
    }
}

void RtmpMediaSourceImp::setListener(const std::weak_ptr<MediaSourceEvent> &listener) {
    if (_muxer) {
        //_muxer对象不能处理的事件再给listener处理
        _muxer->setMediaListener(listener);
    } else {
        // 未创建_muxer对象，事件全部给listener处理
        MediaSource::setListener(listener);
    }
}

} // namespace mediakit
