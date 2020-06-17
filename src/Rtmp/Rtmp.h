/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/Buffer.h"
#include "Network/sockutil.h"
#include "amf.h"
#include "Extension/Track.h"

using namespace toolkit;

#define PORT	1935
#define DEFAULT_CHUNK_LEN	128

#if !defined(_WIN32)
#define PACKED	__attribute__((packed))
#else
#define PACKED
#endif //!defined(_WIN32)


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

#define CHUNK_SERVER_REQUEST			2 /*服务器像客户端发出请求时的chunkID*/
#define CHUNK_CLIENT_REQUEST_BEFORE		3 /*客户端在createStream前,向服务器发出请求的chunkID*/
#define CHUNK_CLIENT_REQUEST_AFTER		4 /*客户端在createStream后,向服务器发出请求的chunkID*/
#define CHUNK_AUDIO						6 /*音频chunkID*/
#define CHUNK_VIDEO						7 /*视频chunkID*/

#define FLV_KEY_FRAME				1
#define FLV_INTER_FRAME				2

#define FLV_CODEC_AAC 10
#define FLV_CODEC_H264 7
#define FLV_CODEC_H265 12
#define FLV_CODEC_G711A 7
#define FLV_CODEC_G711U 8


namespace mediakit {

#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

class RtmpHandshake {
public:
    RtmpHandshake(uint32_t _time, uint8_t *_random = nullptr) {
        _time = htonl(_time);
        memcpy(timeStamp, &_time, 4);
        if (!_random) {
            random_generate((char *) random, sizeof(random));
        } else {
            memcpy(random, _random, sizeof(random));
        }
    }
    uint8_t timeStamp[4];
    uint8_t zero[4] = {0};
    uint8_t random[RANDOM_LEN];
    void random_generate(char* bytes, int size) {
        static char cdata[] = { 0x73, 0x69, 0x6d, 0x70, 0x6c, 0x65, 0x2d, 0x72,
            0x74, 0x6d, 0x70, 0x2d, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72,
            0x2d, 0x77, 0x69, 0x6e, 0x6c, 0x69, 0x6e, 0x2d, 0x77, 0x69,
            0x6e, 0x74, 0x65, 0x72, 0x73, 0x65, 0x72, 0x76, 0x65, 0x72,
            0x40, 0x31, 0x32, 0x36, 0x2e, 0x63, 0x6f, 0x6d };
        for (int i = 0; i < size; i++) {
            bytes[i] = cdata[rand() % (sizeof(cdata) - 1)];
        }
    }
}PACKED;

class RtmpHeader {
public:
    uint8_t flags;
    uint8_t timeStamp[3];
    uint8_t bodySize[3];
    uint8_t typeId;
    uint8_t streamId[4]; /* Note, this is little-endian while others are BE */
}PACKED;

#if defined(_WIN32)
#pragma pack(pop)
#endif // defined(_WIN32)

class RtmpPacket : public Buffer{
public:
    typedef std::shared_ptr<RtmpPacket> Ptr;
    uint8_t typeId;
    uint32_t bodySize = 0;
    uint32_t timeStamp = 0;
    bool hasAbsStamp = false;
    bool hasExtStamp = false;
    uint32_t deltaStamp = 0;
    uint32_t streamId;
    uint32_t chunkId;
    std::string strBuf;
public:
    char *data() const override{
        return (char*)strBuf.data();
    }
    uint32_t size() const override {
        return strBuf.size();
    };
public:
    RtmpPacket() = default;
    RtmpPacket(const RtmpPacket &that) = delete;
    RtmpPacket &operator=(const RtmpPacket &that) = delete;
    RtmpPacket &operator=(RtmpPacket &&that) = delete;

    RtmpPacket(RtmpPacket &&that){
        typeId = that.typeId;
        bodySize = that.bodySize;
        timeStamp = that.timeStamp;
        hasAbsStamp = that.hasAbsStamp;
        hasExtStamp = that.hasExtStamp;
        deltaStamp = that.deltaStamp;
        streamId = that.streamId;
        chunkId = that.chunkId;
        strBuf = std::move(that.strBuf);
    }
    bool isVideoKeyFrame() const {
        return typeId == MSG_VIDEO && (uint8_t) strBuf[0] >> 4 == FLV_KEY_FRAME && (uint8_t) strBuf[1] == 1;
    }
    bool isCfgFrame() const {
        switch (typeId){
            case MSG_VIDEO : return strBuf[1] == 0;
            case MSG_AUDIO : {
                switch (getMediaType()){
                    case FLV_CODEC_AAC : return strBuf[1] == 0;
                    default : return false;
                }
            }
            default : return false;
        }
    }
    int getMediaType() const {
        switch (typeId) {
            case MSG_VIDEO : return (uint8_t) strBuf[0] & 0x0F;
            case MSG_AUDIO : return (uint8_t) strBuf[0] >> 4;
            default : return 0;
        }
    }
    int getAudioSampleRate() const {
        if (typeId != MSG_AUDIO) {
            return 0;
        }
        int flvSampleRate = ((uint8_t) strBuf[0] & 0x0C) >> 2;
        const static int sampleRate[] = { 5512, 11025, 22050, 44100 };
        return sampleRate[flvSampleRate];
    }
    int getAudioSampleBit() const {
        if (typeId != MSG_AUDIO) {
            return 0;
        }
        int flvSampleBit = ((uint8_t) strBuf[0] & 0x02) >> 1;
        const static int sampleBit[] = { 8, 16 };
        return sampleBit[flvSampleBit];
    }
    int getAudioChannel() const {
        if (typeId != MSG_AUDIO) {
            return 0;
        }
        int flvStereoOrMono = (uint8_t) strBuf[0] & 0x01;
        const static int channel[] = { 1, 2 };
        return channel[flvStereoOrMono];
    }
};

/**
 * rtmp metadata基类，用于描述rtmp格式信息
 */
class Metadata : public CodecInfo{
public:
    typedef std::shared_ptr<Metadata> Ptr;

    Metadata():_metadata(AMF_OBJECT){}
    virtual ~Metadata(){}
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
    typedef std::shared_ptr<TitleMeta> Ptr;

    TitleMeta(float dur_sec = 0,
              uint64_t fileSize = 0,
              const map<string,string> &header = map<string,string>()){
        _metadata.set("duration", dur_sec);
        _metadata.set("fileSize", 0);
        _metadata.set("server",SERVER_NAME);
        for (auto &pr : header){
            _metadata.set(pr.first, pr.second);
        }
    }

    CodecId getCodecId() const override{
        return CodecInvalid;
    }
};

class VideoMeta : public Metadata{
public:
    typedef std::shared_ptr<VideoMeta> Ptr;

    VideoMeta(const VideoTrack::Ptr &video,int datarate = 5000);
    virtual ~VideoMeta(){}

    CodecId getCodecId() const override{
        return _codecId;
    }
private:
    CodecId _codecId;
};

class AudioMeta : public Metadata{
public:
    typedef std::shared_ptr<AudioMeta> Ptr;

    AudioMeta(const AudioTrack::Ptr &audio,int datarate = 160);

    virtual ~AudioMeta(){}

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
