/*
* MIT License
*
* Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#ifndef ZLMEDIAKIT_RTSPMUXER_H
#define ZLMEDIAKIT_RTSPMUXER_H

#include "RtspSdp.h"

namespace mediakit{
/**
* rtsp生成器
*/
class RtspMuxer{
public:
    /**
     * 构造函数
     */
    RtspMuxer(const TitleSdp::Ptr &title = std::make_shared<TitleSdp>()){
        _sdp = title->getSdp();
        _rtpRing = std::make_shared<RtpRingInterface::RingType>();
    }
    virtual ~RtspMuxer(){}

    /**
     * 添加音视频或title 媒体
     * @param track 媒体描述
     * @param ssrc 媒体rtp ssrc
     * @param mtu 媒体rtp mtu
     */
    void addTrack(const Track::Ptr & track,uint32_t ssrc = 0,int mtu = 1400) ;

    /**
     * 获取完整的SDP字符串
     * @return SDP字符串
     */
    string getSdp() ;

    /**
     * 写入帧数据然后打包rtp
     * @param frame 帧数据
     */
    void inputFrame(const Frame::Ptr &frame) ;

    /**
     * 也可以在外部打包好rtp然后再写入
     * @param rtp rtp包
     * @param key_pos 是否为关键帧的第一个rtp包
     */
    bool inputRtp(const RtpPacket::Ptr &rtp, bool key_pos = true);

    /**
     * 获取rtp环形缓存
     * @return
     */
    RtpRingInterface::RingType::Ptr getRtpRing() const;
private:
    map<int,Track::Ptr> _track_map;
    map<int,function<void()> > _trackReadyCallback;
    RtpRingInterface::RingType::Ptr _rtpRing;
    string _sdp;
};


} /* namespace mediakit */

#endif //ZLMEDIAKIT_RTSPMUXER_H
