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

#ifndef ZLMEDIAKIT_RTMPMUXER_H
#define ZLMEDIAKIT_RTMPMUXER_H

#include "RtmpMetedata.h"
#include "Extension/Frame.h"
#include "Common/MediaSink.h"

namespace mediakit{

class RtmpMuxer : public MediaSink{
public:
    typedef std::shared_ptr<RtmpMuxer> Ptr;

    /**
     * 构造函数
     */
    RtmpMuxer(const TitleMete::Ptr &title);
    virtual ~RtmpMuxer(){}

    /**
     * 获取完整的SDP字符串
     * @return SDP字符串
     */
    const AMFValue &getMetedata() const ;

    /**
     * 获取rtmp环形缓存
     * @return
     */
    RtmpRingInterface::RingType::Ptr getRtmpRing() const;
protected:
    /**
   * 某track已经准备好，其ready()状态返回true，
   * 此时代表可以获取其例如sps pps等相关信息了
   * @param track
   */
    void onTrackReady(const Track::Ptr & track) override ;
private:
    RtmpRingInterface::RingType::Ptr _rtmpRing;
    AMFValue _metedata;
};


































} /* namespace mediakit */

#endif //ZLMEDIAKIT_RTMPMUXER_H
