/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HTTP_HLSPLAYER_H
#define HTTP_HLSPLAYER_H

#include "Common/Stamp.h"
#include "Player/PlayerBase.h"
#include "HttpTSPlayer.h"
#include "HlsParser.h"
#include "Rtp/TSDecoder.h"

#define MIN_TIMEOUT_MULTIPLE 2
#define MAX_TIMEOUT_MULTIPLE 5
#define MAX_TRY_FETCH_INDEX_TIMES 5

namespace mediakit {

class HlsDemuxer : public MediaSinkInterface , public TrackSource, public std::enable_shared_from_this<HlsDemuxer> {
public:
    HlsDemuxer() = default;
    ~HlsDemuxer() override { _timer = nullptr; }

    void start(const toolkit::EventPoller::Ptr &poller, TrackListener *listener);
    bool inputFrame(const Frame::Ptr &frame) override;
    bool addTrack(const Track::Ptr &track) override { return _delegate.addTrack(track); }
    void addTrackCompleted() override { _delegate.addTrackCompleted(); }
    void resetTracks() override { ((MediaSink &)_delegate).resetTracks(); }
    std::vector<Track::Ptr> getTracks(bool ready = true) const override { return _delegate.getTracks(ready); }
    void pushTask(std::function<void()> task);

private:
    void onTick();
    int64_t getBufferMS();
    int64_t getPlayPosition();
    void setPlayPosition(int64_t pos);

private:
    int64_t _ticker_offset = 0;
    toolkit::Ticker _ticker;
    toolkit::Timer::Ptr _timer;
    MediaSinkDelegate _delegate;
    std::deque<std::pair<int64_t, std::function<void()> > > _frame_cache;
};

class HlsPlayer : public  HttpClientImp , public PlayerBase , public HlsParser{
public:
    HlsPlayer(const toolkit::EventPoller::Ptr &poller);
    ~HlsPlayer() override = default;

    /**
     * 开始播放
     */
    void play(const std::string &url) override;

    /**
     * 停止播放
     */
    void teardown() override;

protected:
    /**
     * 收到ts包
     * @param data ts数据负载
     * @param len ts包长度
     */
    virtual void onPacket(const char *data, size_t len) = 0;

private:
    void onParsed(bool is_m3u8_inner,int64_t sequence,const map<int,ts_segment> &ts_map) override;
    void onResponseHeader(const std::string &status,const HttpHeader &headers) override;
    void onResponseBody(const char *buf,size_t size) override;
    void onResponseCompleted(const toolkit::SockException &e) override;
    bool onRedirectUrl(const std::string &url,bool temporary) override;

private:
    void playDelay();
    float delaySecond();
    void fetchSegment();
    void teardown_l(const toolkit::SockException &ex);
    void fetchIndexFile();

private:
    struct UrlComp {
        //url忽略？后面的参数
        bool operator()(const std::string& __x, const std::string& __y) const {
            return toolkit::split(__x,"?")[0] < toolkit::split(__y,"?")[0];
        }
    };

private:
    bool _play_result = false;
    int64_t _last_sequence = -1;
    std::string _m3u8;
    std::string _play_url;
    toolkit::Timer::Ptr _timer;
    toolkit::Timer::Ptr _timer_ts;
    std::list<ts_segment> _ts_list;
    std::list<std::string> _ts_url_sort;
    std::set<std::string, UrlComp> _ts_url_cache;
    HttpTSPlayer::Ptr _http_ts_player;
    int _timeout_multiple = MIN_TIMEOUT_MULTIPLE;
    int _try_fetch_index_times = 0;
};

class HlsPlayerImp : public PlayerImp<HlsPlayer, PlayerBase>, private TrackListener {
public:
    typedef std::shared_ptr<HlsPlayerImp> Ptr;
    HlsPlayerImp(const toolkit::EventPoller::Ptr &poller = nullptr);
    ~HlsPlayerImp() override = default;

private:
    //// HlsPlayer override////
    void onPacket(const char *data, size_t len) override;

private:
    //// PlayerBase override////
    void onPlayResult(const toolkit::SockException &ex) override;
    std::vector<Track::Ptr> getTracks(bool ready = true) const override;
    void onShutdown(const toolkit::SockException &ex) override;

private:
    //// TrackListener override////
    bool addTrack(const Track::Ptr &track) override { return true; };
    void addTrackCompleted() override;

private:
    DecoderImp::Ptr _decoder;
    MediaSinkInterface::Ptr _demuxer;
};

}//namespace mediakit 
#endif //HTTP_HLSPLAYER_H
