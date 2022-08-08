/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
#include "Extension/Opus.h"
using namespace toolkit;
using namespace std;

namespace mediakit {

MP4Demuxer::MP4Demuxer() {}

MP4Demuxer::~MP4Demuxer() {
    closeMP4();
}

void MP4Demuxer::openMP4(const string &file) {
    closeMP4();

    _mp4_file = std::make_shared<MP4FileDisk>();
    _mp4_file->openFile(file.data(), "rb+");
    _mov_reader = _mp4_file->createReader();
    getAllTracks();
    _duration_ms = mov_reader_getduration(_mov_reader.get());
}

void MP4Demuxer::closeMP4() {
    _mov_reader.reset();
    _mp4_file.reset();
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

            struct mpeg4_avc_t avc;
            memset(&avc, 0, sizeof(avc));
            if (mpeg4_avc_decoder_configuration_record_load((uint8_t *) extra, bytes, &avc) > 0) {
                uint8_t config[1024 * 10] = {0};
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

            struct mpeg4_hevc_t hevc;
            memset(&hevc, 0, sizeof(hevc));
            if (mpeg4_hevc_decoder_configuration_record_load((uint8_t *) extra, bytes, &hevc) > 0) {
                uint8_t config[1024 * 10] = {0};
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
            break;
        }

        case MOV_OBJECT_G711a:
        case MOV_OBJECT_G711u:{
            auto audio = std::make_shared<G711Track>(object == MOV_OBJECT_G711a ? CodecG711A : CodecG711U, sample_rate, channel_count, bit_per_sample / channel_count );
            _track_to_codec.emplace(track_id, audio);
            break;
        }

        case MOV_OBJECT_OPUS: {
            auto audio = std::make_shared<OpusTrack>();
            _track_to_codec.emplace(track_id, audio);
            break;
        }

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

struct Context {
    Context(MP4Demuxer *ptr) : thiz(ptr) {}
    MP4Demuxer *thiz;
    int flags = 0;
    int64_t pts = 0;
    int64_t dts = 0;
    uint32_t track_id = 0;
    BufferRaw::Ptr buffer;
};

#define DATA_OFFSET ADTS_HEADER_LEN

Frame::Ptr MP4Demuxer::readFrame(bool &keyFrame, bool &eof) {
    keyFrame = false;
    eof = false;

    static mov_reader_onread2 mov_onalloc = [](void *param, uint32_t track_id, size_t bytes, int64_t pts, int64_t dts, int flags) -> void * {
        Context *ctx = (Context *) param;
        ctx->pts = pts;
        ctx->dts = dts;
        ctx->flags = flags;
        ctx->track_id = track_id;

        ctx->buffer = ctx->thiz->_buffer_pool.obtain2();
        ctx->buffer->setCapacity(bytes + DATA_OFFSET + 1);
        ctx->buffer->setSize(bytes + DATA_OFFSET);
        return ctx->buffer->data() + DATA_OFFSET;
    };

    Context ctx(this);
    auto ret = mov_reader_read2(_mov_reader.get(), mov_onalloc, &ctx);
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

Frame::Ptr MP4Demuxer::makeFrame(uint32_t track_id, const Buffer::Ptr &buf, int64_t pts, int64_t dts) {
    auto it = _track_to_codec.find(track_id);
    if (it == _track_to_codec.end()) {
        return nullptr;
    }
    auto bytes = buf->size() - DATA_OFFSET;
    auto data = buf->data() + DATA_OFFSET;
    auto codec = it->second->getCodecId();
    Frame::Ptr ret;
    switch (codec) {
        case CodecH264 :
        case CodecH265 : {
            uint32_t offset = 0;
            while (offset < bytes) {
                uint32_t frame_len;
                memcpy(&frame_len, data + offset, 4);
                frame_len = ntohl(frame_len);
                if (frame_len + offset + 4 > bytes) {
                    return nullptr;
                }
                memcpy(data + offset, "\x00\x00\x00\x01", 4);
                offset += (frame_len + 4);
            }
            if (codec == CodecH264) {
                ret = std::make_shared<FrameWrapper<H264FrameNoCacheAble> >(buf, (uint64_t)dts, (uint64_t)pts, 4, DATA_OFFSET);
                break;
            }
            ret = std::make_shared<FrameWrapper<H265FrameNoCacheAble> >(buf, (uint64_t)dts, (uint64_t)pts, 4, DATA_OFFSET);
            break;
        }

        case CodecAAC: {
            AACTrack::Ptr track = dynamic_pointer_cast<AACTrack>(it->second);
            assert(track);
            //加上adts头
            dumpAacConfig(track->getAacCfg(), buf->size() - DATA_OFFSET, (uint8_t *) buf->data() + (DATA_OFFSET - ADTS_HEADER_LEN), ADTS_HEADER_LEN);
            ret = std::make_shared<FrameWrapper<FrameFromPtr> >(buf, (uint64_t)dts, (uint64_t)pts, ADTS_HEADER_LEN, DATA_OFFSET - ADTS_HEADER_LEN, codec);
            break;
        }

        case CodecOpus:
        case CodecG711A:
        case CodecG711U: {
            ret = std::make_shared<FrameWrapper<FrameFromPtr> >(buf, (uint64_t)dts, (uint64_t)pts, 0, DATA_OFFSET, codec);
            break;
        }

        default: return nullptr;
    }
    if (ret) {
        it->second->inputFrame(ret);
    }
    return ret;
}

vector<Track::Ptr> MP4Demuxer::getTracks(bool trackReady) const {
    vector<Track::Ptr> ret;
    for (auto &pr : _track_to_codec) {
        if(trackReady && !pr.second->ready()){
            continue;
        }
        ret.push_back(pr.second);
    }
    return ret;
}

uint64_t MP4Demuxer::getDurationMS() const {
    return _duration_ms;
}

}//namespace mediakit
#endif// ENABLE_MP4