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

static size_t constexpr kMaxFrameCacheSize = 100;

bool FrameMerger::willFlush(const Frame::Ptr &frame) const{
    if (_frameCached.empty()) {
        return false;
    }
    switch (_type) {
        case none : {
            //frame不是完整的帧，我们合并为一帧
            bool new_frame = false;
            switch (frame->getCodecId()) {
                case CodecH264:
                case CodecH265: {
                    //如果是新的一帧，前面的缓存需要输出
                    new_frame = frame->prefixSize();
                    break;
                }
                default: break;
            }
            return new_frame || _frameCached.back()->dts() != frame->dts() || _frameCached.size() > kMaxFrameCacheSize;
        }

        case mp4_nal_size:
        case h264_prefix: {
            if (_frameCached.back()->dts() != frame->dts()) {
                //时间戳变化了
                return true;
            }
            switch (frame->getCodecId()) {
                case CodecH264 : {
                    if (H264_TYPE(frame->data()[frame->prefixSize()]) == H264Frame::NAL_B_P) {
                        //如果是264的b/p帧，那么也刷新输出
                        return true;
                    }
                    break;
                }
                case CodecH265 : {
                    if (H265_TYPE(frame->data()[frame->prefixSize()]) == H265Frame::NAL_TRAIL_R) {
                        //如果是265的TRAIL_R帧，那么也刷新输出
                        return true;
                    }
                    break;
                }
                default : break;
            }
            return _frameCached.size() > kMaxFrameCacheSize;
        }
        default: /*不可达*/ assert(0); return true;
    }
}

void FrameMerger::doMerge(BufferLikeString &merged, const Frame::Ptr &frame) const{
    switch (_type) {
        case none : {
            merged.append(frame->data(), frame->size());
            break;
        }
        case h264_prefix: {
            if (frame->prefixSize()) {
                merged.append(frame->data(), frame->size());
            } else {
                merged.append("\x00\x00\x00\x01", 4);
                merged.append(frame->data(), frame->size());
            }
            break;
        }
        case mp4_nal_size: {
            uint32_t nalu_size = (uint32_t) (frame->size() - frame->prefixSize());
            nalu_size = htonl(nalu_size);
            merged.append((char *) &nalu_size, 4);
            merged.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            break;
        }
        default: /*不可达*/ assert(0); break;
    }
}

void FrameMerger::inputFrame(const Frame::Ptr &frame, const onOutput &cb) {
    if (willFlush(frame)) {
        Frame::Ptr back = _frameCached.back();
        Buffer::Ptr merged_frame = back;
        bool have_idr = back->keyFrame();

        if (_frameCached.size() != 1 || _type == mp4_nal_size) {
            //在MP4模式下，一帧数据也需要在前添加nalu_size
            BufferLikeString merged;
            merged.reserve(back->size() + 1024);
            _frameCached.for_each([&](const Frame::Ptr &frame) {
                doMerge(merged, frame);
                if (frame->keyFrame()) {
                    have_idr = true;
                }
            });
            merged_frame = std::make_shared<BufferOffset<BufferLikeString> >(std::move(merged));
        }
        cb(back->dts(), back->pts(), merged_frame, have_idr);
        _frameCached.clear();
    }
    _frameCached.emplace_back(Frame::getCacheAbleFrame(frame));
}

FrameMerger::FrameMerger(int type) {
    _type = type;
}

void FrameMerger::clear() {
    _frameCached.clear();
}

}//namespace mediakit
