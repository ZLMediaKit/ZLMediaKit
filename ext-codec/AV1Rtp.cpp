/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "AV1Rtp.h"
#include "Util/util.h"

namespace mediakit{

/**
https://aomediacodec.github.io/av1-rtp-spec/#44-av1-aggregation-header

0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|X|  CC   |M|     PT      |       sequence number         |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                           timestamp                           |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|           synchronization source (SSRC) identifier            |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|            contributing source (CSRC) identifiers             |
|                             ....                              |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|         0x100         |  0x0  |       extensions length       |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
|      ID       |  hdr_length   |                               |
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+                               |
|                                                               |
|          dependency descriptor (hdr_length #bytes)            |
|                                                               |
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               | Other rtp header extensions...|
+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
| AV1 aggr hdr  |                                               |
+-+-+-+-+-+-+-+-+                                               |
|                                                               |
|                   Bytes 2..N of AV1 payload                   |
|                                                               |
|                               +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                               :    OPTIONAL RTP padding       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

AV1 aggr hdr
0 1 2 3 4 5 6 7
+-+-+-+-+-+-+-+-+
|Z|Y| W |N|-|-|-|
+-+-+-+-+-+-+-+-+

Z：如果第一个OBU元素是前一个数据包中的OBU片段的续部，那么必须设置为1，否则必须设置为0。

Y：如果最后一个OBU元素将在下一个数据包中继续，必须设置为1，否则必须设置为0。

W：一个包含两个比特的字段，用于描述数据包中的OBU元素数量。该字段必须设置为0或与数据包中包含的OBU元素数量相等。如果设置为0，则每个OBU元素必须在前面带有一个长度字段。
  如果未设置为0（即W = 1、2或3），则最后一个OBU元素不能在前面带有长度字段。相反，数据包中最后一个OBU元素的长度可以按以下方式计算：
  最后一个OBU元素的长度 = RTP负载的长度 - 聚合头的长度 - 包括长度字段的先前OBU元素的长度

N：如果数据包是编码视频序列的第一个数据包，必须设置为1，否则必须设置为0。注意：如果N等于1，则Z必须等于0。

 */

#pragma pack(push, 1)

class AggrHdr {
public:
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned Z : 1;
    unsigned Y : 1;
    unsigned W : 2;
    unsigned N : 1;
    unsigned reserved : 3;
#else
    unsigned reserved : 3;
    unsigned N : 1;
    unsigned W : 2;
    unsigned Y : 1;
    unsigned Z : 1;
#endif
};

#pragma pack(pop)

static AV1Frame::Ptr createFrame(uint64_t stamp) {
    auto frame = FrameImp::create<AV1Frame>();
    frame->_prefix_size = 0;
    frame->_pts = stamp;
    frame->_dts = stamp;
    return frame;
}

bool AV1RtpDecoder::inputRtp(const RtpPacket::Ptr &rtp, bool) {
    auto payload_size = rtp->getPayloadSize();
    if (payload_size <= 1) {
        // 无实际负载
        return false;
    }
    auto frame = rtp->getPayload();
    auto stamp = rtp->getStampMS();
    auto seq = rtp->getSeq();
    AggrHdr *hdr = (AggrHdr *)frame;

    CHECK(!hdr->N || !hdr->Z, "If N equals 1 then Z must equal 0 in AV1 Aggregation Header");

    if (!hdr->Z || rtp->getStamp() != _rtp_stamp) {
        // 帧开始，或者时间戳发生变化了，说明是新的一帧
        if (_frame) {
            // 还有上一帧残余数据
            WarnL << "AV1 frame dropped, pts:" << _frame->pts();
            _frame = nullptr;
        }
        if (!hdr->Z) {
            // 帧开始
            _frame = createFrame(stamp);
            _rtp_stamp = rtp->getStamp();
            _last_seq = seq - 1;
            if (hdr->N) {
                // 新的序列帧，停止gop丢包
                _gop_dropped = false;
            }
        }
    }

    if (!_frame) {
        WarnL << "Incomplete AV1 frame, rtp dropped: " << rtp->dumpString();
        _gop_dropped = true;
        return false;
    }

    if (static_cast<uint16_t>(_last_seq + 1) != seq) {
        WarnL << "AV1 rtp seq jumped: " << _last_seq << " -> " << seq - 1 << ", frame will be dropped";
        _frame = nullptr;
        return false;
    }
    _last_seq = seq;

    size_t offset = 1;
    for (size_t obu_index = 0; offset < payload_size;) {
        uint8_t *obu_data;
        size_t obu_size;
        bool new_frame = false;
        if (++obu_index < hdr->W || !hdr->W) {
            new_frame = true;
            // W为0，或者W不为零，且不是最后一个 OBU element，那么存在OBU element size (leb128)字段
            size_t consumed;
            obu_size = AV1Track::leb128(frame + offset, payload_size - offset, consumed);
            obu_data = frame + offset + consumed;
            offset += (consumed + obu_size);
            CHECK(offset <= payload_size, "Invalid av1 rtp obu element size");
        } else {
            new_frame = false;
            obu_size = payload_size - offset;
            obu_data = frame + offset;
            offset += obu_size;
        }
        _frame->_buffer.append((char *)obu_data, obu_size);
        if (new_frame) {
            flushFrame();
            _frame = createFrame(stamp);
        }
    }
    CHECK(offset == payload_size, "Invalid av1 rtp");

    if (!hdr->Y || rtp->getHeader()->mark) {
        // 帧结束
        flushFrame();
    }

    // 编码视频序列的第一个数据包
    return hdr->N;
}

void AV1RtpDecoder::flushFrame() {
    if (!_gop_dropped) {
        RtpCodec::inputFrame(_frame);
    } else {
        WarnL << "AV1 frame will be dropped: " << _frame->pts() << " until next gop";
    }
    _frame = nullptr;
}

////////////////////////////////////////////////////////////////////////

bool AV1RtpEncoder::inputFrame(const Frame::Ptr &frame) {
    auto len = frame->size() - frame->prefixSize();
    auto ptr = frame->data() + frame->prefixSize();

    if (frame->configFrame()) {
        uint8_t leb128[12];
        auto bytes = AV1Track::write_leb128(len, leb128, sizeof(leb128));

        std::string buf;
        buf.resize(1 + bytes + len);
        auto header = (AggrHdr *)(buf.data());
        header->Z = 0; // 帧开始
        header->Y = 1; // 后续还有帧
        header->W = 2; // rtp中有2个obu
        header->N = 1;
        memcpy((char *)buf.data() + 1, leb128, bytes);
        memcpy((char *)buf.data() + 1 + bytes, ptr, len);
        _config_rtp = std::move(buf);
        CHECK(_config_rtp.size() < getRtpInfo().getMaxSize());
        return true;
    }

    bool first_element = true;
    if (!_config_rtp.empty()) {
        first_element = false;
        auto remain_size = std::min<size_t>(getRtpInfo().getMaxSize() - _config_rtp.size(), len);
        _config_rtp.append(ptr, remain_size);
        ptr += remain_size;
        len -= remain_size;
        if (!len) {
            auto header = (AggrHdr *)(_config_rtp.data());
            header->Y = 0;
        }
        auto rtp = getRtpInfo().makeRtp(TrackVideo, _config_rtp.data(), _config_rtp.size(), !len, frame->pts());
        RtpCodec::inputRtp(rtp, true);
        _config_rtp.clear();
        if (!len) {
            return true;
        }
    }

    auto max_size = getRtpInfo().getMaxSize();
    if (len + 1 <= max_size) {
        auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, len + 1, true, frame->pts());
        uint8_t *payload = rtp->getPayload();
        union {
            AggrHdr header;
            uint8_t header_char;
        };
        header.Z = !first_element; // 帧开始
        header.Y = 0; // 帧结束
        header.W = 1; // rtp中只有一个obu，且没leb128长度
        header.N = 0;
        payload[0] = header_char;
        memcpy(payload + 1, (uint8_t *)ptr, len);
        RtpCodec::inputRtp(rtp, false);
    } else {
        while (len > 0) {
            auto obu_element_size = std::min<size_t>(len, max_size - 1);
            len -= obu_element_size;
            auto rtp = getRtpInfo().makeRtp(TrackVideo, nullptr, obu_element_size + 1, len == 0, frame->pts());
            uint8_t *payload = rtp->getPayload();
            union {
                AggrHdr header;
                uint8_t header_char;
            };
            header.Z = !first_element; // 帧开始
            header.Y = len > 0; // 帧结束
            header.W = 1; // rtp中只有一个obu，且没leb128长度
            header.N = 0;
            first_element = false;
            payload[0] = header_char;
            memcpy(payload + 1, (uint8_t *)ptr, obu_element_size);
            ptr += obu_element_size;
            RtpCodec::inputRtp(rtp, false);
        }
    }
    return true;
}

}//namespace mediakit
