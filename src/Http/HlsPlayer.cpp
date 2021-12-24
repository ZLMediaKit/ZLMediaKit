/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HlsPlayer.h"

namespace mediakit {

HlsPlayer::HlsPlayer(const EventPoller::Ptr &poller) {
    _segment.setOnSegment([this](const char *data, size_t len) { onPacket(data, len); });
    setPoller(poller ? poller : EventPollerPool::Instance().getPoller());
}

HlsPlayer::~HlsPlayer() {}

void HlsPlayer::play(const string &strUrl) {
    _m3u8_list.emplace_back(strUrl);
    play_l();
}

void HlsPlayer::play_l() {
    if (_m3u8_list.empty()) {
        teardown_l(SockException(Err_shutdown, "所有hls url都尝试播放失败!"));
        return;
    }
    if (waitResponse()) {
        return;
    }
    float playTimeOutSec = (*this)[Client::kTimeoutMS].as<int>() / 1000.0f;
    setMethod("GET");
    if (!(*this)[kNetAdapter].empty()) {
        setNetAdapter((*this)[kNetAdapter]);
    }
    sendRequest(_m3u8_list.back(), playTimeOutSec);
}

void HlsPlayer::teardown_l(const SockException &ex) {
    _timer.reset();
    _timer_ts.reset();
    _http_ts_player.reset();
    shutdown(ex);
}

void HlsPlayer::teardown() {
    teardown_l(SockException(Err_shutdown, "teardown"));
}

void HlsPlayer::playNextTs() {
    if (_ts_list.empty()) {
        //播放列表为空，那么立即重新下载m3u8文件
        _timer.reset();
        play_l();
        return;
    }
    if (_http_ts_player && _http_ts_player->waitResponse()) {
        //播放器目前还存活，正在下载中
        return;
    }
    weak_ptr<HlsPlayer> weak_self = dynamic_pointer_cast<HlsPlayer>(shared_from_this());
    if (!_http_ts_player) {
        _http_ts_player = std::make_shared<HttpTSPlayer>(getPoller(), false);
        _http_ts_player->setOnCreateSocket([weak_self](const EventPoller::Ptr &poller) {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                return strong_self->createSocket();
            }
            return Socket::createSocket(poller, true);
        });

        _http_ts_player->setOnPacket([weak_self](const char *data, size_t len) {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            //收到ts包
            strong_self->onPacket_l(data, len);
        });

        if (!(*this)[kNetAdapter].empty()) {
            _http_ts_player->setNetAdapter((*this)[Client::kNetAdapter]);
        }
    }

    Ticker ticker;
    auto url = _ts_list.front().url;
    auto duration = _ts_list.front().duration;
    _ts_list.pop_front();

    _http_ts_player->setOnComplete([weak_self, ticker, duration, url](const SockException &err) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        if (err) {
            WarnL << "download ts segment " << url << " failed:" << err.what();
        }
        //提前半秒下载好
        auto delay = duration - ticker.elapsedTime() / 1000.0f - 0.5;
        if (delay <= 0) {
            //延时最小10ms
            delay = 10;
        }
        //延时下载下一个切片
        strong_self->_timer_ts.reset(new Timer(delay / 1000.0f, [weak_self]() {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                strong_self->playNextTs();
            }
            return false;
        }, strong_self->getPoller()));
    });

    _http_ts_player->setMethod("GET");
    _http_ts_player->sendRequest(url, 2 * duration);
}

