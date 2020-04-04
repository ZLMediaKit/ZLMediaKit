/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "H265Rtmp.h"
#ifdef ENABLE_MP4
#include "mpeg4-hevc.h"
#endif//ENABLE_MP4

namespace mediakit{

H265RtmpDecoder::H265RtmpDecoder() {
    _h265frame = obtainFrame();
}

H265Frame::Ptr  H265RtmpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = obtainObj();
    frame->_buffer.clear();
    frame->_prefix_size = 4;
    return frame;
}

bool H265RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos) {
    return decodeRtmp(rtmp);
}

#ifdef ENABLE_MP4
/**
 * 返回不带0x00 00 00 01头的sps
 * @return
 */
static bool getH265ConfigFrame(const RtmpPacket &thiz,string &frame) {
    if (thiz.getMediaType() != FLV_CODEC_H265) {
        return false;
    }
    if (!thiz.isCfgFrame()) {
        return false;
    }
    if (thiz.strBuf.size() < 6) {
        WarnL << "bad H265 cfg!";
        return false;
    }

    auto extra = thiz.strBuf.data() + 5;
    auto bytes = thiz.strBuf.size() - 5;

    struct mpeg4_hevc_t hevc = {0};
    if (mpeg4_hevc_decoder_configuration_record_load((uint8_t *) extra, bytes, &hevc) > 0) {
        uint8_t config[1024] = {0};
        int size = mpeg4_hevc_to_nalu(&hevc, config, sizeof(config));
        if (size > 4) {
            frame.assign((char *) config + 4, size - 4);
            return true;
        }
    }

    return false;
}
#endif

bool H265RtmpDecoder::decodeRtmp(const RtmpPacket::Ptr &pkt) {
    if (pkt->isCfgFrame()) {
#ifdef ENABLE_MP4
        string config;
        if(getH265ConfigFrame(*pkt,config)){
            onGetH265(config.data(), config.size(), pkt->timeStamp , pkt->timeStamp);
        }
#else
        WarnL << "请开启MP4相关功能并使能\"ENABLE_MP4\",否则对H265-RTMP支持不完善";
#endif
        return false;
    }

    if (pkt->strBuf.size() > 9) {
        uint32_t iTotalLen = pkt->strBuf.size();
        uint32_t iOffset = 5;
        uint8_t *cts_ptr = (uint8_t *) (pkt->strBuf.data() + 2);
        int32_t cts = (((cts_ptr[0] << 16) | (cts_ptr[1] << 8) | (cts_ptr[2])) + 0xff800000) ^ 0xff800000;
        auto pts = pkt->timeStamp + cts;

        while(iOffset + 4 < iTotalLen){
            uint32_t iFrameLen;
            memcpy(&iFrameLen, pkt->strBuf.data() + iOffset, 4);
            iFrameLen = ntohl(iFrameLen);
            iOffset += 4;
            if(iFrameLen + iOffset > iTotalLen){
                break;
            }
            onGetH265(pkt->strBuf.data() + iOffset, iFrameLen, pkt->timeStamp , pts);
            iOffset += iFrameLen;
        }
    }
    return  pkt->isVideoKeyFrame();
}

inline void H265RtmpDecoder::onGetH265(const char* pcData, int iLen, uint32_t dts,uint32_t pts) {
    if(iLen == 0){
        return;
    }
#if 1
    _h265frame->_dts = dts;
    _h265frame->_pts = pts;
    _h265frame->_buffer.assign("\x0\x0\x0\x1", 4);  //添加265头
    _h265frame->_buffer.append(pcData, iLen);

    //写入环形缓存
    RtmpCodec::inputFrame(_h265frame);
    _h265frame = obtainFrame();
#else
    //防止内存拷贝，这样产生的265帧不会有0x00 00 01头
    auto frame = std::make_shared<H265FrameNoCacheAble>((char *)pcData,iLen,dts,pts,0);
    RtmpCodec::inputFrame(frame);
#endif
}

////////////////////////////////////////////////////////////////////////

H265RtmpEncoder::H265RtmpEncoder(const Track::Ptr &track) {
    _track = dynamic_pointer_cast<H265Track>(track);
}

