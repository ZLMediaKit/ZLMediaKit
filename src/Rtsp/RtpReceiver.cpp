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

#include "Common/config.h"
#include "RtpReceiver.h"

#define POP_HEAD(trackidx) \
		auto it = _amapRtpSort[trackidx].begin(); \
		onRtpSorted(it->second, trackidx); \
		_amapRtpSort[trackidx].erase(it);

#   define AV_RB16(x)                           \
    ((((const uint8_t*)(x))[0] << 8) |          \
      ((const uint8_t*)(x))[1])


namespace mediakit {

RtpReceiver::RtpReceiver() {}
RtpReceiver::~RtpReceiver() {}

bool RtpReceiver::handleOneRtp(int iTrackidx,SdpTrack::Ptr &track, unsigned char *pucData, unsigned int uiLen) {
    auto pt_ptr=_pktPool.obtain();
    auto &rtppt=*pt_ptr;
    rtppt.interleaved = track->_interleaved;
    rtppt.length = uiLen + 4;

    rtppt.mark = pucData[1] >> 7;
    rtppt.PT = pucData[1] & 0x7F;
    //序列号
    memcpy(&rtppt.sequence,pucData+2,2);//内存对齐
    rtppt.sequence = ntohs(rtppt.sequence);
    //时间戳
    memcpy(&rtppt.timeStamp, pucData+4, 4);//内存对齐

    if(!track->_samplerate){
        return false;
    }
    //时间戳转换成毫秒
    rtppt.timeStamp = ntohl(rtppt.timeStamp) * 1000LL / track->_samplerate;
    //ssrc
    memcpy(&rtppt.ssrc,pucData+8,4);//内存对齐
    rtppt.ssrc = ntohl(rtppt.ssrc);
    rtppt.type = track->_type;
    if (track->_ssrc == 0) {
        track->_ssrc = rtppt.ssrc;
        //保存SSRC
    } else if (track->_ssrc != rtppt.ssrc) {
        //ssrc错误
        WarnL << "ssrc错误";
        if (_aui32SsrcErrorCnt[iTrackidx]++ > 10) {
            track->_ssrc = rtppt.ssrc;
            WarnL << "ssrc更换!";
        }
        return false;
    }
    _aui32SsrcErrorCnt[iTrackidx] = 0;

    rtppt.payload[0] = '$';
    rtppt.payload[1] = rtppt.interleaved;
    rtppt.payload[2] = (uiLen & 0xFF00) >> 8;
    rtppt.payload[3] = (uiLen & 0x00FF);

    rtppt.offset 	= 16;
    int csrc     	= pucData[0] & 0x0f;
    int ext      	= pucData[0] & 0x10;
    rtppt.offset 	+= 4 * csrc;
    if (ext) {
        if(uiLen < rtppt.offset){
            return false;
        }
        /* calculate the header extension length (stored as number of 32-bit words) */
        ext = (AV_RB16(pucData + rtppt.offset - 2) + 1) << 2;
        rtppt.offset += ext;
    }

    memcpy(rtppt.payload + 4, pucData, uiLen);

    /////////////////////////////////RTP排序逻辑///////////////////////////////////
    if(rtppt.sequence != (uint16_t)(_aui16LastSeq[iTrackidx] + 1) && _aui16LastSeq[iTrackidx] != 0){
        //包乱序或丢包
        _aui64SeqOkCnt[iTrackidx] = 0;
        _abSortStarted[iTrackidx] = true;
        //WarnL << "包乱序或丢包:" << trackidx <<" " << rtppt.sequence << " " << _aui16LastSeq[trackidx];
    }else{
        //正确序列的包
        _aui64SeqOkCnt[iTrackidx]++;
    }
    _aui16LastSeq[iTrackidx] = rtppt.sequence;

    //开始排序缓存
    if (_abSortStarted[iTrackidx]) {
        _amapRtpSort[iTrackidx].emplace(rtppt.sequence, pt_ptr);
        GET_CONFIG_AND_REGISTER(uint32_t,clearCount,Rtp::kClearCount);
        GET_CONFIG_AND_REGISTER(uint32_t,maxRtpCount,Rtp::kMaxRtpCount);
        if (_aui64SeqOkCnt[iTrackidx] >= clearCount) {
            //网络环境改善，需要清空排序缓存
            _aui64SeqOkCnt[iTrackidx] = 0;
            _abSortStarted[iTrackidx] = false;
            while (_amapRtpSort[iTrackidx].size()) {
                POP_HEAD(iTrackidx)
            }
        } else if (_amapRtpSort[iTrackidx].size() >= maxRtpCount) {
            //排序缓存溢出
            POP_HEAD(iTrackidx)
        }
    }else{
        //正确序列
        onRtpSorted(pt_ptr, iTrackidx);
    }
    //////////////////////////////////////////////////////////////////////////////////
    return true;
}

void RtpReceiver::clear() {
    CLEAR_ARR(_aui16LastSeq)
    CLEAR_ARR(_aui32SsrcErrorCnt)
    CLEAR_ARR(_aui64SeqOkCnt)
    CLEAR_ARR(_abSortStarted)

    _amapRtpSort[0].clear();
    _amapRtpSort[1].clear();
}

void RtpReceiver::setPoolSize(int size) {
    _pktPool.setSize(size);
}

}//namespace mediakit
