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

string printSSRC(uint32_t ui32Ssrc);
class RtpProcess : public RtpReceiver , public RtpDecoder, public SockInfo, public MediaSinkInterface, public std::enable_shared_from_this<RtpProcess>{
public:
    typedef std::shared_ptr<RtpProcess> Ptr;
    RtpProcess(uint32_t ssrc);
    ~RtpProcess();
    bool inputRtp(const Socket::Ptr &sock, const char *data,int data_len, const struct sockaddr *addr , uint32_t *dts_out = nullptr);
    bool alive();

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
    uint32_t _ssrc;
    SdpTrack::Ptr _track;
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
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPPROCESS_H