void H265RtmpEncoder::makeConfigPacket(){
    if (_track && _track->ready()) {
        //尝试从track中获取sps pps信息
        _sps = _track->getSps();
        _pps = _track->getPps();
        _vps = _track->getVps();
    }

    if (!_sps.empty() && !_pps.empty() && !_vps.empty()) {
        //获取到sps/pps
        makeVideoConfigPkt();
        _gotSpsPps = true;
    }
}

void H265RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto pcData = frame->data() + frame->prefixSize();
    auto iLen = frame->size() - frame->prefixSize();
    auto type = H265_TYPE(((uint8_t*)pcData)[0]);

    if (!_gotSpsPps) {
        //尝试从frame中获取sps pps
        switch (type) {
            case H265Frame::NAL_SPS: {
                //sps
                _sps = string(pcData, iLen);
                makeConfigPacket();
                break;
            }
            case H265Frame::NAL_PPS: {
                //pps
                _pps = string(pcData, iLen);
                makeConfigPacket();
                break;
            }
            case H265Frame::NAL_VPS: {
                //vps
                _vps = string(pcData, iLen);
                makeConfigPacket();
                break;
            }
            default:
                break;
        }
    }

    if(type == H265Frame::NAL_SEI_PREFIX || type == H265Frame::NAL_SEI_SUFFIX){
        return;
    }

    if(_lastPacket && _lastPacket->timeStamp != frame->dts()) {
        RtmpCodec::inputRtmp(_lastPacket, _lastPacket->isVideoKeyFrame());
        _lastPacket = nullptr;
    }

    if(!_lastPacket) {
        //I or P or B frame
        int8_t flags = FLV_CODEC_H265;
        bool is_config = false;
        flags |= (((frame->configFrame() || frame->keyFrame()) ? FLV_KEY_FRAME : FLV_INTER_FRAME) << 4);

        _lastPacket = ResourcePoolHelper<RtmpPacket>::obtainObj();
        _lastPacket->strBuf.clear();
        _lastPacket->strBuf.push_back(flags);
        _lastPacket->strBuf.push_back(!is_config);
        auto cts = frame->pts() - frame->dts();
        cts = htonl(cts);
        _lastPacket->strBuf.append((char *)&cts + 1, 3);

        _lastPacket->chunkId = CHUNK_VIDEO;
        _lastPacket->streamId = STREAM_MEDIA;
        _lastPacket->timeStamp = frame->dts();
        _lastPacket->typeId = MSG_VIDEO;

    }
    auto size = htonl(iLen);
    _lastPacket->strBuf.append((char *) &size, 4);
    _lastPacket->strBuf.append(pcData, iLen);
    _lastPacket->bodySize = _lastPacket->strBuf.size();
}

void H265RtmpEncoder::makeVideoConfigPkt() {
#ifdef ENABLE_MP4
    int8_t flags = FLV_CODEC_H265;
    flags |= (FLV_KEY_FRAME << 4);
    bool is_config = true;

    RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
    rtmpPkt->strBuf.clear();

    //header
    rtmpPkt->strBuf.push_back(flags);
    rtmpPkt->strBuf.push_back(!is_config);
    //cts
    rtmpPkt->strBuf.append("\x0\x0\x0", 3);

    struct mpeg4_hevc_t hevc = {0};
    string vps_sps_pps = string("\x00\x00\x00\x01", 4) + _vps +
                         string("\x00\x00\x00\x01", 4) + _sps +
                         string("\x00\x00\x00\x01", 4) + _pps;
    h265_annexbtomp4(&hevc, vps_sps_pps.data(), vps_sps_pps.size(), NULL, 0, NULL, NULL);
    uint8_t extra_data[1024];
    int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, extra_data, sizeof(extra_data));
    if (extra_data_size == -1) {
        WarnL << "生成H265 extra_data 失败";
        return;
    }

    //HEVCDecoderConfigurationRecord
    rtmpPkt->strBuf.append((char *)extra_data, extra_data_size);

    rtmpPkt->bodySize = rtmpPkt->strBuf.size();
    rtmpPkt->chunkId = CHUNK_VIDEO;
    rtmpPkt->streamId = STREAM_MEDIA;
    rtmpPkt->timeStamp = 0;
    rtmpPkt->typeId = MSG_VIDEO;
    RtmpCodec::inputRtmp(rtmpPkt, false);
#else
    WarnL << "请开启MP4相关功能并使能\"ENABLE_MP4\",否则对H265-RTMP支持不完善";
#endif
}

}//namespace mediakit
