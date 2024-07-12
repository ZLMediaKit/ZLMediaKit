/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HlsPlayer.h"
#include "Common/config.h"
using namespace std;
using namespace toolkit;

namespace mediakit {

HlsPlayer::HlsPlayer(const EventPoller::Ptr &poller) {
    setPoller(poller ? poller : EventPollerPool::Instance().getPoller());
}

void HlsPlayer::play(const string &url) {
    _play_result = false;
    _play_url = url;
    setProxyUrl((*this)[Client::kProxyUrl]);
    setAllowResendRequest(true);
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
        // 如果不是主动关闭的，则重新拉取索引文件
        // if not actively closed, re-fetch the index file
        if (ex.getErrCode() != Err_shutdown && HlsParser::isLive()) {
            // 如果重试次数已经达到最大次数时, 且切片列表已空, 而且没有正在下载的切片, 则认为失败关闭播放器
            // If the retry count has reached the maximum number of times, and the segments list is empty, and there is no segment being downloaded,
            // the player is considered to be closed due to failure
            if (_ts_list.empty() && !(_http_ts_player && _http_ts_player->waitResponse()) && _try_fetch_index_times >= MAX_TRY_FETCH_INDEX_TIMES) {
                onShutdown(ex);
            } else {
                _try_fetch_index_times += 1;
                shutdown(ex);
                WarnL << "Attempt to pull the m3u8 file again[" << _try_fetch_index_times << "]:" << _play_url;
                // 当网络波动时有可能拉取m3u8文件失败, 因此快速重试拉取m3u8文件, 而不是直接关闭播放器
                // 这里增加一个延时是为了防止_http_ts_player的socket还保持alive状态，就多次拉取m3u8文件了
                // When the network fluctuates, it is possible to fail to pull the m3u8 file, so quickly retry to pull the m3u8 file instead of closing the player directly
                // The delay here is to prevent the socket of _http_ts_player from still keeping alive state, and pull the m3u8 file multiple times
                //todo _http_ts_player->waitResponse()这个判断条件是否有必要？因为有时候存在_complete==true，但是_http_ts_player->alive()为true的情况
                playDelay(0.3);
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
        // 如果是点播文件，播放列表为空代表文件播放结束，关闭播放器: #2628
        // If it is a video-on-demand file, the playlist is empty means the file is finished playing, close the player: #2628
        if (!HlsParser::isLive()) {
            teardown();
            return;
        }
        // 播放列表为空，那么立即重新下载m3u8文件
        // The playlist is empty, so download the m3u8 file immediately
        _timer.reset();
        fetchIndexFile();
        return;
    }
    if (_http_ts_player && _http_ts_player->waitResponse()) {
        // 播放器目前还存活，正在下载中
        return;
    }
    weak_ptr<HlsPlayer> weak_self = static_pointer_cast<HlsPlayer>(shared_from_this());
    if (!_http_ts_player) {
        _http_ts_player = std::make_shared<HttpTSPlayer>(getPoller());
        _http_ts_player->setProxyUrl((*this)[Client::kProxyUrl]);
        _http_ts_player->setAllowResendRequest(true);
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
                // 收到ts包
                // Received ts package
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
            WarnL << "Download ts segment " << url << " failed:" << err;
            if (err.getErrCode() == Err_timeout) {
                strong_self->_timeout_multiple = MAX(strong_self->_timeout_multiple + 1, MAX_TIMEOUT_MULTIPLE);
            } else {
                strong_self->_timeout_multiple = MAX(strong_self->_timeout_multiple - 1, MIN_TIMEOUT_MULTIPLE);
            }
            strong_self->_ts_download_failed_count++;
            if (strong_self->_ts_download_failed_count > MAX_TS_DOWNLOAD_FAILED_COUNT) {
                WarnL << "ts segment " << url << " download failed count is " << strong_self->_ts_download_failed_count << ", teardown player";
                strong_self->teardown_l(SockException(Err_shutdown, "ts segment download failed"));
                return;
            }
        } else {
            strong_self->_ts_download_failed_count = 0;
        }
        // 提前0.5秒下载好，支持点播文件控制下载速度: #2628
        // Download 0.5 seconds in advance to support video-on-demand files to control download speed: #2628
        auto delay = duration - 0.5 - ticker.elapsedTime() / 1000.0f;
        if (delay > 2.0) {
            // 提前1秒下载
            // Download 1 second in advance
            delay -= 1.0;
        } else if (delay <= 0) {
            // 延时最小10ms
            // Delay at least 10ms
            delay = 0.01;
        }
        // 延时下载下一个切片
        strong_self->_timer_ts.reset(new Timer(delay, [weak_self]() {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                strong_self->fetchSegment();
            }
            return false;
        }, strong_self->getPoller()));
    });

    _http_ts_player->setMethod("GET");
    // ts切片必须在其时长的2-5倍内下载完毕
    // The ts segment must be downloaded within 2-5 times its duration
    _http_ts_player->setCompleteTimeout(_timeout_multiple * duration * 1000);
    _http_ts_player->sendRequest(url);
}

