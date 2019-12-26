/*
* MIT License
*
* Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#ifndef SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_
#define SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_

#include "Rtmp/amf.h"
#include "RtspMediaSource.h"
#include "RtspDemuxer.h"
#include "Common/MultiMediaSourceMuxer.h"
using namespace toolkit;

namespace mediakit {
class RtspMediaSourceImp : public RtspMediaSource, public Demuxer::Listener , public MultiMediaSourceMuxer::Listener  {
public:
    typedef std::shared_ptr<RtspMediaSourceImp> Ptr;

    /**
	 * 构造函数
	 * @param vhost 虚拟主机
	 * @param app 应用名
	 * @param id 流id
	 * @param ringSize 环形缓存大小
	 */
    RtspMediaSourceImp(const string &vhost, const string &app, const string &id, int ringSize = 0) : RtspMediaSource(vhost, app, id,ringSize) {}
    ~RtspMediaSourceImp() = default;

    /**
     * 设置sdp
     */
    void setSdp(const string &strSdp) override {
        _demuxer = std::make_shared<RtspDemuxer>(strSdp);
        _demuxer->setTrackListener(this);
        RtspMediaSource::setSdp(strSdp);
    }

    /**
     * 输入rtp并解析
     */
    void onWrite(const RtpPacket::Ptr &rtp, bool key_pos) override {
        if (_demuxer) {
            key_pos = _demuxer->inputRtp(rtp);
        }
        RtspMediaSource::onWrite(rtp, key_pos);
    }

    /**
	 * 设置监听器
	 * @param listener
	 */
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override {
        RtspMediaSource::setListener(listener);
        if(_muxer){
            _muxer->setListener(listener);
        }
    }

    /**
	 * 播放器总数
	 */
    int readerCount() override {
        return RtspMediaSource::readerCount() + (_muxer ? _muxer->readerCount() : 0);
    }

    /**
	 * 设置协议转换
	 * @param enableRtmp 是否转换成rtmp
	 * @param enableHls  是否转换成hls
	 * @param enableMP4  是否mp4录制
	 */
    void setProtocolTranslation(bool enableRtmp,bool enableHls,bool enableMP4){
        //不重复生成rtsp
        _muxer = std::make_shared<MultiMediaSourceMuxer>(getVhost(), getApp(), getId(), 0, false, enableRtmp, enableHls, enableMP4);
        _muxer->setListener(getListener());
        _muxer->setTrackListener(this);
    }

    /**
	 * _demuxer触发的添加Track事件
	 */
    void onAddTrack(const Track::Ptr &track) override {
        if(_muxer){
            _muxer->addTrack(track);
            track->addDelegate(_muxer);
        }
    }

    /**
     * _muxer触发的所有Track就绪的事件
     */
    void onAllTrackReady() override{
        setTrackSource(_muxer);
    }
private:
    RtspDemuxer::Ptr _demuxer;
    MultiMediaSourceMuxer::Ptr _muxer;
};
} /* namespace mediakit */

#endif /* SRC_RTSP_RTSPTORTMPMEDIASOURCE_H_ */
