/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef __rtmp_h
#define __rtmp_h

#include <memory>
#include <string>
#include <cstdlib>
#include "amf.h"
#include "Network/Buffer.h"
#include "Extension/Track.h"

#if !defined(_WIN32)
#define PACKED	__attribute__((packed))
#else
#define PACKED
#endif //!defined(_WIN32)


#define DEFAULT_CHUNK_LEN	128
#define HANDSHAKE_PLAINTEXT	0x03
#define RANDOM_LEN		(1536 - 8)

#define MSG_SET_CHUNK		1 	/*Set Chunk Size (1)*/
#define MSG_ABORT			2	/*Abort Message (2)*/
#define MSG_ACK				3 	/*Acknowledgement (3)*/
#define MSG_USER_CONTROL	4	/*User Control Messages (4)*/
#define MSG_WIN_SIZE		5	/*Window Acknowledgement Size (5)*/
#define MSG_SET_PEER_BW		6	/*Set Peer Bandwidth (6)*/
#define MSG_AUDIO			8	/*Audio Message (8)*/
#define MSG_VIDEO			9	/*Video Message (9)*/
#define MSG_DATA			18	/*Data Message (18, 15) AMF0*/
#define MSG_DATA3			15	/*Data Message (18, 15) AMF3*/
#define MSG_CMD				20	/*Command Message AMF0 */
#define MSG_CMD3			17	/*Command Message AMF3 */
#define MSG_OBJECT3			16	/*Shared Object Message (19, 16) AMF3*/
#define MSG_OBJECT			19	/*Shared Object Message (19, 16) AMF0*/
#define MSG_AGGREGATE		22	/*Aggregate Message (22)*/

#define CONTROL_STREAM_BEGIN		0
#define CONTROL_STREAM_EOF			1
#define CONTROL_STREAM_DRY			2
#define CONTROL_SETBUFFER			3
#define CONTROL_STREAM_ISRECORDED	4
#define CONTROL_PING_REQUEST		6
#define CONTROL_PING_RESPONSE		7

#define STREAM_CONTROL				0
#define STREAM_MEDIA				1

#define CHUNK_NETWORK                   2 /*网络相关的消息(参见 Protocol Control Messages)*/
#define CHUNK_SYSTEM                    3 /*向服务器发送控制消息(反之亦可)*/
#define CHUNK_CLIENT_REQUEST_BEFORE		3 /*客户端在createStream前,向服务器发出请求的chunkID*/
#define CHUNK_CLIENT_REQUEST_AFTER		4 /*客户端在createStream后,向服务器发出请求的chunkID*/
#define CHUNK_AUDIO						6 /*音频chunkID*/
#define CHUNK_VIDEO						7 /*视频chunkID*/

#define FLV_KEY_FRAME				1
#define FLV_INTER_FRAME				2

#define FLV_CODEC_AAC 10
#define FLV_CODEC_H264 7
//金山扩展: https://github.com/ksvc/FFmpeg/wiki
#define FLV_CODEC_H265 12
#define FLV_CODEC_G711A 7
#define FLV_CODEC_G711U 8
//参考学而思网校: https://github.com/notedit/rtmp/commit/6e314ac5b29611431f8fb5468596b05815743c10
#define FLV_CODEC_OPUS 13

namespace mediakit {

#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

class RtmpHandshake {
public:
    RtmpHandshake(uint32_t _time, uint8_t *_random = nullptr);

    uint8_t time_stamp[4];
    uint8_t zero[4] = {0};
    uint8_t random[RANDOM_LEN];

    void random_generate(char *bytes, int size);

