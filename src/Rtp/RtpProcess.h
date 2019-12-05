/*
* MIT License
*
* Copyright (c) 2016-2019 Gemfield <gemfield@civilnet.cn>
*
* This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
*
* Permission is hereby granted, free of charge, to any person obtaining a copy
* of this software and associated documentation files (the "Software"), to deal
* in the Software without restriction, including without limitation the rights
* to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
* copies of the Software, and to permit persons to whom the Software is
* furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all
* copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
* IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
* AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
* LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
* SOFTWARE.
*/

#ifndef RTPPROXY_RTPDECODER_H
#define RTPPROXY_RTPDECODER_H

#include "Rtsp/RtpReceiver.h"
#include "RtpDecoder.h"
#include "PSDecoder.h"
#include "Common/Device.h"
using namespace mediakit;

string printSSRC(uint32_t ui32Ssrc);

class FrameMerger;
class RtpProcess : public RtpReceiver , public RtpDecoder , public PSDecoder {
public:
    typedef std::shared_ptr<RtpProcess> Ptr;
    RtpProcess(uint32_t ssrc);
    ~RtpProcess();
    bool inputRtp(const char *data,int data_len, const struct sockaddr *addr);
    bool alive();
    string get_peer_ip();
    uint16_t get_peer_port();
protected:
    void onRtpSorted(const RtpPacket::Ptr &rtp, int track_index) override ;
    void onRtpDecode(const void *packet, int bytes, uint32_t timestamp, int flags) override;
    void onPSDecode(int stream,
                    int codecid,
                    int flags,
                    int64_t pts,
                    int64_t dts,
                    const void *data,
                    size_t bytes) override ;
private:
    std::shared_ptr<FILE> _save_file_rtp;
    std::shared_ptr<FILE> _save_file_ps;
    std::shared_ptr<FILE> _save_file_video;
    uint32_t _ssrc;
    SdpTrack::Ptr _track;
    struct sockaddr *_addr = nullptr;
    uint16_t _sequence = 0;
    int _codecid_video = 0;
    int _codecid_audio = 0;
    MultiMediaSourceMuxer::Ptr _muxer;
    std::shared_ptr<FrameMerger> _merger;
    Ticker _last_rtp_time;
};


#endif //RTPPROXY_RTPDECODER_H
