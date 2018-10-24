//
// Created by xzl on 2018/10/18.
//

#include "H264RtmpCodec.h"


H264RtmpDecoder::H264RtmpDecoder() {
    m_h264frame = obtainFrame();
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
        m_sps = pkt->getH264SPS();
        m_pps  = pkt->getH264PPS();
        return false;
    }

    if (m_sps.size()) {
        uint32_t iTotalLen = pkt->strBuf.size();
        uint32_t iOffset = 5;
        while(iOffset + 4 < iTotalLen){
            uint32_t iFrameLen;
            memcpy(&iFrameLen, pkt->strBuf.data() + iOffset, 4);
            iFrameLen = ntohl(iFrameLen);
            iOffset += 4;
            if(iFrameLen + iOffset > iTotalLen){
                break;
            }
            onGetH264_l(pkt->strBuf.data() + iOffset, iFrameLen, pkt->timeStamp);
            iOffset += iFrameLen;
        }
    }
    return  pkt->isVideoKeyFrame();
}


inline void H264RtmpDecoder::onGetH264_l(const char* pcData, int iLen, uint32_t ui32TimeStamp) {
    switch (pcData[0] & 0x1F) {
        case 5: {
            //I frame
            onGetH264(m_sps.data(), m_sps.length(), ui32TimeStamp);
            onGetH264(m_pps.data(), m_pps.length(), ui32TimeStamp);
        }
        case 1: {
            //I or P or B frame
            onGetH264(pcData, iLen, ui32TimeStamp);
        }
            break;
        default:
            //WarnL <<(int)(pcData[0] & 0x1F);
            break;
    }
}
inline void H264RtmpDecoder::onGetH264(const char* pcData, int iLen, uint32_t ui32TimeStamp) {
    m_h264frame->type = pcData[0] & 0x1F;
    m_h264frame->timeStamp = ui32TimeStamp;
    m_h264frame->buffer.assign("\x0\x0\x0\x1", 4);  //添加264头
    m_h264frame->buffer.append(pcData, iLen);

    //写入环形缓存
    RtmpCodec::inputFrame(m_h264frame);
    m_h264frame = obtainFrame();
}



////////////////////////////////////////////////////////////////////////

H264RtmpEncoder::H264RtmpEncoder()  {
}

void H264RtmpEncoder::inputFrame(const Frame::Ptr &frame) {
    RtmpCodec::inputFrame(frame);

    auto pcData = frame->data() + frame->prefixSize();
    auto iLen = frame->size() - frame->prefixSize();
    auto type = ((uint8_t*)pcData)[0] & 0x1F;

    switch (type){
        case 7:{
            //sps
            if(m_sps.empty()){
                m_sps = string(pcData,iLen);
                if(!m_pps.empty()){
                    makeVideoConfigPkt();
                }
            }
        }
            break;
        case 8:{
            //pps
            if(m_pps.empty()){
                m_pps = string(pcData,iLen);
                if(!m_sps.empty()){
                    makeVideoConfigPkt();
                }
            }
        }
            break;
        case 1:
        case 5:{
            //I or P or B frame
            int8_t flags = 7; //h.264
            bool is_config = false;
            bool keyFrame = frame->keyFrame();
            flags |= ((keyFrame ? FLV_KEY_FRAME : FLV_INTER_FRAME) << 4);

            RtmpPacket::Ptr rtmpPkt = ResourcePoolHelper<RtmpPacket>::obtainObj();
            rtmpPkt->strBuf.push_back(flags);
            rtmpPkt->strBuf.push_back(!is_config);
            rtmpPkt->strBuf.append("\x0\x0\x0", 3);
            auto size = htonl(iLen);
            rtmpPkt->strBuf.append((char *) &size, 4);
            rtmpPkt->strBuf.append(pcData, iLen);

            rtmpPkt->bodySize = rtmpPkt->strBuf.size();
            rtmpPkt->chunkId = CHUNK_VIDEO;
            rtmpPkt->streamId = STREAM_MEDIA;
            rtmpPkt->timeStamp = frame->stamp();
            rtmpPkt->typeId = MSG_VIDEO;
            RtmpCodec::inputRtmp(rtmpPkt,keyFrame);
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
    //////////header
    rtmpPkt->strBuf.push_back(flags);
    rtmpPkt->strBuf.push_back(!is_config);
    rtmpPkt->strBuf.append("\x0\x0\x0", 3);

    ////////////sps
    rtmpPkt->strBuf.push_back(1); // version

    //DebugL<<hexdump(m_sps.data(), m_sps.size());
    rtmpPkt->strBuf.push_back(m_sps[1]); // profile
    rtmpPkt->strBuf.push_back(m_sps[2]); // compat
    rtmpPkt->strBuf.push_back(m_sps[3]); // level
    rtmpPkt->strBuf.push_back(0xff); // 6 bits reserved + 2 bits nal size length - 1 (11)
    rtmpPkt->strBuf.push_back(0xe1); // 3 bits reserved + 5 bits number of sps (00001)
    uint16_t size = m_sps.size();
    size = htons(size);
    rtmpPkt->strBuf.append((char *) &size, 2);
    rtmpPkt->strBuf.append(m_sps);

    /////////////pps
    rtmpPkt->strBuf.push_back(1); // version
    size = m_pps.size();
    size = htons(size);
    rtmpPkt->strBuf.append((char *) &size, 2);
    rtmpPkt->strBuf.append(m_pps);

    rtmpPkt->bodySize = rtmpPkt->strBuf.size();
    rtmpPkt->chunkId = CHUNK_VIDEO;
    rtmpPkt->streamId = STREAM_MEDIA;
    rtmpPkt->timeStamp = 0;
    rtmpPkt->typeId = MSG_VIDEO;
    RtmpCodec::inputRtmp(rtmpPkt, true);
}





















