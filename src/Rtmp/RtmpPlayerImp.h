/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
            _set_meta_data = true;
        }
        _delegate.reset(new RtmpDemuxer);
        _delegate->loadMetaData(val);
        return true;
    }
    void onMediaData(const RtmpPacket::Ptr &chunkData) override {
        if(_pRtmpMediaSrc){
            if(!_set_meta_data && !chunkData->isCfgFrame()){
                _set_meta_data = true;
                _pRtmpMediaSrc->setMetaData(TitleMeta().getMetadata());
            }
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
    bool _set_meta_data = false;
};


} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPLAYERIMP_H_ */
