/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_RTMP_RTMPDEMUXER_H_
#define SRC_RTMP_RTMPDEMUXER_H_

#include <functional>
#include <unordered_map>
#include "Rtmp/amf.h"
#include "Rtmp/Rtmp.h"
#include "Player/PlayerBase.h"
#include "Util/TimeTicker.h"
#include "RtmpCodec.h"

namespace mediakit {

class RtmpDemuxer : public Demuxer {
public:
    using Ptr = std::shared_ptr<RtmpDemuxer>;

    RtmpDemuxer() = default;
    ~RtmpDemuxer() override = default;

    static size_t trackCount(const AMFValue &metadata);

    bool loadMetaData(const AMFValue &metadata);

    /**
     * 开始解复用
     * @param pkt rtmp包
     */
    void inputRtmp(const RtmpPacket::Ptr &pkt);

    /**
     * 获取节目总时长
     * @return 节目总时长,单位秒
     */
    float getDuration() const;

private:
    void makeVideoTrack(const AMFValue &val, int bit_rate);
    void makeAudioTrack(const AMFValue &val, int sample_rate, int channels, int sample_bit, int bit_rate);

private:
    bool _try_get_video_track = false;
    bool _try_get_audio_track = false;
    float _duration = 0;
    AudioTrack::Ptr _audio_track;
    VideoTrack::Ptr _video_track;
    RtmpCodec::Ptr _audio_rtmp_decoder;
    RtmpCodec::Ptr _video_rtmp_decoder;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPDEMUXER_H_ */
