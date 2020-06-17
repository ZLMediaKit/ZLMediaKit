/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HTTP_HLSPLAYER_H
#define HTTP_HLSPLAYER_H

#include <unordered_set>
#include "Util/util.h"
#include "Poller/Timer.h"
#include "Http/HttpDownloader.h"
#include "Player/MediaPlayer.h"
#include "HlsParser.h"
#include "HttpTSPlayer.h"
#include "Rtp/Decoder.h"
#include "Rtp/TSDecoder.h"

using namespace toolkit;
namespace mediakit {

class HlsPlayer : public  HttpClientImp , public PlayerBase , public HlsParser{
public:
    HlsPlayer(const EventPoller::Ptr &poller);
    ~HlsPlayer() override;

    /**
     * 开始播放
     * @param strUrl
     */
    void play(const string &strUrl) override;

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
    virtual void onPacket(const char *data, uint64_t len) = 0;

private:
    /**
     * 解析m3u8成功
     * @param is_m3u8_inner 是否为m3u8列表
     * @param sequence ts列表seq
     * @param ts_map ts列表或m3u8列表
     */
    void onParsed(bool is_m3u8_inner,int64_t sequence,const map<int,ts_segment> &ts_map) override;
    /**
     * 收到http回复头
     * @param status 状态码，譬如:200 OK
     * @param headers http头
     * @return 返回后续content的长度；-1:后续数据全是content；>=0:固定长度content
     *          需要指出的是，在http头中带有Content-Length字段时，该返回值无效
     */
    int64_t onResponseHeader(const string &status,const HttpHeader &headers) override;
    /**
     * 收到http conten数据
     * @param buf 数据指针
     * @param size 数据大小
     * @param recvedSize 已收数据大小(包含本次数据大小),当其等于totalSize时将触发onResponseCompleted回调
     * @param totalSize 总数据大小
     */
    void onResponseBody(const char *buf,int64_t size,int64_t recvedSize,int64_t totalSize) override;

    /**
     * 接收http回复完毕,
     */
    void onResponseCompleted() override;

    /**
     * http链接断开回调
     * @param ex 断开原因
     */
    void onDisconnect(const SockException &ex) override;

    /**
     * 重定向事件
     * @param url 重定向url
     * @param temporary 是否为临时重定向
     * @return 是否继续
     */
    bool onRedirectUrl(const string &url,bool temporary) override;

private:
    void playDelay();
    float delaySecond();
    void playNextTs(bool force = false);
    void teardown_l(const SockException &ex);
    void play_l();
    void onPacket_l(const char *data, uint64_t len);

private:
    struct UrlComp {
        //url忽略？后面的参数
        bool operator()(const string& __x, const string& __y) const {
            return split(__x,"?")[0] < split(__y,"?")[0];
        }
    };

private:
    bool _is_m3u8 = false;
    bool _first = true;
    int64_t _last_sequence = -1;
    string _m3u8;
    Timer::Ptr _timer;
    Timer::Ptr _timer_ts;
    list<ts_segment> _ts_list;
    list<string> _ts_url_sort;
    list<string> _m3u8_list;
    set<string, UrlComp> _ts_url_cache;
    HttpTSPlayer::Ptr _http_ts_player;
    TSSegment _segment;
};

class HlsPlayerImp : public PlayerImp<HlsPlayer, PlayerBase> , public MediaSink{
public:
    typedef std::shared_ptr<HlsPlayerImp> Ptr;
    HlsPlayerImp(const EventPoller::Ptr &poller = nullptr);
    ~HlsPlayerImp() override {};
    void setOnPacket(const TSSegment::onSegment &cb);

private:
    void onPacket(const char *data, uint64_t len) override;
    void onAllTrackReady() override;
    void onPlayResult(const SockException &ex) override;
    vector<Track::Ptr> getTracks(bool trackReady = true) const override;
    void inputFrame(const Frame::Ptr &frame) override;
    void onShutdown(const SockException &ex) override;
    void onTick();
private:
    TSSegment::onSegment _on_ts;
    DecoderImp::Ptr _decoder;
    multimap<int64_t, Frame::Ptr> _frame_cache;
    Timer::Ptr _timer;
    Ticker _ticker;
    Stamp _stamp[2];
};

}//namespace mediakit 
#endif //HTTP_HLSPLAYER_H