void HlsPlayer::onParsed(bool is_m3u8_inner, int64_t sequence, const map<int, ts_segment> &ts_map) {
    if (!is_m3u8_inner) {
        //这是ts播放列表
        if (_last_sequence == sequence) {
            return;
        }
        _last_sequence = sequence;
        for (auto &pr : ts_map) {
            auto &ts = pr.second;
            if (_ts_url_cache.emplace(ts.url).second) {
                //该ts未重复
                _ts_list.emplace_back(ts);
                //按时间排序
                _ts_url_sort.emplace_back(ts.url);
            }
        }
        if (_ts_url_sort.size() > 2 * ts_map.size()) {
            //去除防重列表中过多的数据
            _ts_url_cache.erase(_ts_url_sort.front());
            _ts_url_sort.pop_front();
        }
        playNextTs();
    } else {
        //这是m3u8列表,我们播放最高清的子hls
        if (ts_map.empty()) {
            teardown_l(SockException(Err_shutdown, StrPrinter << "empty sub hls list:" + getUrl()));
            return;
        }
        _timer.reset();

        weak_ptr<HlsPlayer> weak_self = dynamic_pointer_cast<HlsPlayer>(shared_from_this());
        auto url = ts_map.rbegin()->second.url;
        getPoller()->async([weak_self, url]() {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                strong_self->play(url);
            }
        }, false);
    }
}

ssize_t HlsPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &headers) {
    if (status != "200" && status != "206") {
        //失败
        teardown_l(SockException(Err_shutdown, StrPrinter << "bad http status code:" + status));
        return 0;
    }
    auto content_type = const_cast< HttpClient::HttpHeader &>(headers)["Content-Type"];
    _is_m3u8 = (content_type.find("application/vnd.apple.mpegurl") == 0);
    return -1;
}

void HlsPlayer::onResponseBody(const char *buf, size_t size, size_t recvedSize, size_t totalSize) {
    if (recvedSize == size) {
        //刚开始
        _m3u8.clear();
    }
    _m3u8.append(buf, size);
}

void HlsPlayer::onResponseCompleted() {
    if (HlsParser::parse(getUrl(), _m3u8)) {
        if (_first) {
            _first = false;
            onPlayResult(SockException(Err_success, "play success"));
        }
        playDelay();
    } else {
        teardown_l(SockException(Err_shutdown, "解析m3u8文件失败"));
    }
}

float HlsPlayer::delaySecond() {
    if (HlsParser::isM3u8() && HlsParser::getTargetDur() > 0) {
        float targetOffset;
        if (HlsParser::isLive()) {
            // see RFC 8216, Section 4.4.3.8.
            // 根据rfc刷新index列表的周期应该是分段时间x3, 因为根据规范播放器只处理最后3个Segment
            targetOffset = (float) (3 * HlsParser::getTargetDur());
        } else {
            // 点播则一般m3u8文件不会在改变了, 没必要频繁的刷新, 所以按照总时间来进行刷新
            targetOffset = HlsParser::getTotalDuration();
        }
        // 取最小值, 避免因为分段时长不规则而导致的问题
        if (targetOffset > HlsParser::getTotalDuration()) {
            targetOffset = HlsParser::getTotalDuration();
        }
        // 根据规范为一半的时间
        if (targetOffset / 2 > 1.0f) {
            return targetOffset / 2;
        }
    }
    return 1.0f;
}

void HlsPlayer::onDisconnect(const SockException &ex) {
    if (_first) {
        //第一次失败，则播放失败
        _first = false;
        onPlayResult(ex);
        return;
    }

    //主动shutdown
    if (ex.getErrCode() == Err_shutdown) {
        if (_m3u8_list.size() <= 1) {
            //全部url都播放失败
            _timer = nullptr;
            _timer_ts = nullptr;
            onShutdown(ex);
        } else {
            _m3u8_list.pop_back();
            //还有上一级url可以重试播放
            play_l();
        }
        return;
    }

    //eof等，之后播放失败，那么重试播放m3u8
    playDelay();
}

bool HlsPlayer::onRedirectUrl(const string &url, bool temporary) {
    _m3u8_list.emplace_back(url);
    return true;
}

void HlsPlayer::playDelay() {
    weak_ptr<HlsPlayer> weak_self = dynamic_pointer_cast<HlsPlayer>(shared_from_this());
    _timer.reset(new Timer(delaySecond(), [weak_self]() {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->play_l();
        }
        return false;
    }, getPoller()));
}

void HlsPlayer::onPacket_l(const char *data, size_t len) {
    try {
        _segment.input(data, len);
    } catch (...) {
        //ts解析失败，清空缓存数据
        _segment.reset();
        throw;
    }
}

//////////////////////////////////////////////////////////////////////////

