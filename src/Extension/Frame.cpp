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

TrackType getTrackType(CodecId codecId) {
    switch (codecId) {
#define XX(name, type, value, str) case name : return type;
        CODEC_MAP(XX)
#undef XX
        default : return TrackInvalid;
    }
}

const char *getCodecName(CodecId codec) {
    switch (codec) {
#define XX(name, type, value, str) case name : return str;
        CODEC_MAP(XX)
#undef XX
        default : return "invalid";
    }
}

#define XX(name, type, value, str) {str, name},
static map<string, CodecId, StrCaseCompare> codec_map = {CODEC_MAP(XX)};
#undef XX

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
            //遇到新帧、或时间戳变化或缓存太多，防止内存溢出，则flush输出
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
                    auto type = H264_TYPE(frame->data()[frame->prefixSize()]);
                    // sei aud pps sps 不判断；264 新一帧的开始，刷新输出
                    return (frame->data()[frame->prefixSize() + 1] & 0x80) != 0 && type >= H264Frame::NAL_B_P &&
                           type <= H264Frame::NAL_IDR;
                }
                case CodecH265 : {
                    auto type = H265_TYPE(frame->data()[frame->prefixSize()]);
                    //first_slice_segment_in_pic_flag is frame start
                    return (type >= H265Frame::NAL_TRAIL_R && type <= H265Frame::NAL_RSV_IRAP_VCL23) &&
                           ((frame->data()[frame->prefixSize() + 2] >> 7 & 0x01) != 0);
                }
                default :
                    //缓存太多，防止内存溢出
                    return _frameCached.size() > kMaxFrameCacheSize;
            }
        }
        default: /*不可达*/ assert(0); return true;
    }
}

void FrameMerger::doMerge(BufferLikeString &merged, const Frame::Ptr &frame) const{
    switch (_type) {
        case none : {
            //此处是合并ps解析输出的流，解析出的流可能是半帧或多帧，不能简单的根据nal type过滤
            //此流程只用于合并ps解析输出为H264/H265，后面流程有split和忽略无效帧操作
            merged.append(frame->data(), frame->size());
            break;
        }
        case h264_prefix: {
            if (shouldDrop(frame)) {
                //h264头模式过滤无效的帧
                break;
            }
            if (frame->prefixSize()) {
                merged.append(frame->data(), frame->size());
            } else {
                merged.append("\x00\x00\x00\x01", 4);
                merged.append(frame->data(), frame->size());
            }
            break;
        }
        case mp4_nal_size: {
            if (shouldDrop(frame)) {
                //MP4头模式过滤无效的帧
                break;
            }
            uint32_t nalu_size = (uint32_t) (frame->size() - frame->prefixSize());
            nalu_size = htonl(nalu_size);
            merged.append((char *) &nalu_size, 4);
            merged.append(frame->data() + frame->prefixSize(), frame->size() - frame->prefixSize());
            break;
        }
        default: /*不可达*/ assert(0); break;
    }
}

bool FrameMerger::shouldDrop(const Frame::Ptr &frame) const{
    switch (frame->getCodecId()) {
        case CodecH264: {
            switch (H264_TYPE(frame->data()[frame->prefixSize()])) {
                // 防止把AUD或者SEI当成一帧
                case H264Frame::NAL_SEI:
                case H264Frame::NAL_AUD: return true;
                default: return false;
            }
        }

        case CodecH265: {
            switch (H265_TYPE(frame->data()[frame->prefixSize()])) {
                // 防止把AUD或者SEI当成一帧
                case H265Frame::NAL_AUD:
                case H265Frame::NAL_SEI_SUFFIX:
                case H265Frame::NAL_SEI_PREFIX: return true;
                default: return false;
            }
        }

        default: return false;
    }
}

bool FrameMerger::frameCacheHasVCL(List<Frame::Ptr> &frameCached) const {
    bool hasVCL = false;
    bool isH264OrH265 = false;
    frameCached.for_each([&hasVCL, &isH264OrH265](const Frame::Ptr &frame) {
        switch (frame->getCodecId()) {
            case CodecH264: {
                auto type = H264_TYPE(frame->data()[frame->prefixSize()]);
                if (type >= H264Frame::NAL_B_P && type <= H264Frame::NAL_IDR) {
                    //有编码数据
                    hasVCL = true;
                }
                isH264OrH265 = true;
                break;
            }
            case CodecH265: {
                //如果是新的一帧，前面的缓存需要输出
                auto type = H265_TYPE(frame->data()[frame->prefixSize()]);
                if (type >= H265Frame::NAL_TRAIL_R && type <= H265Frame::NAL_RSV_IRAP_VCL23) {
                    //有编码数据
                    hasVCL = true;
                }
                isH264OrH265 = true;
                break;
            }
            default:
                break;
        }
    });
    if (isH264OrH265) {
        return hasVCL;
    }
    return true;
}

void FrameMerger::inputFrame(const Frame::Ptr &frame, const onOutput &cb) {
    if (willFlush(frame) && frameCacheHasVCL(_frameCached)) {
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
            if (merged.empty()) {
                _frameCached.clear();
                return;
            }
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
