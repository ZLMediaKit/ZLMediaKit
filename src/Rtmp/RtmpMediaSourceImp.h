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

#ifndef SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_
#define SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_

#include <mutex>
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include "Util/util.h"
#include "Util/logger.h"
#include "amf.h"
#include "Rtmp.h"
#include "RtmpMediaSource.h"
#include "RtmpDemuxer.h"
#include "Common/MultiMediaSourceMuxer.h"
using namespace std;
using namespace toolkit;

namespace mediakit {
class RtmpMediaSourceImp: public RtmpMediaSource, public Demuxer::Listener , public MultiMediaSourceMuxer::Listener {
public:
    typedef std::shared_ptr<RtmpMediaSourceImp> Ptr;

    /**
     * 构造函数
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param id 流id
     * @param ringSize 环形缓存大小
     */
    RtmpMediaSourceImp(const string &vhost, const string &app, const string &id, int ringSize = RTMP_GOP_SIZE) : RtmpMediaSource(vhost, app, id, ringSize) {
        _demuxer = std::make_shared<RtmpDemuxer>();
        _demuxer->setTrackListener(this);
    }

    ~RtmpMediaSourceImp() = default;

    /**
     * 设置metadata
     */
    void setMetaData(const AMFValue &metadata) override{
        _demuxer->loadMetaData(metadata);
        RtmpMediaSource::setMetaData(metadata);
    }

    /**
     * 输入rtmp并解析
     */
    void onWrite(const RtmpPacket::Ptr &pkt,bool key_pos = true) override {
        key_pos = _demuxer->inputRtmp(pkt);
        RtmpMediaSource::onWrite(pkt,key_pos);
    }

    /**
     * 设置监听器
     * @param listener
     */
    void setListener(const std::weak_ptr<MediaSourceEvent> &listener) override {
        RtmpMediaSource::setListener(listener);
        if(_muxer){
            _muxer->setListener(listener);
        }
    }

    /**
     * 获取观看总人数，包括(hls/rtsp/rtmp)
     */
    int totalReaderCount() override{
        return readerCount() + (_muxer ? _muxer->totalReaderCount() : 0);
    }

    /**
     * 设置协议转换
     * @param enableRtsp 是否转换成rtsp
     * @param enableHls  是否转换成hls
     * @param enableMP4  是否mp4录制
     */
    void setProtocolTranslation(bool enableRtsp, bool enableHls, bool enableMP4) {
        //不重复生成rtmp
        _muxer = std::make_shared<MultiMediaSourceMuxer>(getVhost(), getApp(), getId(), _demuxer->getDuration(), enableRtsp, false, enableHls, enableMP4);
        _muxer->setListener(getListener());
        _muxer->setTrackListener(this);
        for(auto &track : _demuxer->getTracks(false)){
            _muxer->addTrack(track);
            track->addDelegate(_muxer);
        }
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
    RtmpDemuxer::Ptr _demuxer;
    MultiMediaSourceMuxer::Ptr _muxer;
};
} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPTORTSPMEDIASOURCE_H_ */
