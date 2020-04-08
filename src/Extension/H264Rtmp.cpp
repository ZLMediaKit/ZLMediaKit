/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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

H264Frame::Ptr  H264RtmpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = obtainObj();
    frame->_buffer.clear();
    frame->_prefix_size = 4;
    return frame;
}

bool H264RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos) {
    return decodeRtmp(rtmp);
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
    if (thiz.strBuf.size() < 13) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    uint16_t sps_size ;
    memcpy(&sps_size, thiz.strBuf.data() + 11,2);
    sps_size = ntohs(sps_size);
    if ((int) thiz.strBuf.size() < 13 + sps_size) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    ret.assign(thiz.strBuf.data() + 13, sps_size);
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
    if (thiz.strBuf.size() < 13) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    uint16_t sps_size ;
    memcpy(&sps_size,thiz.strBuf.data() + 11,2);
    sps_size = ntohs(sps_size);

    if ((int) thiz.strBuf.size() < 13 + sps_size + 1 + 2) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    uint16_t pps_size ;
    memcpy(&pps_size, thiz.strBuf.data() + 13 + sps_size + 1,2);
    pps_size = ntohs(pps_size);

    if ((int) thiz.strBuf.size() < 13 + sps_size + 1 + 2 + pps_size) {
        WarnL << "bad H264 cfg!";
        return ret;
    }
    ret.assign(thiz.strBuf.data() + 13 + sps_size + 1 + 2, pps_size);
    return ret;
}

bool H264RtmpDecoder::decodeRtmp(const RtmpPacket::Ptr &pkt) {
    if (pkt->isCfgFrame()) {
        //缓存sps pps，后续插入到I帧之前
        _sps = getH264SPS(*pkt);
        _pps  = getH264PPS(*pkt);
        onGetH264(_sps.data(), _sps.size(), pkt->timeStamp , pkt->timeStamp);
        onGetH264(_pps.data(), _pps.size(), pkt->timeStamp , pkt->timeStamp);
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
            onGetH264(pkt->strBuf.data() + iOffset, iFrameLen, pkt->timeStamp , pts);
            iOffset += iFrameLen;
        }
    }
    return  pkt->isVideoKeyFrame();
}

inline void H264RtmpDecoder::onGetH264(const char* pcData, int iLen, uint32_t dts,uint32_t pts) {
    if(iLen == 0){
        return;
    }
#if 1
    _h264frame->_dts = dts;
    _h264frame->_pts = pts;
    _h264frame->_buffer.assign("\x0\x0\x0\x1", 4);  //添加264头
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
        _gotSpsPps = true;
    }
}

void H264RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto pcData = frame->data() + frame->prefixSize();
    auto iLen = frame->size() - frame->prefixSize();
    auto type = H264_TYPE(((uint8_t*)pcData)[0]);

    if (!_gotSpsPps) {
        //尝试从frame中获取sps pps
        switch (type) {
            case H264Frame::NAL_SPS: {
                //sps
                _sps = string(pcData, iLen);
                makeConfigPacket();
                break;
            }
            case H264Frame::NAL_PPS: {
                //pps
                _pps = string(pcData, iLen);
                makeConfigPacket();
                break;
            }
            default:
                break;
        }
    }

    if(type == H264Frame::NAL_SEI){
        return;
    }

    if(_lastPacket && _lastPacket->timeStamp != frame->dts()) {
        RtmpCodec::inputRtmp(_lastPacket, _lastPacket->isVideoKeyFrame());
        _lastPacket = nullptr;
    }

    if(!_lastPacket) {
        //I or P or B frame
        int8_t flags = FLV_CODEC_H264;
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

void H264RtmpEncoder::makeVideoConfigPkt() {
    int8_t flags = FLV_CODEC_H264;
    flags |= (FLV_KEY_FRAME << 4);
    bool is_config = true;

    RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
    rtmpPkt->strBuf.clear();

    //header
    rtmpPkt->strBuf.push_back(flags);
    rtmpPkt->strBuf.push_back(!is_config);
    //cts
    rtmpPkt->strBuf.append("\x0\x0\x0", 3);

    //AVCDecoderConfigurationRecord start
    rtmpPkt->strBuf.push_back(1); // version
    rtmpPkt->strBuf.push_back(_sps[1]); // profile
    rtmpPkt->strBuf.push_back(_sps[2]); // compat
    rtmpPkt->strBuf.push_back(_sps[3]); // level
    rtmpPkt->strBuf.push_back(0xff); // 6 bits reserved + 2 bits nal size length - 1 (11)
    rtmpPkt->strBuf.push_back(0xe1); // 3 bits reserved + 5 bits number of sps (00001)
    //sps
    uint16_t size = _sps.size();
    size = htons(size);
    rtmpPkt->strBuf.append((char *) &size, 2);
    rtmpPkt->strBuf.append(_sps);
    //pps
    rtmpPkt->strBuf.push_back(1); // version
    size = _pps.size();
    size = htons(size);
    rtmpPkt->strBuf.append((char *) &size, 2);
    rtmpPkt->strBuf.append(_pps);

    rtmpPkt->bodySize = rtmpPkt->strBuf.size();
    rtmpPkt->chunkId = CHUNK_VIDEO;
    rtmpPkt->streamId = STREAM_MEDIA;
    rtmpPkt->timeStamp = 0;
    rtmpPkt->typeId = MSG_VIDEO;
    RtmpCodec::inputRtmp(rtmpPkt, false);
}

}//namespace mediakit
