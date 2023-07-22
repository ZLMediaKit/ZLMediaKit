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
    static constexpr uint8_t kFlvVersion = 1;
    static constexpr uint8_t kFlvHeaderLength = 9;
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
    //固定为0
    uint32_t previous_tag_size0;
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

    // video config frame和key frame都返回true
    // 用于gop缓存定位
    bool isVideoKeyFrame() const;

    // aac config或h264/h265 config返回true，支持增强型rtmp
    // 用于缓存解码配置信息
    bool isConfigFrame() const;

    int getRtmpCodecId() const;
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

////////////////// rtmp video //////////////////////////
//https://rtmp.veriskope.com/pdf/video_file_format_spec_v10_1.pdf

// UB [4]; Type of video frame.
enum class RtmpFrameType : uint8_t {
    reserved = 0,
    key_frame = 1, // key frame (for AVC, a seekable frame)
    inter_frame = 2, // inter frame (for AVC, a non-seekable frame)
    disposable_inter_frame = 3, // disposable inter frame (H.263 only)
    generated_key_frame = 4, // generated key frame (reserved for server use only)
    video_info_frame = 5, // video info/command frame
};

// UB [4]; Codec Identifier.
enum class RtmpVideoCodec : uint8_t {
    h263 = 2, // Sorenson H.263
    screen_video = 3, // Screen video
    vp6 = 4, // On2 VP6
    vp6_alpha = 5, // On2 VP6 with alpha channel
    screen_video2 = 6, // Screen video version 2
    h264 = 7, // avc
    h265 = 12, // 国内扩展
};

// UI8;
enum class RtmpH264PacketType : uint8_t {
    h264_config_header = 0, // AVC or HEVC sequence header(sps/pps)
    h264_nalu = 1, // AVC or HEVC NALU
    h264_end_seq = 2, // AVC or HEVC end of sequence (lower level NALU sequence ender is not REQUIRED or supported)
};

// https://github.com/veovera/enhanced-rtmp/blob/main/enhanced-rtmp.pdf
// UB[4]
enum class RtmpPacketType : uint8_t {
    PacketTypeSequenceStart = 0,
    PacketTypeCodedFrames = 1,
    PacketTypeSequenceEnd = 2,

    // CompositionTime Offset is implied to equal zero. This is
    // an optimization to save putting SI24 composition time value of zero on
    // the wire. See pseudo code below in the VideoTagBody section
    PacketTypeCodedFramesX = 3,

    // VideoTagBody does not contain video data. VideoTagBody
    // instead contains an AMF encoded metadata. See Metadata Frame
    // section for an illustration of its usage. As an example, the metadata
    // can be HDR information. This is a good way to signal HDR
    // information. This also opens up future ways to express additional
    // metadata that is meant for the next video sequence.
    //
    // note: presence of PacketTypeMetadata means that FrameType
    // flags at the top of this table should be ignored
    PacketTypeMetadata = 4,

    // Carriage of bitstream in MPEG-2 TS format
    // note: PacketTypeSequenceStart and PacketTypeMPEG2TSSequenceStart
    // are mutually exclusive
    PacketTypeMPEG2TSSequenceStart = 5,
};

////////////////// rtmp audio //////////////////////////
//https://rtmp.veriskope.com/pdf/video_file_format_spec_v10_1.pdf

// UB [4]; Format of SoundData
enum class RtmpAudioCodec : uint8_t {
    /**
    0 = Linear PCM, platform endian
    1 = ADPCM
    2 = MP3
    3 = Linear PCM, little endian
    4 = Nellymoser 16 kHz mono
    5 = Nellymoser 8 kHz mono
    6 = Nellymoser
    7 = G.711 A-law logarithmic PCM
    8 = G.711 mu-law logarithmic PCM
    9 = reserved
    10 = AAC
    11 = Speex
    14 = MP3 8 kHz
    15 = Device-specific sound
     */
    g711a = 7,
    g711u = 8,
    aac = 10,
    opus = 13 // 国内扩展
};

// UI8;
enum class RtmpAACPacketType : uint8_t {
    aac_config_header = 0, // AAC sequence header
    aac_raw = 1, // AAC raw
};

////////////////////////////////////////////

struct RtmpPacketInfo {
    CodecId codec = CodecInvalid;
    bool is_enhanced;
    union {
        struct {
            RtmpFrameType frame_type;
            RtmpPacketType pkt_type;   // enhanced = true
            RtmpH264PacketType h264_pkt_type; // enhanced = false
        } video;
    };
};
// https://github.com/veovera/enhanced-rtmp
CodecId parseVideoRtmpPacket(const uint8_t *data, size_t size, RtmpPacketInfo *info = nullptr);

}//namespace mediakit
#endif//__rtmp_h
