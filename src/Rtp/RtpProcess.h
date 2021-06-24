/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPPROCESS_H
#define ZLMEDIAKIT_RTPPROCESS_H

#if defined(ENABLE_RTPPROXY)
#include "ProcessInterface.h"
#include "Common/MultiMediaSourceMuxer.h"

namespace mediakit {

class RtpProcess : public SockInfo, public MediaSinkInterface, public MediaSourceEventInterceptor, public std::enable_shared_from_this<RtpProcess>{
public:
    typedef std::shared_ptr<RtpProcess> Ptr;
    friend class RtpProcessHelper;
    RtpProcess(const string &stream_id);
    ~RtpProcess();

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
    bool inputRtp(bool is_udp, const Socket::Ptr &sock, const char *data, size_t len, const struct sockaddr *addr , uint32_t *dts_out = nullptr);

    /**
     * 是否超时，用于超时移除对象
     */
    bool alive();

    /**
     * 超时时被RtpSelector移除时触发
     */
    void onDetach();

    /**
     * 设置onDetach事件回调
     */
    void setOnDetach(const function<void()> &cb);

    /**
     * 设置onDetach事件回调,false检查RTP超时，true停止
     */
    void setStopCheckRtp(bool is_check=false);

    /// SockInfo override
    string get_local_ip() override;
    uint16_t get_local_port() override;
    string get_peer_ip() override;
    uint16_t get_peer_port() override;
    string getIdentifier() const override;

    int getTotalReaderCount();
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener);

protected:
    void inputFrame(const Frame::Ptr &frame) override;
    void addTrack(const Track::Ptr & track) override;
    void addTrackCompleted() override;
    void resetTracks() override {};

    //// MediaSourceEvent override ////
    MediaOriginType getOriginType(MediaSource &sender) const override;
    string getOriginUrl(MediaSource &sender) const override;
    std::shared_ptr<SockInfo> getOriginSock(MediaSource &sender) const override;

private:
    void emitOnPublish();
    void doCachedFunc();

private:
    uint32_t _dts = 0;
    uint64_t _total_bytes = 0;
    struct sockaddr _addr{0};
    Socket::Ptr _sock;
    MediaInfo _media_info;
    Ticker _last_frame_time;
    function<void()> _on_detach;
    std::shared_ptr<FILE> _save_file_rtp;
    std::shared_ptr<FILE> _save_file_video;
    ProcessInterface::Ptr _process;
    MultiMediaSourceMuxer::Ptr _muxer;
    atomic_bool _stop_rtp_check{false};
    atomic_flag _busy_flag{false};
    Ticker _last_check_alive;
    recursive_mutex _func_mtx;
    deque<function<void()> > _cached_func;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPPROCESS_H
