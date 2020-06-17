/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Frame.h"

using namespace std;
using namespace toolkit;

namespace mediakit{

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
            _buffer = std::make_shared<BufferRaw>();
            _buffer->assign(frame->data(),frame->size());
            _ptr = _buffer->data();
        }
        _size = frame->size();
        _dts = frame->dts();
        _pts = frame->pts();
        _prefix_size = frame->prefixSize();
        _codecid = frame->getCodecId();
        _key = frame->keyFrame();
        _config = frame->configFrame();
    }

    virtual ~FrameCacheAble() = default;

    /**
     * 可以被缓存
     */
    bool cacheAble() const override {
        return true;
    }

    CodecId getCodecId() const override{
        return _codecid;
    }

    bool keyFrame() const override{
        return _key;
    }

    bool configFrame() const override{
        return _config;
    }
private:
    Frame::Ptr _frame;
    BufferRaw::Ptr _buffer;
    CodecId _codecid;
    bool _key;
    bool _config;
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
        case CodecOpus: return TrackAudio;
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
