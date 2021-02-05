/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Frame.h"
#include "H264.h"
#include "H265.h"

using namespace std;
using namespace toolkit;

namespace toolkit {
    StatisticImp(mediakit::Frame);
    StatisticImp(mediakit::FrameImp);
}

namespace mediakit{

template<typename C>
std::shared_ptr<C> FrameImp::create_l() {
#if 0
    static ResourcePool<C> packet_pool;
    static onceToken token([]() {
        packet_pool.setSize(1024);
    });
    auto ret = packet_pool.obtain();
    ret->_buffer.clear();
    ret->_prefix_size = 0;
    ret->_dts = 0;
    ret->_pts = 0;
    return ret;
#else
    return std::shared_ptr<C>(new C());
#endif
}

#define CREATE_FRAME_IMP(C) \
template<> \
std::shared_ptr<C> FrameImp::create<C>() { \
    return create_l<C>(); \
}

CREATE_FRAME_IMP(FrameImp);
CREATE_FRAME_IMP(H264Frame);
CREATE_FRAME_IMP(H265Frame);

/**
 * 该对象的功能是把一个不可缓存的帧转换成可缓存的帧
 */
class FrameCacheAble : public FrameFromPtr {
public:
    typedef std::shared_ptr<FrameCacheAble> Ptr;

    FrameCacheAble(const Frame::Ptr &frame){
        if(frame->cacheAble()){
            _frame = frame;
            _ptr = frame->data();
        }else{
            _buffer = FrameImp::create();
            _buffer->_buffer.assign(frame->data(),frame->size());
            _ptr = _buffer->data();
        }
        _size = frame->size();
        _dts = frame->dts();
        _pts = frame->pts();
        _prefix_size = frame->prefixSize();
        _codec_id = frame->getCodecId();
        _key = frame->keyFrame();
        _config = frame->configFrame();
    }

    ~FrameCacheAble() override = default;

    /**
     * 可以被缓存
     */
    bool cacheAble() const override {
        return true;
    }

    bool keyFrame() const override{
        return _key;
    }

    bool configFrame() const override{
        return _config;
    }

private:
    bool _key;
    bool _config;
    Frame::Ptr _frame;
    FrameImp::Ptr _buffer;
};

Frame::Ptr Frame::getCacheAbleFrame(const Frame::Ptr &frame){
    if(frame->cacheAble()){
        return frame;
    }
    return std::make_shared<FrameCacheAble>(frame);
}

#define SWITCH_CASE(codec_id) case codec_id : return #codec_id
const char *getCodecName(CodecId codecId) {
    switch (codecId) {
        SWITCH_CASE(CodecH264);
        SWITCH_CASE(CodecH265);
        SWITCH_CASE(CodecAAC);
        SWITCH_CASE(CodecG711A);
        SWITCH_CASE(CodecG711U);
        SWITCH_CASE(CodecOpus);
        SWITCH_CASE(CodecL16);
        default : return "unknown codec";
    }
}

TrackType getTrackType(CodecId codecId){
    switch (codecId){
        case CodecH264:
        case CodecH265: return TrackVideo;
        case CodecAAC:
        case CodecG711A:
        case CodecG711U:
        case CodecOpus: 
        case CodecL16: return TrackAudio;
        default: return TrackInvalid;
    }
}

const char *CodecInfo::getCodecName() {
    return mediakit::getCodecName(getCodecId());
}

TrackType CodecInfo::getTrackType() {
    return mediakit::getTrackType(getCodecId());
}
}//namespace mediakit
