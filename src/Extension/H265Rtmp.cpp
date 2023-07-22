/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Rtmp/utils.h"
#include "H265Rtmp.h"
#ifdef ENABLE_MP4
#include "mpeg4-hevc.h"
#endif // ENABLE_MP4

using namespace std;
using namespace toolkit;

namespace mediakit {

H265RtmpDecoder::H265RtmpDecoder() {
    _h265frame = obtainFrame();
}

H265Frame::Ptr H265RtmpDecoder::obtainFrame() {
    auto frame = FrameImp::create<H265Frame>();
    frame->_prefix_size = 4;
    return frame;
}

#ifdef ENABLE_MP4

static bool decode_HEVCDecoderConfigurationRecord(uint8_t *extra, size_t bytes, string &frame) {
    struct mpeg4_hevc_t hevc;
    memset(&hevc, 0, sizeof(hevc));
    if (mpeg4_hevc_decoder_configuration_record_load((uint8_t *)extra, bytes, &hevc) > 0) {
        uint8_t *config = new uint8_t[bytes * 2];
        int size = mpeg4_hevc_to_nalu(&hevc, config, bytes * 2);
        if (size > 4) {
            frame.assign((char *)config + 4, size - 4);
        }
        delete[] config;
        return size > 4;
    }
    return false;
}

/**
 * 返回不带0x00 00 00 01头的sps
 */
static bool getH265ConfigFrame(const RtmpPacket &thiz, string &frame) {
    if ((RtmpVideoCodec)thiz.getRtmpCodecId() != RtmpVideoCodec::h265) {
        return false;
    }
    if (thiz.buffer.size() < 6) {
        WarnL << "bad H265 cfg!";
        return false;
    }
    return decode_HEVCDecoderConfigurationRecord((uint8_t *)thiz.buffer.data() + 5, thiz.buffer.size() - 5, frame);
}
#endif

void H265RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt) {
    if (_info.codec == CodecInvalid) {
        // 先判断是否为增强型rtmp
        parseVideoRtmpPacket((uint8_t *)pkt->data(), pkt->size(), &_info);
    }

    if (_info.is_enhanced) {
        // 增强型rtmp
        parseVideoRtmpPacket((uint8_t *)pkt->data(), pkt->size(), &_info);
        if (!_info.is_enhanced || _info.codec != CodecH265) {
            throw std::invalid_argument("Invalid enhanced-rtmp hevc packet!");
        }
        auto data = (uint8_t *)pkt->data() + 5;
        auto size = pkt->size() - 5;
        switch (_info.video.pkt_type) {
            case RtmpPacketType::PacketTypeSequenceStart: {
#ifdef ENABLE_MP4
                string config;
                if (decode_HEVCDecoderConfigurationRecord(data, size, config)) {
                    onGetH265(config.data(), config.size(), pkt->time_stamp, pkt->time_stamp);
                }
#else
                WarnL << "请开启MP4相关功能并使能\"ENABLE_MP4\",否则对H265-RTMP支持不完善";
#endif
                break;
            }

            case RtmpPacketType::PacketTypeCodedFramesX:
            case RtmpPacketType::PacketTypeCodedFrames: {
                auto pts = pkt->time_stamp;
                if (RtmpPacketType::PacketTypeCodedFrames == _info.video.pkt_type) {
                    // SI24 = [CompositionTime Offset]
                    CHECK(size > 7);
                    int32_t cts = (((data[0] << 16) | (data[1] << 8) | (data[2])) + 0xff800000) ^ 0xff800000;
                    pts += cts;
                    data += 3;
                    size -= 3;
                }
                splitFrame(data, size, pkt->time_stamp, pts);
                break;
            }

            case RtmpPacketType::PacketTypeMetadata: {
                // The body does not contain video data. The body is an AMF encoded metadata.
                // The metadata will be represented by a series of [name, value] pairs.
                // For now the only defined [name, value] pair is [“colorInfo”, Object]
                // See Metadata Frame section for more details of this object.
                //
                // For a deeper understanding of the encoding please see description
                // of SCRIPTDATA and SSCRIPTDATAVALUE in the FLV file spec.
                // DATA = [“colorInfo”, Object]
                break;
            }
            case RtmpPacketType::PacketTypeSequenceEnd: {
                // signals end of sequence
                break;
            }
            default: break;
        }
        return;
    }

    // 国内扩展(12) H265 rtmp
    if (pkt->isConfigFrame()) {
#ifdef ENABLE_MP4
        string config;
        if (getH265ConfigFrame(*pkt, config)) {
            onGetH265(config.data(), config.size(), pkt->time_stamp, pkt->time_stamp);
        }
#else
        WarnL << "请开启MP4相关功能并使能\"ENABLE_MP4\",否则对H265-RTMP支持不完善";
#endif
        return;
    }

    if (pkt->buffer.size() > 9) {
        uint8_t *cts_ptr = (uint8_t *)(pkt->buffer.data() + 2);
        int32_t cts = (((cts_ptr[0] << 16) | (cts_ptr[1] << 8) | (cts_ptr[2])) + 0xff800000) ^ 0xff800000;
        auto pts = pkt->time_stamp + cts;
        splitFrame((uint8_t *)pkt->data() + 5, pkt->size() - 5, pkt->time_stamp, pts);
    }
}

void H265RtmpDecoder::splitFrame(const uint8_t *data, size_t size, uint32_t dts, uint32_t pts) {
    auto end = data + size;
    while (data + 4 < end) {
        uint32_t frame_len = load_be32(data);
        data += 4;
        if (data + frame_len > end) {
            break;
        }
        onGetH265((const char *)data, frame_len, dts, pts);
        data += frame_len;
    }
}

