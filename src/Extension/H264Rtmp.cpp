/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H264Rtmp.h"
namespace mediakit{

H264RtmpDecoder::H264RtmpDecoder() {
    _h264frame = obtainFrame();
}

H264Frame::Ptr H264RtmpDecoder::obtainFrame() {
    auto frame = FrameImp::create<H264Frame>();
    frame->_prefix_size = 4;
    return frame;
}

/**
 * 返回不带0x00 00 00 01头的sps
 * @return
 */
static string getH264SPS(const RtmpPacket &thiz) {
    string ret;
    if (thiz.getMediaType() != FLV_CODEC_H264) {
        return ret;
    }
    if (!thiz.isCfgFrame()) {
        return ret;
    }
    if (thiz.buffer.size() < 13) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    uint16_t sps_size ;
    memcpy(&sps_size, thiz.buffer.data() + 11, 2);
    sps_size = ntohs(sps_size);
    if ((int) thiz.buffer.size() < 13 + sps_size) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    ret.assign(thiz.buffer.data() + 13, sps_size);
    return ret;
}

/**
 * 返回不带0x00 00 00 01头的pps
 * @return
 */
static string getH264PPS(const RtmpPacket &thiz) {
    string ret;
    if (thiz.getMediaType() != FLV_CODEC_H264) {
        return ret;
    }
    if (!thiz.isCfgFrame()) {
        return ret;
    }
    if (thiz.buffer.size() < 13) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    uint16_t sps_size ;
    memcpy(&sps_size, thiz.buffer.data() + 11, 2);
    sps_size = ntohs(sps_size);

    if ((int) thiz.buffer.size() < 13 + sps_size + 1 + 2) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    uint16_t pps_size ;
    memcpy(&pps_size, thiz.buffer.data() + 13 + sps_size + 1, 2);
    pps_size = ntohs(pps_size);

    if ((int) thiz.buffer.size() < 13 + sps_size + 1 + 2 + pps_size) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    ret.assign(thiz.buffer.data() + 13 + sps_size + 1 + 2, pps_size);
    return ret;
}

void H264RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &pkt) {
    if (pkt->isCfgFrame()) {
        //缓存sps pps，后续插入到I帧之前
        _sps = getH264SPS(*pkt);
        _pps  = getH264PPS(*pkt);
        onGetH264(_sps.data(), _sps.size(), pkt->time_stamp , pkt->time_stamp);
        onGetH264(_pps.data(), _pps.size(), pkt->time_stamp , pkt->time_stamp);
        return;
    }

    if (pkt->buffer.size() > 9) {
        auto total_len = pkt->buffer.size();
        size_t offset = 5;
        uint8_t *cts_ptr = (uint8_t *) (pkt->buffer.data() + 2);
        int32_t cts = (((cts_ptr[0] << 16) | (cts_ptr[1] << 8) | (cts_ptr[2])) + 0xff800000) ^ 0xff800000;
        auto pts = pkt->time_stamp + cts;
        while (offset + 4 < total_len) {
            uint32_t frame_len;
            memcpy(&frame_len, pkt->buffer.data() + offset, 4);
            frame_len = ntohl(frame_len);
            offset += 4;
            if (frame_len + offset > total_len) {
                break;
            }
            onGetH264(pkt->buffer.data() + offset, frame_len, pkt->time_stamp, pts);
            offset += frame_len;
        }
    }
}

inline void H264RtmpDecoder::onGetH264(const char* pcData, size_t iLen, uint32_t dts,uint32_t pts) {
    if(iLen == 0){
        return;
    }
#if 1
    _h264frame->_dts = dts;
    _h264frame->_pts = pts;
    _h264frame->_buffer.assign("\x00\x00\x00\x01", 4);  //添加264头
    _h264frame->_buffer.append(pcData, iLen);

    //写入环形缓存
    RtmpCodec::inputFrame(_h264frame);
    _h264frame = obtainFrame();
#else
    //防止内存拷贝，这样产生的264帧不会有0x00 00 01头
    auto frame = std::make_shared<H264FrameNoCacheAble>((char *)pcData,iLen,dts,pts,0);
    RtmpCodec::inputFrame(frame);
#endif
}

////////////////////////////////////////////////////////////////////////

H264RtmpEncoder::H264RtmpEncoder(const Track::Ptr &track) {
    _track = dynamic_pointer_cast<H264Track>(track);
}

void H264RtmpEncoder::makeConfigPacket(){
    if (_track && _track->ready()) {
        //尝试从track中获取sps pps信息
        _sps = _track->getSps();
        _pps = _track->getPps();
    }

    if (!_sps.empty() && !_pps.empty()) {
        //获取到sps/pps
        makeVideoConfigPkt();
        _got_config_frame = true;
    }
}

