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

using namespace std;
using namespace toolkit;

namespace mediakit {

HlsPlayer::HlsPlayer(const EventPoller::Ptr &poller) {
    setPoller(poller ? poller : EventPollerPool::Instance().getPoller());
}

void HlsPlayer::play(const string &url) {
    _play_result = false;
    _play_url = url;
    fetchIndexFile();
}

void HlsPlayer::fetchIndexFile() {
    if (waitResponse()) {
        return;
    }
    if (!(*this)[Client::kNetAdapter].empty()) {
        setNetAdapter((*this)[Client::kNetAdapter]);
    }
    setCompleteTimeout((*this)[Client::kTimeoutMS].as<int>());
    setMethod("GET");
    sendRequest(_play_url);
}

void HlsPlayer::teardown_l(const SockException &ex) {
    if (!_play_result) {
        _play_result = true;
        onPlayResult(ex);
    } else {
        //如果不是主动关闭的，则重新拉取索引文件
        if (ex.getErrCode() != Err_shutdown) {
            // 当切片列表已空, 且没有正在下载的切片并且重试次数已经达到最大次数时, 则认为失败关闭播放器
            if (_ts_list.empty() && !(_http_ts_player && _http_ts_player->waitResponse())
                && _try_fetch_index_times >= MAX_TRY_FETCH_INDEX_TIMES) {
                onShutdown(ex);
            } else {
                _try_fetch_index_times += 1;
                shutdown(ex);
                WarnL << "重新尝试拉取索引文件[" << _try_fetch_index_times << "]:" << _play_url;
                fetchIndexFile();
                return;
            }
        } else {
            onShutdown(ex);
        }
    }
    _timer.reset();
    _timer_ts.reset();
    _http_ts_player.reset();
    shutdown(ex);
}

void HlsPlayer::teardown() {
    teardown_l(SockException(Err_shutdown, "teardown"));
}

void HlsPlayer::fetchSegment() {
    if (_ts_list.empty()) {
        //播放列表为空，那么立即重新下载m3u8文件
        _timer.reset();
        fetchIndexFile();
        return;
    }
    if (_http_ts_player && _http_ts_player->waitResponse()) {
        //播放器目前还存活，正在下载中
        return;
    }
    weak_ptr<HlsPlayer> weak_self = dynamic_pointer_cast<HlsPlayer>(shared_from_this());
    if (!_http_ts_player) {
        _http_ts_player = std::make_shared<HttpTSPlayer>(getPoller());
        _http_ts_player->setOnCreateSocket([weak_self](const EventPoller::Ptr &poller) {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                return strong_self->createSocket();
            }
            return Socket::createSocket(poller, true);
        });
        auto benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();
        if (!benchmark_mode) {
            _http_ts_player->setOnPacket([weak_self](const char *data, size_t len) {
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    return;
                }
                //收到ts包
                strong_self->onPacket(data, len);
            });
        }