inline void H265RtmpDecoder::onGetH265(const char *data, size_t size, uint32_t dts, uint32_t pts) {
    if (size == 0) {
        return;
    }
#if 1
    _h265frame->_dts = dts;
    _h265frame->_pts = pts;
    _h265frame->_buffer.assign("\x00\x00\x00\x01", 4); // 添加265头
    _h265frame->_buffer.append(data, size);

    // 写入环形缓存
    RtmpCodec::inputFrame(_h265frame);
    _h265frame = obtainFrame();
#else
    // 防止内存拷贝，这样产生的265帧不会有0x00 00 01头
    auto frame = std::make_shared<H265FrameNoCacheAble>((char *)data, size, dts, pts, 0);
    RtmpCodec::inputFrame(frame);
#endif
}

////////////////////////////////////////////////////////////////////////

H265RtmpEncoder::H265RtmpEncoder(const Track::Ptr &track) {
    _track = dynamic_pointer_cast<H265Track>(track);
}

void H265RtmpEncoder::makeConfigPacket() {
    if (_track && _track->ready()) {
        // 尝试从track中获取sps pps信息
        _sps = _track->getSps();
        _pps = _track->getPps();
        _vps = _track->getVps();
    }

    if (!_sps.empty() && !_pps.empty() && !_vps.empty()) {
        // 获取到sps/pps
        makeVideoConfigPkt();
        _got_config_frame = true;
    }
}

void H265RtmpEncoder::flush() {
    inputFrame(nullptr);
}

bool H265RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    if (frame) {
        auto data = frame->data() + frame->prefixSize();
        auto len = frame->size() - frame->prefixSize();
        auto type = H265_TYPE(data[0]);
        switch (type) {
            case H265Frame::NAL_SPS: {
                if (!_got_config_frame) {
                    _sps = string(data, len);
                    makeConfigPacket();
                }
                break;
            }
            case H265Frame::NAL_PPS: {
                if (!_got_config_frame) {
                    _pps = string(data, len);
                    makeConfigPacket();
                }
                break;
            }
            case H265Frame::NAL_VPS: {
                if (!_got_config_frame) {
                    _vps = string(data, len);
                    makeConfigPacket();
                }
                break;
            }
            default: break;
        }
    }

    if (!_rtmp_packet) {
        _rtmp_packet = RtmpPacket::create();
        // flags/not_config/cts预占位
        _rtmp_packet->buffer.resize(5);
    }

    return _merger.inputFrame(frame, [this](uint64_t dts, uint64_t pts, const Buffer::Ptr &, bool have_key_frame) {
            // flags
            _rtmp_packet->buffer[0] = (uint8_t)RtmpVideoCodec::h265 | ((uint8_t)(have_key_frame ? RtmpFrameType::key_frame : RtmpFrameType::inter_frame) << 4);
            _rtmp_packet->buffer[1] = (uint8_t)RtmpH264PacketType::h264_nalu;
            int32_t cts = pts - dts;
            // cts
            set_be24(&_rtmp_packet->buffer[2], cts);
            _rtmp_packet->time_stamp = dts;
            _rtmp_packet->body_size = _rtmp_packet->buffer.size();
            _rtmp_packet->chunk_id = CHUNK_VIDEO;
            _rtmp_packet->stream_index = STREAM_MEDIA;
            _rtmp_packet->type_id = MSG_VIDEO;
            // 输出rtmp packet
            RtmpCodec::inputRtmp(_rtmp_packet);
            _rtmp_packet = nullptr;
        }, &_rtmp_packet->buffer);
}

void H265RtmpEncoder::makeVideoConfigPkt() {
#ifdef ENABLE_MP4
    auto flags = (uint8_t)RtmpVideoCodec::h265;
    flags |= ((uint8_t)RtmpFrameType::key_frame << 4);
    auto pkt = RtmpPacket::create();
    // header
    pkt->buffer.push_back(flags);
    pkt->buffer.push_back((uint8_t)RtmpH264PacketType::h264_config_header);
    // cts
    pkt->buffer.append("\x0\x0\x0", 3);

    struct mpeg4_hevc_t hevc;
    memset(&hevc, 0, sizeof(hevc));
    string vps_sps_pps = string("\x00\x00\x00\x01", 4) + _vps + string("\x00\x00\x00\x01", 4) + _sps + string("\x00\x00\x00\x01", 4) + _pps;
    h265_annexbtomp4(&hevc, vps_sps_pps.data(), (int)vps_sps_pps.size(), NULL, 0, NULL, NULL);
    uint8_t extra_data[1024];
    int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, extra_data, sizeof(extra_data));
    if (extra_data_size == -1) {
        WarnL << "生成H265 extra_data 失败";
        return;
    }
    // HEVCDecoderConfigurationRecord
    pkt->buffer.append((char *)extra_data, extra_data_size);
    pkt->body_size = pkt->buffer.size();
    pkt->chunk_id = CHUNK_VIDEO;
    pkt->stream_index = STREAM_MEDIA;
    pkt->time_stamp = 0;
    pkt->type_id = MSG_VIDEO;
    RtmpCodec::inputRtmp(pkt);
#else
    WarnL << "请开启MP4相关功能并使能\"ENABLE_MP4\",否则对H265-RTMP支持不完善";
#endif
}

} // namespace mediakit
