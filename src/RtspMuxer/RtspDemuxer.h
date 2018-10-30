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

#ifndef SRC_RTP_RTSPDEMUXER_H_
#define SRC_RTP_RTSPDEMUXER_H_

#include <unordered_map>
#include "Rtsp/Rtsp.h"
#include "Player/PlayerBase.h"
#include "Util/TimeTicker.h"
#include "RtspMuxer/RtpCodec.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtspDemuxer : public PlayerBase{
public:
	typedef std::shared_ptr<RtspDemuxer> Ptr;
	RtspDemuxer(const string &sdp);
	RtspDemuxer(const SdpAttr &attr);
	virtual ~RtspDemuxer(){};

	/**
	 * 开始解复用
	 * @param rtp rtp包
	 * @return true 代表是i帧第一个rtp包
	 */
	bool inputRtp(const RtpPacket::Ptr &rtp);

	/**
	 * 获取节目总时长
	 * @return
	 */
	float getDuration() const override;

	/**
	 * 返回是否完成初始化完毕
	 * 由于有些rtsp的sdp不包含sps pps信息
	 * 所以要等待接收到到sps的rtp包后才能完成
	 * @return
	 */
    bool isInited() const override;

    /**
     * 获取所有可用Track，请在isInited()返回true时调用
     * @return
     */
    vector<Track::Ptr> getTracks() const override;
private:
	void makeAudioTrack(const SdpTrack::Ptr &audio);
	void makeVideoTrack(const SdpTrack::Ptr &video);
	void loadSdp(const SdpAttr &attr);
private:
	float _fDuration = 0;
	AudioTrack::Ptr _audioTrack;
	VideoTrack::Ptr _videoTrack;
	RtpCodec::Ptr _audioRtpDecoder;
	RtpCodec::Ptr _videoRtpDecoder;
};

} /* namespace mediakit */

#endif /* SRC_RTP_RTSPDEMUXER_H_ */
