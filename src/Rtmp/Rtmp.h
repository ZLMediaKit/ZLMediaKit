/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */
#ifndef __rtmp_h
#define __rtmp_h

#include <memory>
#include <string>
#include "Util/util.h"
#include "Util/logger.h"
#include "Network/sockutil.h"

using namespace std;
using namespace ZL::Util;
using namespace ZL::Network;

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

class RtmpPacket {
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
    bool isVideoKeyFrame() const {
        return typeId == MSG_VIDEO && (uint8_t) strBuf[0] >> 4 == FLV_KEY_FRAME
        && (uint8_t) strBuf[1] == 1;
    }
    bool isCfgFrame() const {
        return (typeId == MSG_VIDEO || typeId == MSG_AUDIO)
        && (uint8_t) strBuf[1] == 0;
    }
    int getMediaType() const {
        switch (typeId) {
            case MSG_VIDEO: {
                return (uint8_t) strBuf[0] & 0x0F;
            }
                break;
            case MSG_AUDIO: {
                return (uint8_t) strBuf[0] >> 4;
            }
                break;
            default:
                break;
        }
        return 0;
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
    string getH264SPS() const {
        string ret;
        if (getMediaType() != 7) {
            return ret;
        }
        if (!isCfgFrame()) {
            return ret;
        }
        if (strBuf.size() < 13) {
            WarnL << "bad H264 cfg!";
            return ret;
        }
        uint16_t sps_size ;
        memcpy(&sps_size,strBuf.data() + 11,2);
        sps_size = ntohs(sps_size);
        if ((int) strBuf.size() < 13 + sps_size) {
            WarnL << "bad H264 cfg!";
            return ret;
        }
        ret.assign(strBuf.data() + 13, sps_size);
        return ret;
    }
    string getH264PPS() const {
        string ret;
        if (getMediaType() != 7) {
            return ret;
        }
        if (!isCfgFrame()) {
            return ret;
        }
        if (strBuf.size() < 13) {
            WarnL << "bad H264 cfg!";
            return ret;
        }
        uint16_t sps_size ;
        memcpy(&sps_size,strBuf.data() + 11,2);
        sps_size = ntohs(sps_size);
        
        if ((int) strBuf.size() < 13 + sps_size + 1 + 2) {
            WarnL << "bad H264 cfg!";
            return ret;
        }
        uint16_t pps_size ;
        memcpy(&pps_size,strBuf.data() + 13 + sps_size + 1,2);
        pps_size = ntohs(pps_size);
        
        if ((int) strBuf.size() < 13 + sps_size + 1 + 2 + pps_size) {
            WarnL << "bad H264 cfg!";
            return ret;
        }
        ret.assign(strBuf.data() + 13 + sps_size + 1 + 2, pps_size);
        return ret;
    }
    string getAacCfg() const {
        string ret;
        if (getMediaType() != 10) {
            return ret;
        }
        if (!isCfgFrame()) {
            return ret;
        }
        if (strBuf.size() < 4) {
            WarnL << "bad aac cfg!";
            return ret;
        }
        ret = strBuf.substr(2, 2);
        return ret;
    }
};

#endif
