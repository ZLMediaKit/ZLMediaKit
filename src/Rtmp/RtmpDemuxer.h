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
#include "Common/MediaSink.h"
#include "Util/TimeTicker.h"
#include "RtmpCodec.h"

namespace mediakit {
/*
Rtmp协议解复用器，用于将Rtmp包转成addTrack及Track上的inputFrame回调
支持两种建立Track机制：
- MetaData：解析完毕后调用addTrackCompleted
- RtmpPacket：由于客户端未必有发MetaData，这是最可靠的创建Track机制，但相对延迟会高点
*/
class RtmpDemuxer : public Demuxer {
public:
    using Ptr = std::shared_ptr<RtmpDemuxer>;

    RtmpDemuxer() = default;
    ~RtmpDemuxer() override = default;

    static size_t trackCount(const AMFValue &metadata);

    // 从MetaData中创建Track
    bool loadMetaData(const AMFValue &metadata);

    /**
     * 解复用RTMP包，必要时创建Track
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
    // 从Metadata中获取，否则为0
    float _duration = 0;
    // 限制了最多只能一路音视频流
    bool _try_get_video_track = false;
    bool _try_get_audio_track = false;
    AudioTrack::Ptr _audio_track;
    VideoTrack::Ptr _video_track;
    RtmpCodec::Ptr _audio_rtmp_decoder;
    RtmpCodec::Ptr _video_rtmp_decoder;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPDEMUXER_H_ */
