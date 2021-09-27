/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_PSENCODER_H
#define ZLMEDIAKIT_PSENCODER_H
#if defined(ENABLE_RTPPROXY)
#include "mpeg-ps.h"
#include "Common/MediaSink.h"
#include "Common/Stamp.h"
#include "Extension/CommonRtp.h"
namespace mediakit{

//该类实现mpeg-ps容器格式的打包
class PSEncoder : public MediaSinkInterface {
public:
    PSEncoder();
    ~PSEncoder() override;

    /**
     * 添加音视频轨道
     */
    bool addTrack(const Track::Ptr &track) override;

    /**
     * 重置音视频轨道
     */
    void resetTracks() override;

    /**
     * 输入帧数据
     */
    bool inputFrame(const Frame::Ptr &frame) override;

protected:
    /**
     * 输出mpeg-ps的回调函数
     * @param stamp 时间戳，毫秒
     * @param packet 数据指针
     * @param bytes 数据长度
     */
    virtual void onPS(uint32_t stamp, void *packet, size_t bytes) = 0;

private:
    void init();
    //音视频时间戳同步用
    void stampSync();

private:
    struct track_info {
        int track_id = -1;
        Stamp stamp;
    };

private:
    uint32_t _timestamp = 0;
    BufferRaw::Ptr _buffer;
    std::shared_ptr<struct ps_muxer_t> _muxer;
    unordered_map<int, track_info> _codec_to_trackid;
    FrameMerger _frame_merger{FrameMerger::h264_prefix};
};

class PSEncoderImp : public PSEncoder{
public:
    PSEncoderImp(uint32_t ssrc, uint8_t payload_type = 96);
    ~PSEncoderImp() override;

protected:
    //rtp打包后回调
    virtual void onRTP(Buffer::Ptr rtp) = 0;

protected:
    void onPS(uint32_t stamp, void *packet, size_t bytes) override;

private:
    std::shared_ptr<CommonRtpEncoder> _rtp_encoder;
};

}//namespace mediakit
#endif //ENABLE_RTPPROXY
#endif //ZLMEDIAKIT_PSENCODER_H
