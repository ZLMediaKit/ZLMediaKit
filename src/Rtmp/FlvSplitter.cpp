/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */
#include "FlvSplitter.h"
#include "utils.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

const char *FlvSplitter::onSearchPacketTail(const char *data, size_t len) {
    if (!_flv_started) {
        //还没获取到flv头
        if (len < sizeof(FLVHeader)) {
            //数据不够
            return nullptr;
        }
        return data + sizeof(FLVHeader);
    }

    //获取到flv头，处理tag数据
    if (len < sizeof(RtmpTagHeader)) {
        //数据不够
        return nullptr;
    }
    return data + sizeof(RtmpTagHeader);
}

ssize_t FlvSplitter::onRecvHeader(const char *data, size_t len) {
    if (!_flv_started) {
        //获取到flv头了
        auto header = reinterpret_cast<const FLVHeader *>(data);
        if (memcmp(header->flv, "FLV", 3)) {
            throw std::invalid_argument("不是flv容器格式！");
        }
        if (header->version != FLVHeader::kFlvVersion) {
            throw std::invalid_argument("flv头中version字段不正确");
        }
        if (!header->have_video && !header->have_audio) {
            throw std::invalid_argument("flv头中声明音频和视频都不存在");
        }
        if (FLVHeader::kFlvHeaderLength != ntohl(header->length)) {
            throw std::invalid_argument("flv头中length字段非法");
        }
        if (0 != ntohl(header->previous_tag_size0)) {
            throw std::invalid_argument("flv头中previous tag size字段非法");
        }
        onRecvFlvHeader(*header);
        _flv_started = true;
        return 0;
    }

    //获取到flv头，处理tag数据
    auto tag = reinterpret_cast<const RtmpTagHeader *>(data);
    auto data_size = load_be24(tag->data_size);
    _type = tag->type;
    _time_stamp = load_be24(tag->timestamp);
    _time_stamp |= (tag->timestamp_ex << 24);
    return data_size + 4/*PreviousTagSize*/;
}

void FlvSplitter::onRecvContent(const char *data, size_t len) {
    len -= 4;
    auto previous_tag_size = load_be32(data + len);
    if (len != previous_tag_size - sizeof(RtmpTagHeader)) {
        WarnL << "flv previous tag size 字段非法:" << len << " != " << previous_tag_size - sizeof(RtmpTagHeader);
    }
    RtmpPacket::Ptr packet;
    switch (_type) {
        case MSG_AUDIO : {
            packet = RtmpPacket::create();
            packet->chunk_id = CHUNK_AUDIO;
            packet->stream_index = STREAM_MEDIA;
            break;
        }
        case MSG_VIDEO: {
            packet = RtmpPacket::create();
            packet->chunk_id = CHUNK_VIDEO;
            packet->stream_index = STREAM_MEDIA;
            break;
        }

        case MSG_DATA:
        case MSG_DATA3: {
            BufferLikeString buffer(string(data, len));
            AMFDecoder dec(buffer, _type == MSG_DATA3 ? 3 : 0);
            auto first = dec.load<AMFValue>();
            bool flag = true;
            if (first.type() == AMFType::AMF_STRING) {
                auto type = first.as_string();
                if (type == "@setDataFrame") {
                    type = dec.load<std::string>();
                    if (type == "onMetaData") {
                        flag = onRecvMetadata(dec.load<AMFValue>());
                    } else {
                        WarnL << "unknown type:" << type;
                    }
                } else if (type == "onMetaData") {
                    flag = onRecvMetadata(dec.load<AMFValue>());
                } else {
                    WarnL << "unknown notify:" << type;
                }
            } else {
                WarnL << "Parse flv script data failed, invalid amf value: " << first.to_string();
            }
            if (!flag) {
                throw std::invalid_argument("check rtmp metadata failed");
            }
            return;
        }

        default: WarnL << "不识别的flv msg type:" << (int) _type; return;
    }

    packet->time_stamp = _time_stamp;
    packet->type_id = _type;
    packet->body_size = len;
    packet->buffer.assign(data, len);
    onRecvRtmpPacket(std::move(packet));
}


}//namespace mediakit