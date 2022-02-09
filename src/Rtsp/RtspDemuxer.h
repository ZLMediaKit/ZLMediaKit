/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTP_RTSPDEMUXER_H_
#define SRC_RTP_RTSPDEMUXER_H_

#include <unordered_map>
#include "Player/PlayerBase.h"
#include "Util/TimeTicker.h"
#include "RtpCodec.h"

namespace mediakit {

class RtspDemuxer : public Demuxer {
public:
    typedef std::shared_ptr<RtspDemuxer> Ptr;
    RtspDemuxer() = default;
    virtual ~RtspDemuxer() = default;

    /**
     * 加载sdp
     */
    void loadSdp(const std::string &sdp);

    /**
     * 开始解复用
     * @param rtp rtp包
     * @return true 代表是i帧第一个rtp包
     */
    bool inputRtp(const RtpPacket::Ptr &rtp);

    /**
     * 获取节目总时长
     * @return 节目总时长,单位秒
     */
    float getDuration() const;

private:
    void makeAudioTrack(const SdpTrack::Ptr &audio);
    void makeVideoTrack(const SdpTrack::Ptr &video);
    void loadSdp(const SdpParser &parser);

private:
    float _duration = 0;
    AudioTrack::Ptr _audio_track;
    VideoTrack::Ptr _video_track;
    RtpCodec::Ptr _audio_rtp_decoder;
    RtpCodec::Ptr _video_rtp_decoder;
};

} /* namespace mediakit */

#endif /* SRC_RTP_RTSPDEMUXER_H_ */
