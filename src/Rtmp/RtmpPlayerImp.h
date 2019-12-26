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

#ifndef SRC_RTMP_RTMPPLAYERIMP_H_
#define SRC_RTMP_RTMPPLAYERIMP_H_

#include <memory>
#include <functional>
#include "Common/config.h"
#include "RtmpPlayer.h"
#include "RtmpMediaSource.h"
#include "RtmpDemuxer.h"
#include "Poller/Timer.h"
#include "Util/TimeTicker.h"
using namespace toolkit;
using namespace mediakit::Client;

namespace mediakit {

class RtmpPlayerImp: public PlayerImp<RtmpPlayer,RtmpDemuxer> {
public:
    typedef std::shared_ptr<RtmpPlayerImp> Ptr;
    RtmpPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<RtmpPlayer,RtmpDemuxer>(poller){};
    virtual ~RtmpPlayerImp(){
        DebugL<<endl;
    };
    float getProgress() const override{
        if(getDuration() > 0){
            return getProgressMilliSecond() / (getDuration() * 1000);
        }
        return PlayerBase::getProgress();
    };
    void seekTo(float fProgress) override{
        fProgress = MAX(float(0),MIN(fProgress,float(1.0)));
        seekToMilliSecond(fProgress * getDuration() * 1000);
    };
    void play(const string &strUrl) override {
        PlayerImp<RtmpPlayer,RtmpDemuxer>::play(strUrl);
    }
private:
    //派生类回调函数
    bool onCheckMeta(const AMFValue &val) override {
        _pRtmpMediaSrc = dynamic_pointer_cast<RtmpMediaSource>(_pMediaSrc);
        if(_pRtmpMediaSrc){
            _pRtmpMediaSrc->setMetaData(val);
        }
        _delegate.reset(new RtmpDemuxer);
        _delegate->loadMetaData(val);
        return true;
    }
    void onMediaData(const RtmpPacket::Ptr &chunkData) override {
    	if(_pRtmpMediaSrc){
            _pRtmpMediaSrc->onWrite(chunkData);
        }
        if(!_delegate){
    	    //这个流没有metadata
            _delegate.reset(new RtmpDemuxer());
        }
        _delegate->inputRtmp(chunkData);
    }
private:
    RtmpMediaSource::Ptr _pRtmpMediaSrc;
};


} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPLAYERIMP_H_ */
