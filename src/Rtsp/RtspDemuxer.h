/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#ifndef SRC_RTP_RTSPDEMUXER_H_
#define SRC_RTP_RTSPDEMUXER_H_

#include <unordered_map>
#include "Player/PlayerBase.h"
#include "Util/TimeTicker.h"
#include "RtpCodec.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtspDemuxer : public Demuxer{
public:
    typedef std::shared_ptr<RtspDemuxer> Ptr;
    RtspDemuxer() = default;
    virtual ~RtspDemuxer() = default;

    /**
     * 加载sdp
     */
    void loadSdp(const string &sdp);

    /**
     * 开始解复用
     * @param rtp rtp包
     * @return true 代表是i帧第一个rtp包
     */
    bool inputRtp(const RtpPacket::Ptr &rtp);
private:
    void makeAudioTrack(const SdpTrack::Ptr &audio);
    void makeVideoTrack(const SdpTrack::Ptr &video);
    void loadSdp(const SdpParser &parser);
private:
    RtpCodec::Ptr _audioRtpDecoder;
    RtpCodec::Ptr _videoRtpDecoder;
};

} /* namespace mediakit */

#endif /* SRC_RTP_RTSPDEMUXER_H_ */
