/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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

using namespace toolkit;

namespace mediakit {

class RtmpDemuxer : public Demuxer{
public:
    typedef std::shared_ptr<RtmpDemuxer> Ptr;

    RtmpDemuxer() = default;
    virtual ~RtmpDemuxer() = default;

    bool loadMetaData(const AMFValue &metadata);

    /**
     * 开始解复用
     * @param pkt rtmp包
     * @return true 代表是i帧
     */
    bool inputRtmp(const RtmpPacket::Ptr &pkt);
private:
    void makeVideoTrack(const AMFValue &val);
    void makeAudioTrack(const AMFValue &val, int sample_rate, int channels, int sample_bit);
private:
    bool _tryedGetVideoTrack = false;
    bool _tryedGetAudioTrack = false;
    RtmpCodec::Ptr _audioRtmpDecoder;
    RtmpCodec::Ptr _videoRtmpDecoder;
};

} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPDEMUXER_H_ */