        if (!(*this)[Client::kNetAdapter].empty()) {
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
            if (err.getErrCode() == Err_timeout) {
                strong_self->_timeout_multiple = MAX(strong_self->_timeout_multiple + 1, MAX_TIMEOUT_MULTIPLE);
            }else{
                strong_self->_timeout_multiple = MAX(strong_self->_timeout_multiple -1 , MIN_TIMEOUT_MULTIPLE);
            }
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
                strong_self->fetchSegment();
            }
            return false;
        }, strong_self->getPoller()));
    });

    _http_ts_player->setMethod("GET");
    //ts切片必须在其时长的2-5倍内下载完毕
    _http_ts_player->setCompleteTimeout(_timeout_multiple * duration * 1000);
    _http_ts_player->sendRequest(url);
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
        fetchSegment();
    } else {
        //这是m3u8列表,我们播放最高清的子hls
        if (ts_map.empty()) {
            throw invalid_argument("empty sub hls list:" + getUrl());
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

void HlsPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &headers) {
    if (status != "200" && status != "206") {
        //失败
        throw invalid_argument("bad http status code:" + status);
    }
    auto content_type = strToLower(const_cast<HttpClient::HttpHeader &>(headers)["Content-Type"]);
    if (content_type.find("application/vnd.apple.mpegurl") != 0 && content_type.find("/x-mpegurl") == _StrPrinter::npos) {
        WarnL << "may not a hls video: " << content_type << ", url: " << getUrl();
    }
    _m3u8.clear();
}

void HlsPlayer::onResponseBody(const char *buf, size_t size) {
    _m3u8.append(buf, size);
}

void HlsPlayer::onResponseCompleted(const SockException &ex) {
    if (ex) {
        teardown_l(ex);
        return;
    }
    if (!HlsParser::parse(getUrl(), _m3u8)) {
        teardown_l(SockException(Err_other, "parse m3u8 failed:" + _m3u8));
        return;
    }
    if (!_play_result) {
        _play_result = true;
        onPlayResult(SockException());
    }
    playDelay();
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

bool HlsPlayer::onRedirectUrl(const string &url, bool temporary) {
    _play_url = url;
    return true;
}

void HlsPlayer::playDelay() {
    weak_ptr<HlsPlayer> weak_self = dynamic_pointer_cast<HlsPlayer>(shared_from_this());
    _timer.reset(new Timer(delaySecond(), [weak_self]() {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->fetchIndexFile();
        }
        return false;
    }, getPoller()));
}

//////////////////////////////////////////////////////////////////////////

void HlsDemuxer::start(const EventPoller::Ptr &poller, TrackListener *listener) {
    _frame_cache.clear();
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

void HlsDemuxer::pushTask(std::function<void()> task) {
    int64_t stamp = 0;
    if (!_frame_cache.empty()) {
        stamp = _frame_cache.back().first;
    }
    _frame_cache.emplace_back(std::make_pair(stamp, std::move(task)));
}

bool HlsDemuxer::inputFrame(const Frame::Ptr &frame) {
    //为了避免track准备时间过长, 因此在没准备好之前, 直接消费掉所有的帧
    if (!_delegate.isAllTrackReady()) {
        _delegate.inputFrame(frame);
        return true;
    }

    if (_frame_cache.empty()) {
        //设置当前播放位置时间戳
        setPlayPosition(frame->dts());
    }
    //根据时间戳缓存frame
    auto cached_frame = Frame::getCacheAbleFrame(frame);
    _frame_cache.emplace_back(std::make_pair(frame->dts(), [cached_frame, this]() {
        _delegate.inputFrame(cached_frame);
    }));

    if (getBufferMS() > 30 * 1000) {
        //缓存超过30秒，强制消费至15秒(减少延时或内存占用)
        while (getBufferMS() > 15 * 1000) {
            _frame_cache.begin()->second();
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
        it->second();
        it = _frame_cache.erase(it);
    }
}

//////////////////////////////////////////////////////////////////////////

HlsPlayerImp::HlsPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<HlsPlayer, PlayerBase>(poller) {}

void HlsPlayerImp::onPacket(const char *data, size_t len) {
    if (!_decoder && _demuxer) {
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
    auto benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();
    if (ex || benchmark_mode) {
        PlayerImp<HlsPlayer, PlayerBase>::onPlayResult(ex);
    } else {
        auto demuxer = std::make_shared<HlsDemuxer>();
        demuxer->start(getPoller(), this);
        _demuxer = std::move(demuxer);
    }
}

void HlsPlayerImp::onShutdown(const SockException &ex) {
    while (_demuxer) {
        try {
            std::weak_ptr<HlsPlayerImp> weak_self = static_pointer_cast<HlsPlayerImp>(shared_from_this());
            static_pointer_cast<HlsDemuxer>(_demuxer)->pushTask([weak_self, ex]() {
                auto strong_self = weak_self.lock();
                if (strong_self) {
                    strong_self->_demuxer = nullptr;
                    strong_self->onShutdown(ex);
                }
            });
            return;
        } catch (...) {
            break;
        }
    }
    PlayerImp<HlsPlayer, PlayerBase>::onShutdown(ex);
}

vector<Track::Ptr> HlsPlayerImp::getTracks(bool ready) const {
    if (!_demuxer) {
        return vector<Track::Ptr>();
    }
    return static_pointer_cast<HlsDemuxer>(_demuxer)->getTracks(ready);
}

}//namespace mediakit
