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

#ifndef ZLMEDIAKIT_RTPRECEIVER_H
#define ZLMEDIAKIT_RTPRECEIVER_H


#include <map>
#include <string>
#include <memory>
#include "Rtsp.h"
#include "RtspMuxer/RtpCodec.h"
#include "RtspMediaSource.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

class RtpReceiver {
public:
    RtpReceiver();
    virtual ~RtpReceiver();
protected:

    /**
     * 输入数据指针生成并排序rtp包
     * @param iTrackidx track下标索引
     * @param track  sdp track相关信息
     * @param pucData rtp数据指针
     * @param uiLen rtp数据指针长度
     * @return 解析成功返回true
     */
    bool handleOneRtp(int iTrackidx,SdpTrack::Ptr &track, unsigned char *pucData, unsigned int uiLen);

    /**
     * rtp数据包排序后输出
     * @param rtppt rtp数据包
     * @param trackidx track索引
     */
    virtual void onRtpSorted(const RtpPacket::Ptr &rtppt, int trackidx){}
    void clear();
    void setPoolSize(int size);
private:
    uint32_t _aui32SsrcErrorCnt[2] = { 0, 0 };
    /* RTP包排序所用参数 */
    uint16_t _aui16LastSeq[2] = { 0 , 0 };
    uint64_t _aui64SeqOkCnt[2] = { 0 , 0};
    bool _abSortStarted[2] = { 0 , 0};
    map<uint32_t , RtpPacket::Ptr> _amapRtpSort[2];
    RtspMediaSource::PoolType _pktPool;
};

}//namespace mediakit


#endif //ZLMEDIAKIT_RTPRECEIVER_H
