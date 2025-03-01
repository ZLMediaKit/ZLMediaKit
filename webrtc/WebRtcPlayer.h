/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBRTCPLAYER_H
#define ZLMEDIAKIT_WEBRTCPLAYER_H

#include "Rtsp/RtspMediaSource.h"
#include "WebRtcTransport.h"

namespace mediakit {
/**
 * @brief H.264 B 帧过滤器
 * 用于从 H.264 RTP 流中移除 B 帧
 */
class H264BFrameFilter {
public:
    /**
     * ISO_IEC_14496-10-AVC-2012
     * Table 7-6 – Name association to slice_type
     */
    enum H264SliceType {
        H264SliceTypeP = 0,
        H264SliceTypeB = 1,
        H264SliceTypeI = 2,
        H264SliceTypeSP = 3,
        H264SliceTypeSI = 4,
        H264SliceTypeP1 = 5,
        H264SliceTypeB1 = 6,
        H264SliceTypeI1 = 7,
        H264SliceTypeSP1 = 8,
        H264SliceTypeSI1 = 9,
    };

    enum H264NALUType {
        NAL_NIDR = 1,
        NAL_PARTITION_A = 2,
        NAL_PARTITION_B = 3,
        NAL_PARTITION_C = 4,
        NAL_IDR = 5,
    };

    H264BFrameFilter();

    ~H264BFrameFilter() = default;

    /**
     * @brief 处理单个 RTP 包，移除 B 帧
     * @param packet 输入的 RTP 包
     * @return 如果不是 B 帧则返回原包，否则返回 nullptr
     */
    RtpPacket::Ptr processPacket(const RtpPacket::Ptr &packet);

private:
    /**
     * @brief 判断 RTP 包是否包含 H.264 的 B 帧
     * @param packet RTP 包
     * @return 如果是 B 帧返回 true，否则返回 false
     */
    bool isH264BFrame(const RtpPacket::Ptr &packet) const;

    /**
     * @brief 根据 NAL 类型和数据判断是否是 B 帧
     * @param nal_type NAL 单元类型
     * @param data NAL 单元数据（不含 NAL 头）
     * @param size 数据大小
     * @return 如果是 B 帧返回 true，否则返回 false
     */
    bool isBFrameByNalType(uint8_t nal_type, const uint8_t *data, size_t size) const;

    /**
     * @brief 解析指数哥伦布编码
     * @param data 数据缓冲区
     * @param size 缓冲区大小
     * @param bits_offset 位偏移量
     * @return 解析出的数值
     */
    int decodeExpGolomb(const uint8_t *data, size_t size, size_t &bitPos) const;

    /**
     * @brief 从比特流中读取位
     * @param data 数据缓冲区
     * @param size 缓冲区大小
     * @return 读取的位值（0 或 1）
     */
    int getBit(const uint8_t *data, size_t size) const;

    /**
     * @brief 提取切片类型值
     * @param data 数据缓冲区
     * @param size 缓冲区大小
     * @return 切片类型值
     */
    uint8_t extractSliceType(const uint8_t *data, size_t size) const;

    /**
     * @brief 处理FU-A分片
     * @param payload 数据缓冲区
     * @param payload_size 缓冲区大小
     * @return 如果是 B 帧返回 true，否则返回 false
     */
    bool handleFua(const uint8_t *payload, size_t payload_size) const;

    /**
   * @brief 处理 STAP-A 组合包
   * @param payload 数据缓冲区
   * @param payload_size 缓冲区大小
   * @return 如果是 B 帧返回 true，否则返回 false
   */
    bool handleStapA(const uint8_t *payload, size_t payload_size) const;


private:
    uint16_t _last_seq; // 维护输出流的序列号
    uint32_t _last_stamp; // 维护输出流的时间戳
    bool _first_packet; // 是否是第一个包的标记
};

class WebRtcPlayer : public WebRtcTransportImp {
public:
    using Ptr = std::shared_ptr<WebRtcPlayer>;
    static Ptr create(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info);
    MediaInfo getMediaInfo() { return _media_info; }

protected:
    ///////WebRtcTransportImp override///////
    void onStartWebRTC() override;
    void onDestory() override;
    void onRtcConfigure(RtcConfigure &configure) const override;

private:
    WebRtcPlayer(const EventPoller::Ptr &poller, const RtspMediaSource::Ptr &src, const MediaInfo &info);

    void sendConfigFrames(uint32_t before_seq, uint32_t sample_rate, uint32_t timestamp, uint64_t ntp_timestamp);

private:
    // 媒体相关元数据  [AUTO-TRANSLATED:f4cf8045]
    // Media related metadata
    MediaInfo _media_info;
    // 播放的rtsp源  [AUTO-TRANSLATED:9963eed1]
    // Playing rtsp source
    std::weak_ptr<RtspMediaSource> _play_src;

    // rtp 直接转发情况下通常会缺少 sps/pps, 在转发 rtp 前, 先发送一次相关帧信息, 部分情况下是可以播放的  [AUTO-TRANSLATED:65fdf16a]
    // In the case of direct RTP forwarding, sps/pps is usually missing. Before forwarding RTP, send the relevant frame information once. In some cases, it can be played.
    bool _send_config_frames_once { false };

    // 播放rtsp源的reader对象  [AUTO-TRANSLATED:7b305055]
    // Reader object for playing rtsp source
    RtspMediaSource::RingType::RingReader::Ptr _reader;

    bool _is_h264 { false };
    bool _bfliter_flag { false };
    std::shared_ptr<H264BFrameFilter> _bfilter;
};

}// namespace mediakit
#endif // ZLMEDIAKIT_WEBRTCPLAYER_H
