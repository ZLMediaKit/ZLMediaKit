/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include "PlayerBase.h"
#include "Rtsp/RtspPlayerImp.h"
#include "Rtmp/RtmpPlayerImp.h"
#include "Http/HlsPlayer.h"

using namespace toolkit;

namespace mediakit {

PlayerBase::Ptr PlayerBase::createPlayer(const EventPoller::Ptr &poller, const string &url_in) {
    static auto releasePlayer = [](PlayerBase *ptr) {
        onceToken token(nullptr, [&]() {
            delete ptr;
        });
        ptr->teardown();
    };
    string url = url_in;
    string prefix = FindField(url.data(), NULL, "://");
    auto pos = url.find('?');
    if (pos != string::npos) {
        //去除？后面的字符串
        url = url.substr(0, pos);
    }

    if (strcasecmp("rtsps", prefix.data()) == 0) {
        return PlayerBase::Ptr(new TcpClientWithSSL<RtspPlayerImp>(poller), releasePlayer);
    }

    if (strcasecmp("rtsp", prefix.data()) == 0) {
        return PlayerBase::Ptr(new RtspPlayerImp(poller), releasePlayer);
    }

    if (strcasecmp("rtmps", prefix.data()) == 0) {
        return PlayerBase::Ptr(new TcpClientWithSSL<RtmpPlayerImp>(poller), releasePlayer);
    }

    if (strcasecmp("rtmp", prefix.data()) == 0) {
        return PlayerBase::Ptr(new RtmpPlayerImp(poller), releasePlayer);
    }

    if ((strcasecmp("http", prefix.data()) == 0 || strcasecmp("https", prefix.data()) == 0) && end_with(url, ".m3u8")) {
        return PlayerBase::Ptr(new HlsPlayerImp(poller), releasePlayer);
    }

    return PlayerBase::Ptr(new RtspPlayerImp(poller), releasePlayer);
}

PlayerBase::PlayerBase() {
    this->mINI::operator[](kTimeoutMS) = 10000;
    this->mINI::operator[](kMediaTimeoutMS) = 5000;
    this->mINI::operator[](kBeatIntervalMS) = 5000;
    this->mINI::operator[](kWaitTrackReady) = true;
}

///////////////////////////DemuxerSink//////////////////////////////

class DemuxerSink : public MediaSink {
public:
    DemuxerSink() = default;
    ~DemuxerSink() override = default;

    /**
     * 设置track监听器
     */
    void setTrackListener(TrackListener *listener);
    vector<Track::Ptr> getTracks(bool ready = true) const override;

protected:
    bool addTrack(const Track::Ptr & track) override;
    void resetTracks() override;
    bool onTrackReady(const Track::Ptr & track) override;
    void onAllTrackReady() override;
    bool onTrackFrame(const Frame::Ptr &frame) override;

private:
    Track::Ptr _tracks[TrackMax];
    TrackListener *_listener = nullptr;
};

bool DemuxerSink::addTrack(const Track::Ptr &track) {
    auto ret = MediaSink::addTrack(track);
    if (ret) {
        track->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([this](const Frame::Ptr &frame) {
            return inputFrame(frame);
        }));
    }
    return ret;
}

void DemuxerSink::setTrackListener(TrackListener *listener) {
    _listener = listener;
}

bool DemuxerSink::onTrackReady(const Track::Ptr &track) {
    _tracks[track->getTrackType()] = track->clone();
    return true;
}

void DemuxerSink::onAllTrackReady() {
    if (!_listener) {
        return;
    }
    for (auto &track : _tracks) {
        if (track) {
            _listener->addTrack(track);
        }
    }
    _listener->addTrackCompleted();
}

bool DemuxerSink::onTrackFrame(const Frame::Ptr &frame) {
    return _tracks[frame->getTrackType()]->inputFrame(frame);
}

void DemuxerSink::resetTracks() {
    MediaSink::resetTracks();
    for (auto &track : _tracks) {
        track = nullptr;
    }
    if (_listener) {
        _listener->resetTracks();
    }
}

vector<Track::Ptr> DemuxerSink::getTracks(bool ready) const {
    vector<Track::Ptr> ret;
    for (auto &track : _tracks) {
        if (!track || (ready && !track->ready())) {
            continue;
        }
        ret.emplace_back(track);
    }
    return ret;
}

///////////////////////////Demuxer//////////////////////////////

void Demuxer::setTrackListener(TrackListener *listener, bool wait_track_ready) {
    if (wait_track_ready) {
        auto sink = std::make_shared<DemuxerSink>();
        sink->setTrackListener(listener);
        _sink = std::move(sink);
    }
    _listener = listener;
}

bool Demuxer::addTrack(const Track::Ptr &track) {
    if (!_sink) {
        _origin_track.emplace_back(track);
    }
    return _sink ? _sink->addTrack(track) : (_listener ? _listener->addTrack(track) : false);
}

void Demuxer::addTrackCompleted() {
    if (_sink) {
        _sink->addTrackCompleted();
    } else if (_listener) {
        _listener->addTrackCompleted();
    }
}

void Demuxer::resetTracks() {
    if (_sink) {
        _sink->resetTracks();
    } else if (_listener) {
        _listener->resetTracks();
    }
}

vector<Track::Ptr> Demuxer::getTracks(bool ready) const {
    if (_sink) {
        return _sink->getTracks(ready);
    }

    vector<Track::Ptr> ret;
    for (auto &track : _origin_track) {
        if (ready && !track->ready()) {
            continue;
        }
        ret.emplace_back(track);
    }
    return ret;
}

} /* namespace mediakit */
