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

namespace mediakit{

class RtmpMuxer{
public:
    typedef std::shared_ptr<RtmpMuxer> Ptr;

    /**
     * 构造函数
     */
    RtmpMuxer(const TitleMete::Ptr &title = nullptr) : _metedata(AMF_OBJECT){
        if(!title){
            _metedata = std::make_shared<TitleMete>()->getMetedata();
        }else{
            _metedata = title->getMetedata();
        }
        _rtmpRing = std::make_shared<RtmpRingInterface::RingType>();
    }
    virtual ~RtmpMuxer(){}

    /**
     * 添加音视频媒体
     * @param track 媒体描述
     */
    void addTrack(const Track::Ptr & track) ;

    /**
     * 获取完整的SDP字符串
     * @return SDP字符串
     */
    const AMFValue &getMetedata() const ;

    /**
     * 写入帧数据然后打包rtmp
     * @param frame 帧数据
     */
    void inputFrame(const Frame::Ptr &frame) ;

    /**
     * 也可以在外部打包好rtmp然后再写入
     * @param rtmp rtmp包
     * @param key_pos 是否为关键帧
     */
    bool inputRtmp(const RtmpPacket::Ptr &rtmp, bool key_pos = true);

    /**
     * 获取rtmp环形缓存
     * @return
     */
    RtmpRingInterface::RingType::Ptr getRtmpRing() const;

protected:
    virtual void onInited(){};
private:
    map<int,Track::Ptr> _track_map;
    map<int,function<void()> > _trackReadyCallback;
    RtmpRingInterface::RingType::Ptr _rtmpRing;
    AMFValue _metedata;
    bool  _inited = false;
};


































} /* namespace mediakit */

#endif //ZLMEDIAKIT_RTMPMUXER_H