bool HlsPlayer::onParsed(bool is_m3u8_inner, int64_t sequence, const map<int, ts_segment> &ts_map) {
    if (!is_m3u8_inner) {
        // 这是ts播放列表
        // This is the ts playlist
        if (_last_sequence == sequence) {
            // 如果是重复的ts列表，那么忽略
            // 但是需要注意, 如果当前ts列表为空了, 那么表明直播结束了或者m3u8文件有问题,需要重新拉流
            // 这里的5倍是为了防止m3u8文件有问题导致的无限重试
            // If it is a duplicate ts list, ignore it
            // But it should be noted that if the current ts list is empty, it means that the live broadcast is over or the m3u8 file is problematic, and you need to re-pull the stream
            // The 5 times here is to prevent infinite retries caused by problems with the m3u8 file
            if (_last_sequence > 0 && _ts_list.empty() && HlsParser::isLive()
                && _wait_index_update_ticker.elapsedTime() > (uint64_t)HlsParser::getTargetDur() * 1000 * 5) {
                _wait_index_update_ticker.resetTime();
                WarnL << "Fetch new ts list from m3u8 timeout";
                return false;
            }
            return true;
        }

        _last_sequence = sequence;
        _wait_index_update_ticker.resetTime();
        for (auto &pr : ts_map) {
            auto &ts = pr.second;
            if (_ts_url_cache.emplace(ts.url).second) {
                // 该ts未重复
                // The ts is not repeated
                _ts_list.emplace_back(ts);
                // 按时间排序
                // Sort by time
                _ts_url_sort.emplace_back(ts.url);
            }
        }
        if (_ts_url_sort.size() > 2 * ts_map.size()) {
            // 去除防重列表中过多的数据
            // Remove too much data from the anti-repetition list
            _ts_url_cache.erase(_ts_url_sort.front());
            _ts_url_sort.pop_front();
        }
        fetchSegment();
    } else {
        // 这是m3u8列表,我们播放最高清的子hls
        // This is the m3u8 list, we play the highest quality sub-hls
        if (ts_map.empty()) {
            throw invalid_argument("empty sub hls list:" + getUrl());
        }
        _timer.reset();
        weak_ptr<HlsPlayer> weak_self = static_pointer_cast<HlsPlayer>(shared_from_this());
        auto url = ts_map.rbegin()->second.url;
        getPoller()->async([weak_self, url]() {
            auto strong_self = weak_self.lock();
            if (strong_self) {
                strong_self->play(url);
            }
        }, false);
    }
    return true;
}

void HlsPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &headers) {
    if (status != "200" && status != "206") {
        // 失败
        // Failed
        throw invalid_argument("bad http status code:" + status);
    }
    auto content_type = strToLower(const_cast<HttpClient::HttpHeader &>(headers)["Content-Type"]);
    if (content_type.find("application/vnd.apple.mpegurl") != 0 && content_type.find("/x-mpegurl") == _StrPrinter::npos) {
        WarnL << "May not a hls video: " << content_type << ", url: " << getUrl();
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
        teardown_l(SockException(Err_other, "parse m3u8 failed:" + _play_url));
        return;
    }
    // 如果有或取到新的切片, 那么就算成功, 应该重置失败次数
    // if there are new segments or get new segments, it is considered successful, and the number of failures should be reset
    if (!_ts_list.empty()) {
        _try_fetch_index_times = 0;
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
            // refresh the index list according to rfc cycle should be the segment time x3,
            // because according to the specification, the player only handles the last 3 segments
            targetOffset = (float)(3 * HlsParser::getTargetDur());
        } else {
            // 点播则一般m3u8文件不会在改变了, 没必要频繁的刷新, 所以按照总时间来进行刷新
            // On-demand, the m3u8 file will generally not change, so there is no need to refresh frequently,
            targetOffset = HlsParser::getTotalDuration();
        }
        // 取最小值, 避免因为分段时长不规则而导致的问题
        // Take the minimum value to avoid problems caused by irregular segment duration
        if (targetOffset > HlsParser::getTotalDuration()) {
            targetOffset = HlsParser::getTotalDuration();
        }
        // 根据规范为一半的时间
        // According to the specification, it is half the time
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

void HlsPlayer::playDelay(float delay_sec) {
    weak_ptr<HlsPlayer> weak_self = static_pointer_cast<HlsPlayer>(shared_from_this());
    if (delay_sec == 0) {
        delay_sec = delaySecond();
    }
    _timer.reset(new Timer(delay_sec, [weak_self]() {
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

    // 每50毫秒执行一次
    // Execute every 50 milliseconds
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
    // 为了避免track准备时间过长, 因此在没准备好之前, 直接消费掉所有的帧
    // In order to avoid the track preparation time is too long, so before it is ready, all frames are consumed directly
    if (!_delegate.isAllTrackReady()) {
        _delegate.inputFrame(frame);
        return true;
    }

    if (_frame_cache.empty()) {
        // 设置当前播放位置时间戳
        // Set the current playback position timestamp
        setPlayPosition(frame->dts());
    }
    // 根据时间戳缓存frame
    // Cache frame according to timestamp
    auto cached_frame = Frame::getCacheAbleFrame(frame);
    _frame_cache.emplace_back(std::make_pair(frame->dts(), [cached_frame, this]() {
        _delegate.inputFrame(cached_frame);
    }));

    if (getBufferMS() > 30 * 1000) {
        // 缓存超过30秒，强制消费至15秒(减少延时或内存占用)
        // The cache exceeds 30 seconds, and the consumption is forced to 15 seconds (reduce delay or memory usage)
        while (getBufferMS() > 15 * 1000) {
            _frame_cache.begin()->second();
            _frame_cache.erase(_frame_cache.begin());
        }
        // 接着播放缓存中最早的帧
        // Then play the earliest frame in the cache
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
            // 这些帧还未到时间播放
            // These frames are not yet time to play
            break;
        }

        if (getBufferMS() < 3 * 1000) {
            // 缓存小于3秒,那么降低定时器消费速度(让剩余的数据在3秒后消费完毕)
            // 目的是为了防止定时器长时间干等后，数据瞬间消费完毕
            // If the cache is less than 3 seconds, then reduce the speed of the timer to consume (let the remaining data be consumed after 3 seconds)
            // The purpose is to prevent the timer from waiting for a long time, and the data is consumed instantly
            setPlayPosition(_frame_cache.begin()->first);
        }

        // 消费掉已经到期的帧
        // Consume expired frames
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
            // shared_from_this()可能抛异常
            // shared_from_this() may throw an exception
            std::weak_ptr<HlsPlayerImp> weak_self = static_pointer_cast<HlsPlayerImp>(shared_from_this());
            if (_decoder) {
                _decoder->flush();
            }
            // 等待所有frame flush输出后，再触发onShutdown事件
            // Wait for all frame flush output, then trigger the onShutdown event
            static_pointer_cast<HlsDemuxer>(_demuxer)->pushTask([weak_self, ex]() {
                if (auto strong_self = weak_self.lock()) {
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
