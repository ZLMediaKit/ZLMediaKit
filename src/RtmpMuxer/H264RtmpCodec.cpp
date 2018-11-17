/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "H264RtmpCodec.h"

namespace mediakit{

H264RtmpDecoder::H264RtmpDecoder() {
    _h264frame = obtainFrame();
}

H264Frame::Ptr  H264RtmpDecoder::obtainFrame() {
    //从缓存池重新申请对象，防止覆盖已经写入环形缓存的对象
    auto frame = obtainObj();
    frame->buffer.clear();
    frame->iPrefixSize = 4;
    return frame;
}

bool H264RtmpDecoder::inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos) {
    key_pos = decodeRtmp(rtmp);
    RtmpCodec::inputRtmp(rtmp, key_pos);
    return key_pos;
}

bool H264RtmpDecoder::decodeRtmp(const RtmpPacket::Ptr &pkt) {
    if (pkt->isCfgFrame()) {
        //缓存sps pps，后续插入到I帧之前
        _sps = pkt->getH264SPS();
        _pps  = pkt->getH264PPS();
        return false;
    }

    if (_sps.size() && pkt->strBuf.size() > 9) {
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
            onGetH264_l(pkt->strBuf.data() + iOffset, iFrameLen, pkt->timeStamp , pts);
            iOffset += iFrameLen;
        }
    }
    return  pkt->isVideoKeyFrame();
}


inline void H264RtmpDecoder::onGetH264_l(const char* pcData, int iLen, uint32_t dts,uint32_t pts) {
    switch (H264_TYPE(pcData[0])) {
        case H264Frame::NAL_IDR: {
            //I frame
            onGetH264(_sps.data(), _sps.length(), dts , pts);
            onGetH264(_pps.data(), _pps.length(), dts , pts);
        }
        case H264Frame::NAL_B_P: {
            //I or P or B frame
            onGetH264(pcData, iLen, dts , pts);
        }
            break;
        default:
            break;
    }
}
inline void H264RtmpDecoder::onGetH264(const char* pcData, int iLen, uint32_t dts,uint32_t pts) {
    _h264frame->type = H264_TYPE(pcData[0]);
    _h264frame->timeStamp = dts;
    _h264frame->ptsStamp = pts;
    _h264frame->buffer.assign("\x0\x0\x0\x1", 4);  //添加264头
    _h264frame->buffer.append(pcData, iLen);

    //写入环形缓存
    RtmpCodec::inputFrame(_h264frame);
    _h264frame = obtainFrame();
}



////////////////////////////////////////////////////////////////////////

H264RtmpEncoder::H264RtmpEncoder(const Track::Ptr &track) {
    _track = dynamic_pointer_cast<H264Track>(track);

}

void H264RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    RtmpCodec::inputFrame(frame);

    auto pcData = frame->data() + frame->prefixSize();
    auto iLen = frame->size() - frame->prefixSize();
    auto type = H264_TYPE(((uint8_t*)pcData)[0]);

    if(!_gotSpsPps){
        //尝试从frame中获取sps pps
        switch (type){
            case H264Frame::NAL_SPS:{
                //sps
                if(_sps.empty()){
                    _sps = string(pcData,iLen);
                }
            }
                break;
            case H264Frame::NAL_PPS:{
                //pps
                if(_pps.empty()){
                    _pps = string(pcData,iLen);
                }
            }
                break;
            default:
                break;
        }

        if(_track && _track->ready()){
            //尝试从track中获取sps pps信息
            _sps = _track->getSps();
            _pps = _track->getPps();
        }

        if(!_sps.empty() && !_pps.empty()){
            _gotSpsPps = true;
            makeVideoConfigPkt();
        }
    }

    switch (type){
        case H264Frame::NAL_IDR:
        case H264Frame::NAL_B_P:{
            if(_lastPacket && _lastPacket->timeStamp != frame->stamp()) {
                RtmpCodec::inputRtmp(_lastPacket, _lastPacket->isVideoKeyFrame());
                _lastPacket = nullptr;
            }

            if(!_lastPacket) {
                //I or P or B frame
                int8_t flags = 7; //h.264
                bool is_config = false;
                flags |= ((frame->keyFrame() ? FLV_KEY_FRAME : FLV_INTER_FRAME) << 4);

                _lastPacket = ResourcePoolHelper<RtmpPacket>::obtainObj();
                _lastPacket->strBuf.clear();
                _lastPacket->strBuf.push_back(flags);
                _lastPacket->strBuf.push_back(!is_config);
                auto cts = frame->pts() - frame->dts();
                cts = htonl(cts);
                _lastPacket->strBuf.append((char *)&cts + 1, 3);

                _lastPacket->chunkId = CHUNK_VIDEO;
                _lastPacket->streamId = STREAM_MEDIA;
                _lastPacket->timeStamp = frame->stamp();
                _lastPacket->typeId = MSG_VIDEO;

            }
            auto size = htonl(iLen);
            _lastPacket->strBuf.append((char *) &size, 4);
            _lastPacket->strBuf.append(pcData, iLen);
            _lastPacket->bodySize = _lastPacket->strBuf.size();
        }
            break;

        default:
            break;
    }
}


void H264RtmpEncoder::makeVideoConfigPkt() {
    int8_t flags = 7; //h.264
    flags |= (FLV_KEY_FRAME << 4);
    bool is_config = true;

    RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
    rtmpPkt->strBuf.clear();

    //////////header
    rtmpPkt->strBuf.push_back(flags);
    rtmpPkt->strBuf.push_back(!is_config);
    rtmpPkt->strBuf.append("\x0\x0\x0", 3);

    ////////////sps
    rtmpPkt->strBuf.push_back(1); // version

    //DebugL<<hexdump(_sps.data(), _sps.size());
    rtmpPkt->strBuf.push_back(_sps[1]); // profile
    rtmpPkt->strBuf.push_back(_sps[2]); // compat
    rtmpPkt->strBuf.push_back(_sps[3]); // level
    rtmpPkt->strBuf.push_back(0xff); // 6 bits reserved + 2 bits nal size length - 1 (11)
    rtmpPkt->strBuf.push_back(0xe1); // 3 bits reserved + 5 bits number of sps (00001)
    uint16_t size = _sps.size();
    size = htons(size);
    rtmpPkt->strBuf.append((char *) &size, 2);
    rtmpPkt->strBuf.append(_sps);

    /////////////pps
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





