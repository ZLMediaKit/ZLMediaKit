/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 * Created by alex on 2021/4/6.
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "TsPlayerImp.h"

namespace mediakit {

void TsDemuxer::start(const EventPoller::Ptr &poller, TrackListener *listener) {
    _frame_cache.clear();
    _stamp[TrackAudio].setRelativeStamp(0);
    _stamp[TrackVideo].setRelativeStamp(0);
    _stamp[TrackAudio].syncTo(_stamp[TrackVideo]);
    setPlayPosition(0);

    _delegate.setTrackListener(listener);

    //每50毫秒执行一次
    weak_ptr <TsDemuxer> weak_self = shared_from_this();
    _timer = std::make_shared<Timer>(0.05f, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onTick();
        return true;
    }, poller);
}

bool TsDemuxer::inputFrame(const Frame::Ptr &frame) {
    //为了避免track准备时间过长, 因此在没准备好之前, 直接消费掉所有的帧
    if (!_delegate.isAllTrackReady()) {
        _delegate.inputFrame(frame);
        return true;
    }
    //计算相对时间戳
    int64_t dts, pts;
    //根据时间戳缓存frame
    _stamp[frame->getTrackType()].revise(frame->dts(), frame->pts(), dts, pts);
    _frame_cache.emplace(dts, Frame::getCacheAbleFrame(frame));

    if (getBufferMS() > 30 * 1000) {
        //缓存超过30秒，强制消费至15秒(减少延时或内存占用)
        while (getBufferMS() > 15 * 1000) {
            _delegate.inputFrame(_frame_cache.begin()->second);
            _frame_cache.erase(_frame_cache.begin());
        }
        //接着播放缓存中最早的帧
        setPlayPosition(_frame_cache.begin()->first);
    }
    return true;
}

int64_t TsDemuxer::getPlayPosition() {
    return _ticker.elapsedTime() + _ticker_offset;
}

int64_t TsDemuxer::getBufferMS() {
    if (_frame_cache.empty()) {
        return 0;
    }
    return _frame_cache.rbegin()->first - _frame_cache.begin()->first;
}

void TsDemuxer::setPlayPosition(int64_t pos) {
    _ticker.resetTime();
    _ticker_offset = pos;
}

void TsDemuxer::onTick() {
    auto it = _frame_cache.begin();
    while (it != _frame_cache.end()) {
        if (it->first > getPlayPosition()) {
            //这些帧还未到时间播放
            break;
        }
        if (getBufferMS() < 3 * 1000) {
            //缓存小于3秒,那么降低定时器消费速度(让剩余的数据在3秒后消费完毕)
            //目的是为了防止定时器长时间干等后，数据瞬间消费完毕
            setPlayPosition(_frame_cache.begin()->first);
        }
        //消费掉已经到期的帧
        _delegate.inputFrame(it->second);
        it = _frame_cache.erase(it);
    }
}

//////////////////////////////////////////////////////////////////////////

TsPlayerImp::TsPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<TsPlayer, PlayerBase>(poller) {}

void TsPlayerImp::onPacket(const char *data, size_t len) {
    if (!_decoder) {
        _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, _demuxer.get());
    }

    if (_decoder && _demuxer) {
        _decoder->input((uint8_t *) data, len);
    }
}

void TsPlayerImp::addTrackCompleted() {
    PlayerImp<TsPlayer, PlayerBase>::onPlayResult(SockException(Err_success, "play hls success"));
}

void TsPlayerImp::onPlayResult(const SockException &ex) {
    WarnL << ex.getErrCode() << " " << ex.what();
    if (ex) {
        PlayerImp<TsPlayer, PlayerBase>::onPlayResult(ex);
    } else {
        auto demuxer = std::make_shared<TsDemuxer>();
        demuxer->start(getPoller(), this);
        _demuxer = std::move(demuxer);
    }
}

void TsPlayerImp::onShutdown(const SockException &ex) {
    PlayerImp<TsPlayer, PlayerBase>::onShutdown(ex);
    _demuxer = nullptr;
}

vector <Track::Ptr> TsPlayerImp::getTracks(bool ready) const {
    return static_pointer_cast<TsDemuxer>(_demuxer)->getTracks(ready);
}

}//namespace mediakit