    void create_complex_c0c1();

}PACKED;

class RtmpHeader {
public:
#if __BYTE_ORDER == __BIG_ENDIAN
    uint8_t fmt : 2;
    uint8_t chunk_id : 6;
#else
    uint8_t chunk_id : 6;
    //0、1、2、3分别对应 12、8、4、1长度
    uint8_t fmt : 2;
#endif
    uint8_t time_stamp[3];
    uint8_t body_size[3];
    uint8_t type_id;
    uint8_t stream_index[4]; /* Note, this is little-endian while others are BE */
}PACKED;

class FLVHeader {
public:
    //FLV
    char flv[3];
    //File version (for example, 0x01 for FLV version 1)
    uint8_t version;
#if __BYTE_ORDER == __BIG_ENDIAN
    //保留,置0
    uint8_t : 5;
    //是否有音频
    uint8_t have_audio: 1;
    //保留,置0
    uint8_t : 1;
    //是否有视频
    uint8_t have_video: 1;
#else
    //是否有视频
    uint8_t have_video: 1;
    //保留,置0
    uint8_t : 1;
    //是否有音频
    uint8_t have_audio: 1;
    //保留,置0
    uint8_t : 5;
#endif
    //The length of this header in bytes,固定为9
    uint32_t length;
} PACKED;

class RtmpTagHeader {
public:
    uint8_t type = 0;
    uint8_t data_size[3] = {0};
    uint8_t timestamp[3] = {0};
    uint8_t timestamp_ex = 0;
    uint8_t streamid[3] = {0}; /* Always 0. */
} PACKED;

#if defined(_WIN32)
#pragma pack(pop)
#endif // defined(_WIN32)

class RtmpPacket : public toolkit::Buffer{
public:
    friend class RtmpProtocol;
    using Ptr = std::shared_ptr<RtmpPacket>;
    bool is_abs_stamp;
    uint8_t type_id;
    uint32_t time_stamp;
    uint32_t ts_field;
    uint32_t stream_index;
    uint32_t chunk_id;
    size_t body_size;
    toolkit::BufferLikeString buffer;

public:
    static Ptr create();

    char *data() const override{
        return (char*)buffer.data();
    }
    size_t size() const override {
        return buffer.size();
    }

    void clear();

    bool isVideoKeyFrame() const;
    bool isCfgFrame() const;

    int getMediaType() const;

    int getAudioSampleRate() const;
    int getAudioSampleBit() const;
    int getAudioChannel() const;

private:
    friend class toolkit::ResourcePool_l<RtmpPacket>;
    RtmpPacket(){
        clear();
    }

    RtmpPacket &operator=(const RtmpPacket &that);

private:
    //对象个数统计
    toolkit::ObjectStatistic<RtmpPacket> _statistic;
};

/**
 * rtmp metadata基类，用于描述rtmp格式信息
 */
class Metadata : public CodecInfo{
public:
    using Ptr = std::shared_ptr<Metadata>;

    Metadata(): _metadata(AMF_OBJECT) {}
    virtual ~Metadata() = default;
    const AMFValue &getMetadata() const{
        return _metadata;
    }

    static void addTrack(AMFValue &metadata, const Track::Ptr &track);
protected:
    AMFValue _metadata;
};

/**
* metadata中除音视频外的其他描述部分
*/
class TitleMeta : public Metadata{
public:
    using Ptr = std::shared_ptr<TitleMeta>;

    TitleMeta(float dur_sec = 0,
              size_t fileSize = 0,
              const std::map<std::string, std::string> &header = std::map<std::string, std::string>());

    CodecId getCodecId() const override{
        return CodecInvalid;
    }
};

class VideoMeta : public Metadata{
public:
    using Ptr = std::shared_ptr<VideoMeta>;

    VideoMeta(const VideoTrack::Ptr &video);
    virtual ~VideoMeta() = default;

    CodecId getCodecId() const override{
        return _codecId;
    }
private:
    CodecId _codecId;
};

class AudioMeta : public Metadata{
public:
    using Ptr = std::shared_ptr<AudioMeta>;

    AudioMeta(const AudioTrack::Ptr &audio);
    virtual ~AudioMeta() = default;

    CodecId getCodecId() const override{
        return _codecId;
    }
private:
    CodecId _codecId;
};

//根据音频track获取flags
uint8_t getAudioRtmpFlags(const Track::Ptr &track);

}//namespace mediakit
#endif//__rtmp_h