class HlsDemuxer : public MediaSinkInterface, public TrackSource, public std::enable_shared_from_this<HlsDemuxer> {
public:
    HlsDemuxer() = default;
    ~HlsDemuxer() override { _timer = nullptr; }

    void start(const EventPoller::Ptr &poller, TrackListener *listener);

    bool inputFrame(const Frame::Ptr &frame) override;

    bool addTrack(const Track::Ptr &track) override {
        return _delegate.addTrack(track);
    }

    void addTrackCompleted() override {
        _delegate.addTrackCompleted();
    }

    void resetTracks() override {
        ((MediaSink &) _delegate).resetTracks();
    }

    vector<Track::Ptr> getTracks(bool ready = true) const override {
        return _delegate.getTracks(ready);
    }

private:
    void onTick();
    int64_t getBufferMS();
    int64_t getPlayPosition();
    void setPlayPosition(int64_t pos);

private:
    int64_t _ticker_offset = 0;
    Ticker _ticker;
    Stamp _stamp[2];
    Timer::Ptr _timer;
    MediaSinkDelegate _delegate;
    multimap<int64_t, Frame::Ptr> _frame_cache;
};

void HlsDemuxer::start(const EventPoller::Ptr &poller, TrackListener *listener) {
    _frame_cache.clear();
    _stamp[TrackAudio].setRelativeStamp(0);
    _stamp[TrackVideo].setRelativeStamp(0);
    _stamp[TrackAudio].syncTo(_stamp[TrackVideo]);
    setPlayPosition(0);

    _delegate.setTrackListener(listener);

    //每50毫秒执行一次
    weak_ptr<HlsDemuxer> weak_self = shared_from_this();
    _timer = std::make_shared<Timer>(0.05f, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onTick();
        return true;
    }, poller);
}

bool HlsDemuxer::inputFrame(const Frame::Ptr &frame) {
    //为了避免track准备时间过长, 因此在没准备好之前, 直接消费掉所有的帧
    if (!_delegate.isAllTrackReady()) {
        _delegate.inputFrame(frame);
        return true;
    }

    //计算相对时间戳
    int64_t dts, pts;
    _stamp[frame->getTrackType()].revise(frame->dts(), frame->pts(), dts, pts);
    //根据时间戳缓存frame
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

int64_t HlsDemuxer::getPlayPosition() {
    return _ticker.elapsedTime() + _ticker_offset;
}

int64_t HlsDemuxer::getBufferMS() {
    if (_frame_cache.empty()) {
        return 0;
    }
    return _frame_cache.rbegin()->first - _frame_cache.begin()->first;
}

void HlsDemuxer::setPlayPosition(int64_t pos) {
    _ticker.resetTime();
    _ticker_offset = pos;
}

void HlsDemuxer::onTick() {
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

HlsPlayerImp::HlsPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<HlsPlayer, PlayerBase>(poller) {}

void HlsPlayerImp::onPacket(const char *data, size_t len) {
    if (!_decoder) {
        _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, _demuxer.get());
    }

    if (_decoder && _demuxer) {
        _decoder->input((uint8_t *) data, len);
    }
}

void HlsPlayerImp::addTrackCompleted() {
    PlayerImp<HlsPlayer, PlayerBase>::onPlayResult(SockException(Err_success, "play hls success"));
}

void HlsPlayerImp::onPlayResult(const SockException &ex) {
    if (ex) {
        PlayerImp<HlsPlayer, PlayerBase>::onPlayResult(ex);
    } else {
        auto demuxer = std::make_shared<HlsDemuxer>();
        demuxer->start(getPoller(), this);
        _demuxer = std::move(demuxer);
    }
}

void HlsPlayerImp::onShutdown(const SockException &ex) {
    WarnL << ex.what();
    PlayerImp<HlsPlayer, PlayerBase>::onShutdown(ex);
    _demuxer = nullptr;
}

vector<Track::Ptr> HlsPlayerImp::getTracks(bool ready) const {
    return static_pointer_cast<HlsDemuxer>(_demuxer)->getTracks(ready);
}

}//namespace mediakit
