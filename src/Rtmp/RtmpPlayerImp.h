/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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

    RtmpPlayerImp(const EventPoller::Ptr &poller) : PlayerImp<RtmpPlayer, RtmpDemuxer>(poller) {};

    ~RtmpPlayerImp() override {
        DebugL << endl;
    }

    float getProgress() const override {
        if (getDuration() > 0) {
            return getProgressMilliSecond() / (getDuration() * 1000);
        }
        return PlayerBase::getProgress();
    }

    void seekTo(float fProgress) override {
        fProgress = MAX(float(0), MIN(fProgress, float(1.0)));
        seekToMilliSecond((uint32_t)(fProgress * getDuration() * 1000));
    }

    void seekTo(uint32_t seekPos) override {
        uint32_t pos = MAX(float(0), MIN(seekPos, getDuration())) * 1000;
        seekToMilliSecond(pos);
    }
    
    void play(const string &strUrl) override {
        PlayerImp<RtmpPlayer, RtmpDemuxer>::play(strUrl);
    }

private:
    //派生类回调函数
    bool onCheckMeta(const AMFValue &val) override {
        _rtmp_src = dynamic_pointer_cast<RtmpMediaSource>(_pMediaSrc);
        if (_rtmp_src) {
            _rtmp_src->setMetaData(val);
            _set_meta_data = true;
        }
        _delegate.reset(new RtmpDemuxer);
        _delegate->loadMetaData(val);
        return true;
    }

    void onMediaData(RtmpPacket::Ptr chunkData) override {
        if (!_delegate) {
            //这个流没有metadata
            _delegate.reset(new RtmpDemuxer());
        }
        _delegate->inputRtmp(chunkData);

        if (_rtmp_src) {
            if (!_set_meta_data && !chunkData->isCfgFrame()) {
                _set_meta_data = true;
                _rtmp_src->setMetaData(TitleMeta().getMetadata());
            }
            _rtmp_src->onWrite(std::move(chunkData));
        }
    }

private:
    RtmpMediaSource::Ptr _rtmp_src;
    bool _set_meta_data = false;
};


} /* namespace mediakit */

#endif /* SRC_RTMP_RTMPPLAYERIMP_H_ */