void H264RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto data = frame->data() + frame->prefixSize();
    auto len = frame->size() - frame->prefixSize();
    auto type = H264_TYPE(((uint8_t*)data)[0]);
    switch (type) {
        case H264Frame::NAL_SEI:
        case H264Frame::NAL_AUD: return;
        case H264Frame::NAL_SPS: {
            if (!_got_config_frame) {
                _sps = string(data, len);
                makeConfigPacket();
            }
            break;
        }
        case H264Frame::NAL_PPS: {
            if (!_got_config_frame) {
                _pps = string(data, len);
                makeConfigPacket();
            }
            break;
        }
        default : break;
    }

    if (frame->configFrame() && _rtmp_packet && _has_vcl) {
        //sps pps flush frame
        RtmpCodec::inputRtmp(_rtmp_packet);
        _has_vcl = false;
        _rtmp_packet = nullptr;
    }

    if (_rtmp_packet && (_rtmp_packet->time_stamp != frame->dts() || ((data[1] & 0x80) != 0 && type >= H264Frame::NAL_B_P && type <= H264Frame::NAL_IDR && _has_vcl))) {
        RtmpCodec::inputRtmp(_rtmp_packet);
        _has_vcl = false;
        _rtmp_packet = nullptr;
    }
    if (type >= H264Frame::NAL_B_P && type <= H264Frame::NAL_IDR) {
        _has_vcl = true;
    }
    if (!_rtmp_packet) {
        //I or P or B frame
        int8_t flags = FLV_CODEC_H264;
        bool is_config = false;
        flags |= (((frame->configFrame() || frame->keyFrame()) ? FLV_KEY_FRAME : FLV_INTER_FRAME) << 4);

        _rtmp_packet = RtmpPacket::create();
        _rtmp_packet->buffer.push_back(flags);
        _rtmp_packet->buffer.push_back(!is_config);
        int32_t cts = frame->pts() - frame->dts();
        if (cts < 0) {
            cts = 0;
        }
        cts = htonl(cts);
        _rtmp_packet->buffer.append((char *) &cts + 1, 3);
        _rtmp_packet->chunk_id = CHUNK_VIDEO;
        _rtmp_packet->stream_index = STREAM_MEDIA;
        _rtmp_packet->time_stamp = frame->dts();
        _rtmp_packet->type_id = MSG_VIDEO;
    }
    uint32_t size = htonl((uint32_t) len);
    _rtmp_packet->buffer.append((char *) &size, 4);
    _rtmp_packet->buffer.append(data, len);
    _rtmp_packet->body_size = _rtmp_packet->buffer.size();
}

void H264RtmpEncoder::makeVideoConfigPkt() {
    if (_sps.size() < 4) {
        WarnL << "sps长度不足4字节";
        return;
    }
    int8_t flags = FLV_CODEC_H264;
    flags |= (FLV_KEY_FRAME << 4);
    bool is_config = true;

    auto rtmpPkt = RtmpPacket::create();
    //header
    rtmpPkt->buffer.push_back(flags);
    rtmpPkt->buffer.push_back(!is_config);
    //cts
    rtmpPkt->buffer.append("\x0\x0\x0", 3);

    //AVCDecoderConfigurationRecord start
    rtmpPkt->buffer.push_back(1); // version
    rtmpPkt->buffer.push_back(_sps[1]); // profile
    rtmpPkt->buffer.push_back(_sps[2]); // compat
    rtmpPkt->buffer.push_back(_sps[3]); // level
    rtmpPkt->buffer.push_back((char)0xff); // 6 bits reserved + 2 bits nal size length - 1 (11)
    rtmpPkt->buffer.push_back((char)0xe1); // 3 bits reserved + 5 bits number of sps (00001)
    //sps
    uint16_t size = (uint16_t)_sps.size();
    size = htons(size);
    rtmpPkt->buffer.append((char *) &size, 2);
    rtmpPkt->buffer.append(_sps);
    //pps
    rtmpPkt->buffer.push_back(1); // version
    size = (uint16_t)_pps.size();
    size = htons(size);
    rtmpPkt->buffer.append((char *) &size, 2);
    rtmpPkt->buffer.append(_pps);

    rtmpPkt->body_size = rtmpPkt->buffer.size();
    rtmpPkt->chunk_id = CHUNK_VIDEO;
    rtmpPkt->stream_index = STREAM_MEDIA;
    rtmpPkt->time_stamp = 0;
    rtmpPkt->type_id = MSG_VIDEO;
    RtmpCodec::inputRtmp(rtmpPkt);
}

}//namespace mediakit
