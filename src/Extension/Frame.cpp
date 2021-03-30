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
#include "Common/Parser.h"
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

TrackType getTrackType(CodecId codecId){
    switch (codecId){
        case CodecVP8:
        case CodecVP9:
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

const char* getCodecName(CodecId codec){
    switch (codec) {
        case CodecH264 : return "H264";
        case CodecH265 : return "H265";
        case CodecAAC : return "mpeg4-generic";
        case CodecG711A : return "PCMA";
        case CodecG711U : return "PCMU";
        case CodecOpus : return "opus";
        case CodecVP8 : return "VP8";
        case CodecVP9 : return "VP9";
        case CodecL16 : return "L16";
        default: return "invalid";
    }
}

static map<string, CodecId, StrCaseCompare> codec_map = {
        {"H264",          CodecH264},
        {"H265",          CodecH265},
        {"mpeg4-generic", CodecAAC},
        {"PCMA",          CodecG711A},
        {"PCMU",          CodecG711U},
        {"opus",          CodecOpus},
        {"VP8",           CodecVP8},
        {"VP9",           CodecVP9},
        {"L16",           CodecL16}
};

CodecId getCodecId(const string &str){
    auto it = codec_map.find(str);
    return it == codec_map.end() ? CodecInvalid : it->second;
}

static map<string, TrackType, StrCaseCompare> track_str_map = {
        {"video",       TrackVideo},
        {"audio",       TrackAudio},
        {"application", TrackApplication}
};

TrackType getTrackType(const string &str) {
    auto it = track_str_map.find(str);
    return it == track_str_map.end() ? TrackInvalid : it->second;
}

const char* getTrackString(TrackType type){
    switch (type) {
        case TrackVideo : return "video";
        case TrackAudio : return "audio";
        case TrackApplication : return "application";
        default: return "invalid";
    }
}

const char *CodecInfo::getCodecName() {
    return mediakit::getCodecName(getCodecId());
}

TrackType CodecInfo::getTrackType() {
    return mediakit::getTrackType(getCodecId());
}
}//namespace mediakit
