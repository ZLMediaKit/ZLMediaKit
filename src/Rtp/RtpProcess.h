/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPPROCESS_H
#define ZLMEDIAKIT_RTPPROCESS_H

#if defined(ENABLE_RTPPROXY)

#include "Rtsp/RtpReceiver.h"
#include "RtpDecoder.h"
#include "Decoder.h"
#include "Common/Device.h"
#include "Common/Stamp.h"
using namespace mediakit;

namespace mediakit{

class RtpProcess : public RtpReceiver , public RtpDecoder, public SockInfo, public MediaSinkInterface, public std::enable_shared_from_this<RtpProcess>{
public:
    typedef std::shared_ptr<RtpProcess> Ptr;
    RtpProcess(const string &stream_id);
    ~RtpProcess();

    /**
     * 输入rtp
     * @param sock 本地监听的socket
     * @param data rtp数据指针
     * @param data_len rtp数据长度
     * @param addr 数据源地址
     * @param dts_out 解析出最新的dts
     * @return 是否解析成功
     */
    bool inputRtp(const Socket::Ptr &sock, const char *data,int data_len, const struct sockaddr *addr , uint32_t *dts_out = nullptr);

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

    /// SockInfo override
    string get_local_ip() override;
    uint16_t get_local_port() override;
    string get_peer_ip() override;
    uint16_t get_peer_port() override;
    string getIdentifier() const override;

    int totalReaderCount();
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener);

protected:
    void onRtpSorted(const RtpPacket::Ptr &rtp, int track_index) override ;
    void onRtpDecode(const uint8_t *packet, int bytes, uint32_t timestamp, int flags) override;
    void inputFrame(const Frame::Ptr &frame) override;
    void addTrack(const Track::Ptr & track) override;
    void resetTracks() override {};

private:
    void emitOnPublish();

private:
    std::shared_ptr<FILE> _save_file_rtp;
    std::shared_ptr<FILE> _save_file_ps;
    std::shared_ptr<FILE> _save_file_video;
    struct sockaddr *_addr = nullptr;
    uint16_t _sequence = 0;
    MultiMediaSourceMuxer::Ptr _muxer;
    Ticker _last_rtp_time;
    uint32_t _dts = 0;
    DecoderImp::Ptr _decoder;
    std::weak_ptr<MediaSourceEvent> _listener;
    MediaInfo _media_info;
    uint64_t _total_bytes = 0;
    Socket::Ptr _sock;
    function<void()> _on_detach;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPPROCESS_H
