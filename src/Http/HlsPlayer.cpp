/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HlsPlayer.h"
namespace mediakit {

HlsPlayer::HlsPlayer(const EventPoller::Ptr &poller){
    _segment.setOnSegment([this](const char *data, uint64_t len) { onPacket(data, len); });
    _poller = poller ? poller : EventPollerPool::Instance().getPoller();
}

HlsPlayer::~HlsPlayer() {}

void HlsPlayer::play(const string &strUrl) {
    _m3u8_list.emplace_back(strUrl);
    play_l();
}

void HlsPlayer::play_l(){
    if (_m3u8_list.empty()) {
        teardown_l(SockException(Err_shutdown, "所有hls url都尝试播放失败!"));
        return;
    }
    float playTimeOutSec = (*this)[Client::kTimeoutMS].as<int>() / 1000.0;
    setMethod("GET");
    if(!(*this)[kNetAdapter].empty()) {
        setNetAdapter((*this)[kNetAdapter]);
    }
    sendRequest(_m3u8_list.back(), playTimeOutSec);
}

void HlsPlayer::teardown_l(const SockException &ex){
    _timer.reset();
    _timer_ts.reset();
    _http_ts_player.reset();
    shutdown(ex);
}

void HlsPlayer::teardown() {
    teardown_l(SockException(Err_shutdown,"teardown"));
}

void HlsPlayer::playNextTs(bool force){
    if (_ts_list.empty()) {
        //播放列表为空，那么立即重新下载m3u8文件
        _timer.reset();
        play_l();
        return;
    }
    if (!force && _http_ts_player && _http_ts_player->alive()) {
        //播放器目前还存活，正在下载中
        return;
    }
    auto ts_duration = _ts_list.front().duration * 1000;
    weak_ptr<HlsPlayer> weakSelf = dynamic_pointer_cast<HlsPlayer>(shared_from_this());
    std::shared_ptr<Ticker> ticker(new Ticker);

    _http_ts_player = std::make_shared<HttpTSPlayer>(getPoller(), false);
    _http_ts_player->setOnDisconnect([weakSelf, ticker, ts_duration](const SockException &err) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        auto delay = ts_duration - 500 - ticker->elapsedTime();
        if (delay <= 0) {
            //播放这个ts切片花费了太长时间，我们立即下一个切片的播放
            strongSelf->playNextTs(true);
        } else {
            //下一个切片慢点播放
            strongSelf->_timer_ts.reset(new Timer(delay / 1000.0, [weakSelf, delay]() {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return false;
                }
                strongSelf->playNextTs(true);
                return false;
            }, strongSelf->getPoller()));
        }
    });
    _http_ts_player->setOnPacket([weakSelf](const char *data, uint64_t len) {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        //收到ts包
        strongSelf->onPacket_l(data, len);
    });

    _http_ts_player->setMethod("GET");
    if(!(*this)[kNetAdapter].empty()) {
        _http_ts_player->setNetAdapter((*this)[Client::kNetAdapter]);
    }
    _http_ts_player->sendRequest(_ts_list.front().url, 2 * _ts_list.front().duration);
    _ts_list.pop_front();
}

void HlsPlayer::onParsed(bool is_m3u8_inner,int64_t sequence,const map<int,ts_segment> &ts_map){
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

        weak_ptr<HlsPlayer> weakSelf = dynamic_pointer_cast<HlsPlayer>(shared_from_this());
        auto url = ts_map.rbegin()->second.url;
        getPoller()->async([weakSelf, url]() {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                strongSelf->play(url);
            }
        }, false);
    }
}

int64_t HlsPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &headers) {
    if (status != "200" && status != "206") {
        //失败
        teardown_l(SockException(Err_shutdown, StrPrinter << "bad http status code:" + status));
        return 0;
    }
    auto contet_type = const_cast< HttpClient::HttpHeader &>(headers)["Content-Type"];
    _is_m3u8 = (contet_type.find("application/vnd.apple.mpegurl") == 0);
    return -1;
}

void HlsPlayer::onResponseBody(const char *buf, int64_t size, int64_t recvedSize, int64_t totalSize) {
    if (recvedSize == size) {
        //刚开始
        _m3u8.clear();
    }
    _m3u8.append(buf, size);
}

