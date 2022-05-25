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

Frame::Ptr Frame::getCacheAbleFrame(const Frame::Ptr &frame){
    if(frame->cacheAble()){
        return frame;
    }
    return std::make_shared<FrameCacheAble>(frame);
}

TrackType getTrackType(CodecId codecId) {
    switch (codecId) {
#define XX(name, type, value, str, mpeg_id) case name : return type;
        CODEC_MAP(XX)
#undef XX
        default : return TrackInvalid;
    }
}

const char *getCodecName(CodecId codec) {
    switch (codec) {
#define XX(name, type, value, str, mpeg_id) case name : return str;
        CODEC_MAP(XX)
#undef XX
        default : return "invalid";
    }
}

#define XX(name, type, value, str, mpeg_id) {str, name},
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

const char *CodecInfo::getCodecName() const {
    return mediakit::getCodecName(getCodecId());
}

TrackType CodecInfo::getTrackType() const {
    return mediakit::getTrackType(getCodecId());
}

static size_t constexpr kMaxFrameCacheSize = 100;

bool FrameMerger::willFlush(const Frame::Ptr &frame) const{
    if (_frame_cache.empty()) {
        //缓存为空
        return false;
    }
    if (!frame) {
        return true;
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
            return new_frame || _frame_cache.back()->dts() != frame->dts() || _frame_cache.size() > kMaxFrameCacheSize;
        }

        case mp4_nal_size:
        case h264_prefix: {
            if (!_have_decode_able_frame) {
                //缓存中没有有效的能解码的帧，所以这次不flush
                return _frame_cache.size() > kMaxFrameCacheSize;
            }
            if (_frame_cache.back()->dts() != frame->dts() || frame->decodeAble() || frame->configFrame()) {
                //时间戳变化了,或新的一帧，或遇到config帧，立即flush
                return true;
            }
            return _frame_cache.size() > kMaxFrameCacheSize;
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

bool FrameMerger::inputFrame(const Frame::Ptr &frame, const onOutput &cb, BufferLikeString *buffer) {
    if (willFlush(frame)) {
        Frame::Ptr back = _frame_cache.back();
        Buffer::Ptr merged_frame = back;
        bool have_key_frame = back->keyFrame();

        if (_frame_cache.size() != 1 || _type == mp4_nal_size || buffer) {
            //在MP4模式下，一帧数据也需要在前添加nalu_size
            BufferLikeString tmp;
            BufferLikeString &merged = buffer ? *buffer : tmp;

            if (!buffer) {
                tmp.reserve(back->size() + 1024);
            }

            _frame_cache.for_each([&](const Frame::Ptr &frame) {
                doMerge(merged, frame);
                if (frame->keyFrame()) {
                    have_key_frame = true;
                }
            });
            merged_frame = std::make_shared<BufferOffset<BufferLikeString> >(buffer ? merged : std::move(merged));
        }
        cb(back->dts(), back->pts(), merged_frame, have_key_frame);
        _frame_cache.clear();
        _have_decode_able_frame = false;
    }

    if (!frame) {
        return false;
    }

    if (frame->decodeAble()) {
        _have_decode_able_frame = true;
    }
    _frame_cache.emplace_back(Frame::getCacheAbleFrame(frame));
    return true;
}

FrameMerger::FrameMerger(int type) {
    _type = type;
}

void FrameMerger::clear() {
    _frame_cache.clear();
    _have_decode_able_frame = false;
}

}//namespace mediakit
