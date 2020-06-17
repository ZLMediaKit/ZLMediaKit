/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifdef ENABLE_MP4
#include "MP4Demuxer.h"
#include "Util/logger.h"
#include "Extension/H265.h"
#include "Extension/H264.h"
#include "Extension/AAC.h"
#include "Extension/G711.h"
using namespace toolkit;
namespace mediakit {

MP4Demuxer::MP4Demuxer(const char *file) {
    openFile(file,"rb+");
    _mov_reader = createReader();
    getAllTracks();
    _duration_ms = mov_reader_getduration(_mov_reader.get());
}

MP4Demuxer::~MP4Demuxer() {
    _mov_reader = nullptr;
    closeFile();
}

int MP4Demuxer::getAllTracks() {
    static mov_reader_trackinfo_t s_on_track = {
            [](void *param, uint32_t track, uint8_t object, int width, int height, const void *extra, size_t bytes) {
                //onvideo
                MP4Demuxer *thiz = (MP4Demuxer *)param;
                thiz->onVideoTrack(track,object,width,height,extra,bytes);
            },
            [](void *param, uint32_t track, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void *extra, size_t bytes) {
                //onaudio
                MP4Demuxer *thiz = (MP4Demuxer *)param;
                thiz->onAudioTrack(track,object,channel_count,bit_per_sample,sample_rate,extra,bytes);
            },
            [](void *param, uint32_t track, uint8_t object, const void *extra, size_t bytes) {
                //onsubtitle, do nothing
            }
    };
    return mov_reader_getinfo(_mov_reader.get(),&s_on_track,this);
}

#define SWITCH_CASE(obj_id) case obj_id : return #obj_id
static const char *getObjectName(int obj_id) {
    switch (obj_id) {
        SWITCH_CASE(MOV_OBJECT_TEXT);
        SWITCH_CASE(MOV_OBJECT_MP4V);
        SWITCH_CASE(MOV_OBJECT_H264);
        SWITCH_CASE(MOV_OBJECT_HEVC);
        SWITCH_CASE(MOV_OBJECT_AAC);
        SWITCH_CASE(MOV_OBJECT_MP2V);
        SWITCH_CASE(MOV_OBJECT_AAC_MAIN);
        SWITCH_CASE(MOV_OBJECT_AAC_LOW);
        SWITCH_CASE(MOV_OBJECT_AAC_SSR);
        SWITCH_CASE(MOV_OBJECT_MP3);
        SWITCH_CASE(MOV_OBJECT_MP1V);
        SWITCH_CASE(MOV_OBJECT_MP1A);
        SWITCH_CASE(MOV_OBJECT_JPEG);
        SWITCH_CASE(MOV_OBJECT_PNG);
        SWITCH_CASE(MOV_OBJECT_JPEG2000);
        SWITCH_CASE(MOV_OBJECT_G719);
        SWITCH_CASE(MOV_OBJECT_OPUS);
        SWITCH_CASE(MOV_OBJECT_G711a);
        SWITCH_CASE(MOV_OBJECT_G711u);
        SWITCH_CASE(MOV_OBJECT_AV1);
        default:
            return "unknown mp4 object";
    }
}


void MP4Demuxer::onVideoTrack(uint32_t track, uint8_t object, int width, int height, const void *extra, size_t bytes) {
    switch (object) {
        case MOV_OBJECT_H264: {
            auto video = std::make_shared<H264Track>();
            _track_to_codec.emplace(track,video);

            struct mpeg4_avc_t avc = {0};
            if (mpeg4_avc_decoder_configuration_record_load((uint8_t *) extra, bytes, &avc) > 0) {
                uint8_t config[1024] = {0};
                int size = mpeg4_avc_to_nalu(&avc, config, sizeof(config));
                if (size > 0) {
                    video->inputFrame(std::make_shared<H264FrameNoCacheAble>((char *)config, size, 0, 4));
                }
            }
        }
            break;
        case MOV_OBJECT_HEVC: {
            auto video = std::make_shared<H265Track>();
            _track_to_codec.emplace(track,video);

            struct mpeg4_hevc_t hevc = {0};
            if (mpeg4_hevc_decoder_configuration_record_load((uint8_t *) extra, bytes, &hevc) > 0) {
                uint8_t config[1024] = {0};
                int size = mpeg4_hevc_to_nalu(&hevc, config, sizeof(config));
                if (size > 0) {
                    video->inputFrame(std::make_shared<H265FrameNoCacheAble>((char *) config, size, 0, 4));
                }
            }
        }
            break;
        default:
            WarnL << "不支持该编码类型的MP4,已忽略:" << getObjectName(object);
            break;
    }
}

void MP4Demuxer::onAudioTrack(uint32_t track_id, uint8_t object, int channel_count, int bit_per_sample, int sample_rate, const void *extra, size_t bytes) {
    switch(object){
        case MOV_OBJECT_AAC:{
            auto audio = std::make_shared<AACTrack>(bytes > 0 ? string((char *)extra,bytes) : "");
            _track_to_codec.emplace(track_id, audio);
        }
            break;
        case MOV_OBJECT_G711a:
        case MOV_OBJECT_G711u:{
            auto audio = std::make_shared<G711Track>(object == MOV_OBJECT_G711a ? CodecG711A : CodecG711U, sample_rate, channel_count, bit_per_sample / channel_count );
            _track_to_codec.emplace(track_id, audio);
        }
            break;
        default:
            WarnL << "不支持该编码类型的MP4,已忽略:" << getObjectName(object);
            break;
    }
}

int64_t MP4Demuxer::seekTo(int64_t stamp_ms) {
    if(0 != mov_reader_seek(_mov_reader.get(),&stamp_ms)){
        return -1;
    }
    return stamp_ms;
}

struct Context{
    MP4Demuxer *thiz;
    int flags;
    int64_t pts;
    int64_t dts;
    uint32_t track_id;
    BufferRaw::Ptr buffer;
};

Frame::Ptr MP4Demuxer::readFrame(bool &keyFrame, bool &eof) {
    keyFrame = false;
    eof = false;
    static mov_reader_onread mov_reader_onread = [](void *param, uint32_t track_id, const void *buffer, size_t bytes, int64_t pts, int64_t dts, int flags) {
        Context *ctx = (Context *) param;
        ctx->pts = pts;
        ctx->dts = dts;
        ctx->flags = flags;
        ctx->track_id = track_id;
    };

    static mov_onalloc mov_onalloc = [](void *param, int bytes) -> void * {
        Context *ctx = (Context *) param;
        ctx->buffer = ctx->thiz->_buffer_pool.obtain();
        ctx->buffer->setCapacity(bytes + 1);
        ctx->buffer->setSize(bytes);
        return ctx->buffer->data();
    };

    Context ctx = {this, 0};
    auto ret = mov_reader_read2(_mov_reader.get(), mov_onalloc, mov_reader_onread, &ctx);
    switch (ret) {
        case 0 : {
            eof = true;
            return nullptr;
        }

        case 1 : {
            keyFrame = ctx.flags & MOV_AV_FLAG_KEYFREAME;
            return makeFrame(ctx.track_id, ctx.buffer, ctx.pts, ctx.dts);
        }

        default : {
            eof = true;
            WarnL << "读取mp4文件数据失败:" << ret;
            return nullptr;
        }
    }
}

template <typename Parent>
class FrameWrapper : public Parent{
public:
    ~FrameWrapper() = default;
    FrameWrapper(const Buffer::Ptr &buf, int64_t pts, int64_t dts, int prefix) : Parent(buf->data(), buf->size(), dts, pts, prefix){
        _buf = buf;
    }
    bool cacheAble() const override {
        return true;
    }
private:
    Buffer::Ptr _buf;
};

Frame::Ptr MP4Demuxer::makeFrame(uint32_t track_id, const Buffer::Ptr &buf, int64_t pts, int64_t dts) {
    auto it = _track_to_codec.find(track_id);
    if (it == _track_to_codec.end()) {
        return nullptr;
    }
    auto numBytes = buf->size();
    auto pBytes = buf->data();
    auto codec = it->second->getCodecId();
    switch (codec) {
        case CodecH264 :
        case CodecH265 : {
            uint32_t iOffset = 0;
            while (iOffset < numBytes) {
                uint32_t iFrameLen;
                memcpy(&iFrameLen, pBytes + iOffset, 4);
                iFrameLen = ntohl(iFrameLen);
                if (iFrameLen + iOffset + 4 > numBytes) {
                    return nullptr;
                }
                memcpy(pBytes + iOffset, "\x0\x0\x0\x1", 4);
                iOffset += (iFrameLen + 4);
            }
            if (codec == CodecH264) {
                return std::make_shared<FrameWrapper<H264FrameNoCacheAble> >(buf, pts, dts, 4);
            }
            return std::make_shared<FrameWrapper<H265FrameNoCacheAble> >(buf, pts, dts, 4);
        }

        case CodecAAC :
            return std::make_shared<FrameWrapper<AACFrameNoCacheAble> >(buf, pts, dts, 0);

        case CodecG711A:
        case CodecG711U: {
            auto frame = std::make_shared<FrameWrapper<G711FrameNoCacheAble> >(buf, pts, dts, 0);
            frame->setCodec(codec);
            return frame;
        }
        default:
            return nullptr;
    }
}

vector<Track::Ptr> MP4Demuxer::getTracks(bool trackReady) const {
    vector<Track::Ptr> ret;
    for (auto &pr : _track_to_codec) {
        if(trackReady && !pr.second->ready()){
            continue;
        }
        ret.push_back(pr.second);
    }
    return std::move(ret);
}

uint64_t MP4Demuxer::getDurationMS() const {
    return _duration_ms;
}

}//namespace mediakit
#endif// ENABLE_MP4