void HlsPlayer::onResponseCompleted() {
    if (HlsParser::parse(getUrl(), _m3u8)) {
        playDelay();
        if (_first) {
            _first = false;
            onPlayResult(SockException(Err_success, "play success"));
        }
    } else {
        teardown_l(SockException(Err_shutdown, "解析m3u8文件失败"));
    }
}

float HlsPlayer::delaySecond(){
    if (HlsParser::isM3u8() && HlsParser::getTargetDur() > 0) {
        return HlsParser::getTargetDur();
    }
    return 1;
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

bool HlsPlayer::onRedirectUrl(const string &url,bool temporary) {
    _m3u8_list.emplace_back(url);
    return true;
}

void HlsPlayer::playDelay(){
    weak_ptr<HlsPlayer> weakSelf = dynamic_pointer_cast<HlsPlayer>(shared_from_this());
    _timer.reset(new Timer(delaySecond(), [weakSelf]() {
        auto strongSelf = weakSelf.lock();
        if (strongSelf) {
            strongSelf->play_l();
        }
        return false;
    }, getPoller()));
}

void HlsPlayer::onPacket_l(const char *data, uint64_t len){
    _segment.input(data,len);
}

//////////////////////////////////////////////////////////////////////////

HlsPlayerImp::HlsPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<HlsPlayer, PlayerBase>(poller) {

}

void HlsPlayerImp::setOnPacket(const TSSegment::onSegment &cb){
    _on_ts = cb;
}

void HlsPlayerImp::onPacket(const char *data,uint64_t len) {
    if (_on_ts) {
        _on_ts(data, len);
    }

    if (!_decoder) {
        _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, this);
    }

    if (_decoder) {
        _decoder->input((uint8_t *) data, len);
    }
}

void HlsPlayerImp::onAllTrackReady() {
    PlayerImp<HlsPlayer, PlayerBase>::onPlayResult(SockException(Err_success,"play hls success"));
}

void HlsPlayerImp::onPlayResult(const SockException &ex) {
    if(ex){
        PlayerImp<HlsPlayer, PlayerBase>::onPlayResult(ex);
    }else{
        _stamp[TrackAudio].syncTo(_stamp[TrackVideo]);
        _ticker.resetTime();
        weak_ptr<HlsPlayerImp> weakSelf = dynamic_pointer_cast<HlsPlayerImp>(shared_from_this());
        //每50毫秒执行一次
        _timer = std::make_shared<Timer>(0.05, [weakSelf]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            strongSelf->onTick();
            return true;
        }, getPoller());
    }
}

void HlsPlayerImp::onShutdown(const SockException &ex) {
    PlayerImp<HlsPlayer, PlayerBase>::onShutdown(ex);
    _timer = nullptr;
}

vector<Track::Ptr> HlsPlayerImp::getTracks(bool trackReady) const {
    return MediaSink::getTracks(trackReady);
}

void HlsPlayerImp::inputFrame(const Frame::Ptr &frame) {
    //计算相对时间戳
    int64_t dts, pts;
    _stamp[frame->getTrackType()].revise(frame->dts(), frame->pts(), dts, pts);
    //根据时间戳缓存frame
    _frame_cache.emplace(dts, Frame::getCacheAbleFrame(frame));

    while (!_frame_cache.empty()) {
        if (_frame_cache.rbegin()->first - _frame_cache.begin()->first > 30 * 1000) {
            //缓存超过30秒，强制消费掉
            MediaSink::inputFrame(_frame_cache.begin()->second);
            _frame_cache.erase(_frame_cache.begin());
            continue;
        }
        //缓存小于30秒
        break;
    }
}

void HlsPlayerImp::onTick() {
    auto it = _frame_cache.begin();
    while (it != _frame_cache.end()) {
        if (it->first > _ticker.elapsedTime()) {
            //这些帧还未到时间播放
            break;
        }
        //消费掉已经到期的帧
        MediaSink::inputFrame(it->second);
        it = _frame_cache.erase(it);
    }
}


}//namespace mediakit