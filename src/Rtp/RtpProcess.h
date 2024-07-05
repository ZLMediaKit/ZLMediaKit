﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPPROCESS_H
#define ZLMEDIAKIT_RTPPROCESS_H

#if defined(ENABLE_RTPPROXY)
#include "ProcessInterface.h"
#include "Rtcp/RtcpContext.h"
#include "Common/MultiMediaSourceMuxer.h"

namespace mediakit {

static constexpr char kRtpAppName[] = "rtp";

class RtpProcess final : public RtcpContextForRecv, public toolkit::SockInfo, public MediaSinkInterface, public MediaSourceEvent, public std::enable_shared_from_this<RtpProcess>{
public:
    using Ptr = std::shared_ptr<RtpProcess>;
    using onDetachCB = std::function<void(const toolkit::SockException &ex)>;

    static Ptr createProcess(const MediaTuple &tuple);
    ~RtpProcess();
    enum OnlyTrack { kAll = 0, kOnlyAudio = 1, kOnlyVideo = 2 };

    /**
     * 输入rtp
     * @param is_udp 是否为udp模式
     * @param sock 本地监听的socket
     * @param data rtp数据指针
     * @param len rtp数据长度
     * @param addr 数据源地址
     * @param dts_out 解析出最新的dts
     * @return 是否解析成功
     */
    bool inputRtp(bool is_udp, const toolkit::Socket::Ptr &sock, const char *data, size_t len, const struct sockaddr *addr , uint64_t *dts_out = nullptr);


    /**
     * 超时时被RtpSelector移除时触发
     */
    void onDetach(const toolkit::SockException &ex);

    /**
     * 设置onDetach事件回调
     */
    void setOnDetach(onDetachCB cb);

    /**
     * 设置onDetach事件回调,false检查RTP超时，true停止
     */
    void setStopCheckRtp(bool is_check=false);

    /**
     * 设置为单track，单音频/单视频时可以加快媒体注册速度
     * 请在inputRtp前调用此方法，否则可能会是空操作
     */
    void setOnlyTrack(OnlyTrack only_track);

    /**
     * flush输出缓存
     */
    void flush() override;

    /// SockInfo override
    std::string get_local_ip() override;
    uint16_t get_local_port() override;
    std::string get_peer_ip() override;
    uint16_t get_peer_port() override;
    std::string getIdentifier() const override;

protected:
    bool inputFrame(const Frame::Ptr &frame) override;
    bool addTrack(const Track::Ptr & track) override;
    void addTrackCompleted() override;
    void resetTracks() override {};

    //// MediaSourceEvent override ////
    MediaOriginType getOriginType(MediaSource &sender) const override;
    std::string getOriginUrl(MediaSource &sender) const override;
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;
    float getLossRate(MediaSource &sender, TrackType type) override;
    Ptr getRtpProcess(mediakit::MediaSource &sender) const override;
    bool close(mediakit::MediaSource &sender) override;

private:
    RtpProcess(const MediaTuple &tuple);

    void emitOnPublish();
    void doCachedFunc();
    bool alive();
    void onManager();
    void createTimer();

private:
    OnlyTrack _only_track = kAll;
    std::string _auth_err;
    uint64_t _dts = 0;
    uint64_t _total_bytes = 0;
    std::unique_ptr<sockaddr_storage> _addr;
    toolkit::Socket::Ptr _sock;
    MediaInfo _media_info;
    toolkit::Ticker _last_frame_time;
    onDetachCB _on_detach;
    std::shared_ptr<FILE> _save_file_rtp;
    std::shared_ptr<FILE> _save_file_video;
    ProcessInterface::Ptr _process;
    MultiMediaSourceMuxer::Ptr _muxer;
    std::atomic_bool _stop_rtp_check{false};
    toolkit::Timer::Ptr _timer;
    toolkit::Ticker _last_check_alive;
    std::recursive_mutex _func_mtx;
    toolkit::Ticker _cache_ticker;
    std::deque<std::function<void()> > _cached_func;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPPROCESS_H
