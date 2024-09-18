/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtcpFCI.h"
#include "Util/logger.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

void FCI_SLI::check(size_t size) {
    CHECK(size >= kSize);
}

FCI_SLI::FCI_SLI(uint16_t first, uint16_t number, uint8_t pic_id) {
    // 13 bits
    first &= 0x1FFF;
    // 13 bits
    number &= 0x1FFF;
    // 6 bits
    pic_id &= 0x3F;
    data = (first << 19) | (number << 6) | pic_id;
    data = htonl(data);
}

uint16_t FCI_SLI::getFirst() const {
    return ntohl(data) >> 19;
}

uint16_t FCI_SLI::getNumber() const {
    return (ntohl(data) >> 6) & 0x1FFF;
}

uint8_t FCI_SLI::getPicID() const {
    return ntohl(data) & 0x3F;
}

string FCI_SLI::dumpString() const {
    return StrPrinter << "First:" << getFirst() << ", Number:" << getNumber() << ", PictureID:" << (int)getPicID();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FCI_FIR::check(size_t size) {
    CHECK(size >= kSize);
}

uint32_t FCI_FIR::getSSRC() const {
    return ntohl(ssrc);
}

uint8_t FCI_FIR::getSeq() const {
    return seq_number;
}

uint32_t FCI_FIR::getReserved() const {
    return (reserved[0] << 16) | (reserved[1] << 8) | reserved[2];
}

string FCI_FIR::dumpString() const {
    return StrPrinter << "ssrc:" << getSSRC() << ", seq_number:" << (int)getSeq() << ", reserved:" << getReserved();
}

FCI_FIR::FCI_FIR(uint32_t ssrc, uint8_t seq_number, uint32_t reserved) {
    this->ssrc = htonl(ssrc);
    this->seq_number = seq_number;
    this->reserved[0] = (reserved >> 16) & 0xFF;
    this->reserved[1] = (reserved >> 8) & 0xFF;
    this->reserved[2] = reserved & 0xFF;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

static const char kRembMagic[] = "REMB";

void FCI_REMB::check(size_t size) {
    CHECK(size >= kSize);
    CHECK(memcmp(magic, kRembMagic, sizeof(magic)) == 0);
    auto num_ssrc = bitrate[0];
    auto expect_size = kSize + 4 * num_ssrc;
    CHECK(size >= expect_size);
}

string FCI_REMB::create(const vector<uint32_t> &ssrcs, uint32_t bitrate) {
    CHECK(ssrcs.size() > 0 && ssrcs.size() <= 0xFF);
    string ret;
    ret.resize(kSize + ssrcs.size() * 4);
    FCI_REMB *thiz = (FCI_REMB *)ret.data();
    memcpy(thiz->magic, kRembMagic, sizeof(magic));

    /* bitrate --> BR Exp/BR Mantissa */
    uint8_t b = 0;
    uint8_t exp = 0;
    uint32_t mantissa = 0;
    for (b = 0; b < 32; b++) {
        if (bitrate <= ((uint32_t)0x3FFFF << b)) {
            exp = b;
            break;
        }
    }
    if (b > 31) {
        b = 31;
    }
    mantissa = bitrate >> b;
    // Num SSRC (8 bits)
    thiz->bitrate[0] = ssrcs.size() & 0xFF;
    // BR Exp (6 bits)/BR Mantissa (18 bits)
    thiz->bitrate[1] = (uint8_t)((exp << 2) + ((mantissa >> 16) & 0x03));
    // BR Mantissa (18 bits)
    thiz->bitrate[2] = (uint8_t)(mantissa >> 8);
    // BR Mantissa (18 bits)
    thiz->bitrate[3] = (uint8_t)(mantissa);

    // 设置ssrc列表  [AUTO-TRANSLATED:51a0df3a]
    // Set ssrc list
    int i = 0;
    for (auto ssrc : ssrcs) {
        thiz->ssrc_feedback[i++] = htonl(ssrc);
    }
    return ret;
}

uint32_t FCI_REMB::getBitRate() const {
    uint8_t exp = (bitrate[1] >> 2) & 0x3F;
    uint32_t mantissa = (bitrate[1] & 0x03) << 16;
    mantissa += (bitrate[2] << 8);
    mantissa += (bitrate[3]);
    return mantissa << exp;
}

vector<uint32_t> FCI_REMB::getSSRC() {
    vector<uint32_t> ret;
    auto num_ssrc = bitrate[0];
    int i = 0;
    while (num_ssrc--) {
        ret.emplace_back(ntohl(ssrc_feedback[i]));
        ++i;
    }
    return ret;
}

string FCI_REMB::dumpString() const {
    _StrPrinter printer;
    printer << "bitrate:" << getBitRate() << ", ssrc:";
    for (auto &ssrc : ((FCI_REMB *)this)->getSSRC()) {
        printer << ssrc << " ";
    }
    return std::move(printer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

FCI_NACK::FCI_NACK(uint16_t pid_h, const vector<bool> &type) {
    assert(type.size() <= kBitSize);
    uint16_t blp_h = 0;
    int i = 0;
    for (auto item : type) {
        if (item) {
            blp_h |= (1 << i);
        }
        ++i;
    }
    blp = htons(blp_h);
    pid = htons(pid_h);
}

void FCI_NACK::check(size_t size) {
    CHECK(size >= kSize);
}

uint16_t FCI_NACK::getPid() const {
    return ntohs(pid);
}

uint16_t FCI_NACK::getBlp() const {
    return ntohs(blp);
}

vector<bool> FCI_NACK::getBitArray() const {
    vector<bool> ret;
    ret.resize(kBitSize + 1);
    // nack第一个包丢包  [AUTO-TRANSLATED:357feeea]
    // Nack the first packet loss
    ret[0] = true;

    auto blp_h = getBlp();
    for (size_t i = 0; i < kBitSize; ++i) {
        ret[i + 1] = blp_h & (1 << i);
    }
    return ret;
}

string FCI_NACK::dumpString() const {
    _StrPrinter printer;
    auto pid = getPid();
    printer << "pid:" << pid << ",blp:" << getBlp() << ",dropped rtp seq:";
    for (auto flag : getBitArray()) {
        if (flag) {
            printer << pid << " ";
        }
        ++pid;
    }
    return std::move(printer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma pack(push, 1)
class RunLengthChunk {
public:
    static size_t constexpr kSize = 2;
    //  0                   1
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |T| S |       Run Length        |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#if __BYTE_ORDER == __BIG_ENDIAN
    uint16_t type : 1;
    uint16_t symbol : 2;
    uint16_t run_length_high : 5;
#else
    // Run Length 高5位  [AUTO-TRANSLATED:a1426130]
    // Run Length high 5 bits
    uint16_t run_length_high : 5;
    // 参考SymbolStatus定义  [AUTO-TRANSLATED:0268c65e]
    // Refer to SymbolStatus definition
    uint16_t symbol : 2;
    // 固定为0  [AUTO-TRANSLATED:7b516577]
    // Fixed to 0
    uint16_t type : 1;
#endif
    // Run Length 低8位  [AUTO-TRANSLATED:8984c00d]
    // Run Length low 8 bits
    uint16_t run_length_low : 8;

    // 获取Run Length  [AUTO-TRANSLATED:9fb792e6]
    // Get Run Length
    uint16_t getRunLength() const;
    // 构造函数  [AUTO-TRANSLATED:b9f7407d]
    // Constructor
    RunLengthChunk(SymbolStatus status, uint16_t run_length);
    // 打印本对象  [AUTO-TRANSLATED:e8bd8207]
    // Print this object
    string dumpString() const;
};
#pragma pack(pop)

RunLengthChunk::RunLengthChunk(SymbolStatus status, uint16_t run_length) {
    type = 0;
    symbol = (uint8_t)status & 0x03;
    run_length_high = (run_length >> 8) & 0x1F;
    run_length_low = run_length & 0xFF;
}

uint16_t RunLengthChunk::getRunLength() const {
    CHECK(type == 0);
    return run_length_high << 8 | run_length_low;
}

string RunLengthChunk::dumpString() const {
    _StrPrinter printer;
    printer << "run length chunk, symbol:" << (int)symbol << ", run length:" << getRunLength();
    return std::move(printer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
#pragma pack(push, 1)
class StatusVecChunk {
public:
    static size_t constexpr kSize = 2;
    // 0                   1
    // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |T|S|       symbol list         |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#if __BYTE_ORDER == __BIG_ENDIAN
    uint16_t type : 1;
    uint16_t symbol : 1;
    uint16_t symbol_list_high : 6;
#else
    // symbol_list 高6位  [AUTO-TRANSLATED:2ef1be51]
    // symbol_list high 6 bits
    uint16_t symbol_list_high : 6;
    // symbol_list中元素是1个还是2个bit  [AUTO-TRANSLATED:0f56756f]
    // Whether the element in symbol_list is 1 or 2 bits
    uint16_t symbol : 1;
    // 固定为1  [AUTO-TRANSLATED:1fb4c9c8]
    // Fixed to 1
    uint16_t type : 1;
#endif
    // symbol_list 低8位  [AUTO-TRANSLATED:257bf13b]
    // symbol_list low 8 bits
    uint16_t symbol_list_low : 8;

    // 获取symbollist  [AUTO-TRANSLATED:a24b8d73]
    // Get symbollist
    vector<SymbolStatus> getSymbolList() const;
    // 构造函数  [AUTO-TRANSLATED:b9f7407d]
    // Constructor
    StatusVecChunk(bool symbol_bit, const vector<SymbolStatus> &status);
    // 打印本对象  [AUTO-TRANSLATED:e8bd8207]
    // Print this object
    string dumpString() const;
};
#pragma pack(pop)

StatusVecChunk::StatusVecChunk(bool symbol_bit, const vector<SymbolStatus> &status) {
    CHECK(status.size() << symbol_bit <= 14);
    uint16_t value = 0;
    type = 1;
    symbol = symbol_bit;
    int i = 13;
    for (auto &item : status) {
        CHECK(item <= SymbolStatus::reserved);
        if (!symbol) {
            CHECK(item <= SymbolStatus::small_delta);
            value |= (int)item << i;
            --i;
        } else {
            value |= (int)item << (i - 1);
            i -= 2;
        }
    }
    symbol_list_low = value & 0xFF;
    symbol_list_high = (value >> 8) & 0x3F;
}

vector<SymbolStatus> StatusVecChunk::getSymbolList() const {
    CHECK(type == 1);
    vector<SymbolStatus> ret;
    auto thiz = ntohs(*((uint16_t *)this));
    if (symbol == 0) {
        // s = 0 时，表示symbollist的每一个bit能表示一个数据包的到达状态  [AUTO-TRANSLATED:89d8f104]
        // When s = 0, each bit in symbollist represents the arrival status of a data packet
        for (int i = 13; i >= 0; --i) {
            SymbolStatus status = (SymbolStatus)((bool)(thiz & (1 << i)));
            ret.emplace_back(status);
        }
    } else {
        // s = 1 时，表示symbollist每两个bit表示一个数据包的状态  [AUTO-TRANSLATED:fd7eb5fe]
        // When s = 1, every two bits in symbollist represent the status of a data packet
        for (int i = 12; i >= 0; i -= 2) {
            SymbolStatus status = (SymbolStatus)((thiz & (3 << i)) >> i);
            ret.emplace_back(status);
        }
    }
    return ret;
}

string StatusVecChunk::dumpString() const {
    _StrPrinter printer;
    printer << "status vector chunk, symbol:" << (int)symbol << ", symbol list:";
    auto vec = getSymbolList();
    for (auto &item : vec) {
        printer << (int)item << " ";
    }
    return std::move(printer);
}

///////////////////////////////////////////////////////

void FCI_TWCC::check(size_t size) {
    CHECK(size >= kSize);
}

uint16_t FCI_TWCC::getBaseSeq() const {
    return ntohs(base_seq);
}

uint16_t FCI_TWCC::getPacketCount() const {
    return ntohs(pkt_status_count);
}

uint32_t FCI_TWCC::getReferenceTime() const {
    uint32_t ret = 0;
    ret |= ref_time[0] << 16;
    ret |= ref_time[1] << 8;
    ret |= ref_time[2];
    return ret;
}
// 3.1.5.  Receive Delta
//
//    Deltas are represented as multiples of 250us:
//
//    o  If the "Packet received, small delta" symbol has been appended to
//       the status list, an 8-bit unsigned receive delta will be appended
//       to recv delta list, representing a delta in the range [0, 63.75]
//       ms.
//
//    o  If the "Packet received, large or negative delta" symbol has been
//       appended to the status list, a 16-bit signed receive delta will be
//       appended to recv delta list, representing a delta in the range
//       [-8192.0, 8191.75] ms.
//
//    o  If the delta exceeds even the larger limits, a new feedback
//       message must be used, where the 24-bit base receive delta can
//       cover very large gaps.
//
//    The smaller receive delta upper bound of 63.75 ms means that this is
//    only viable at about 1000/25.5 ~= 16 packets per second and above.
//    With a packet size of 1200 bytes/packet that amounts to a bitrate of
//    about 150 kbit/s.
//
//    The 0.25 ms resolution means that up to 4000 packets per second can
//    be represented.  With a 1200 bytes/packet payload, that amounts to
//    38.4 Mbit/s payload bandwidth.

static int16_t getRecvDelta(SymbolStatus status, uint8_t *&ptr, const uint8_t *end) {
    int16_t delta = 0;
    switch (status) {
    case SymbolStatus::not_received: {
        // 丢包， recv delta为0个字节  [AUTO-TRANSLATED:4312b1ce]
        // Packet loss, recv delta is 0 bytes
        break;
    }
    case SymbolStatus::small_delta: {
        CHECK(ptr + 1 <= end);
        // 时间戳增量小于256， recv delta为1个字节  [AUTO-TRANSLATED:4b93efeb]
        // Timestamp increment is less than 256, recv delta is 1 byte
        delta = *ptr;
        ptr += 1;
        break;
    }
    case SymbolStatus::large_delta: {
        CHECK(ptr + 2 <= end);
        // 时间戳增量256~65535间，recv delta为2个字节  [AUTO-TRANSLATED:989c8340]
        // Timestamp increment is between 256 and 65535, recv delta is 2 bytes
        delta = *ptr << 8 | *(ptr + 1);
        ptr += 2;
        break;
    }
    case SymbolStatus::reserved: {
        // 没有时间戳  [AUTO-TRANSLATED:0767909f]
        // No timestamp
        break;
    }
    default:
        // 这个逻辑分支不可达到  [AUTO-TRANSLATED:451ba1eb]
        // This logic branch cannot be reached
        CHECK(0);
        break;
    }
    return delta;
}

FCI_TWCC::TwccPacketStatus FCI_TWCC::getPacketChunkList(size_t total_size) const {
    TwccPacketStatus ret;
    auto ptr = (uint8_t *)this + kSize;
    auto end = (uint8_t *)this + total_size;
    CHECK(ptr < end);
    auto seq = getBaseSeq();
    auto rtp_count = getPacketCount();
    for (uint16_t i = 0; i < rtp_count;) {
        CHECK(ptr + RunLengthChunk::kSize <= end);
        RunLengthChunk *chunk = (RunLengthChunk *)ptr;
        if (!chunk->type) {
            // RunLengthChunk
            for (auto j = 0; j < chunk->getRunLength(); ++j) {
                ret.emplace(seq++, std::make_pair((SymbolStatus)chunk->symbol, 0));
                if (++i >= rtp_count) {
                    break;
                }
            }
        } else {
            // StatusVecChunk
            StatusVecChunk *chunk = (StatusVecChunk *)ptr;
            for (auto &symbol : chunk->getSymbolList()) {
                ret.emplace(seq++, std::make_pair(symbol, 0));
                if (++i >= rtp_count) {
                    break;
                }
            }
        }
        ptr += 2;
    }
    for (auto &pr : ret) {
        CHECK(ptr <= end);
        pr.second.second = getRecvDelta(pr.second.first, ptr, end);
    }
    return ret;
}

string FCI_TWCC::dumpString(size_t total_size) const {
    _StrPrinter printer;
    auto map = getPacketChunkList(total_size);
    printer << "twcc fci, base_seq:" << getBaseSeq() << ", pkt_status_count:" << getPacketCount()
            << ", ref time:" << getReferenceTime() << ", fb count:" << (int)fb_pkt_count << "\n";
    for (auto &pr : map) {
        printer << "rtp seq:" << pr.first << ", packet status:" << (int)(pr.second.first)
                << ", delta:" << pr.second.second << "\n";
    }
    return std::move(printer);
}

static void appendDeltaString(string &delta_str, FCI_TWCC::TwccPacketStatus &status, int count) {
    for (auto it = status.begin(); it != status.end() && count--;) {
        switch (it->second.first) {
        // large delta模式先写高字节，再写低字节  [AUTO-TRANSLATED:cb25ff45]
        // Large delta mode writes the high byte first, then the low byte
        case SymbolStatus::large_delta:
            delta_str.push_back((it->second.second >> 8) & 0xFF);
            // small delta模式只写低字节  [AUTO-TRANSLATED:5caea5d6]
            // Small delta mode only writes the low byte
        case SymbolStatus::small_delta:
            delta_str.push_back(it->second.second & 0xFF);
            break;
        default:
            break;
        }
        // 移除已经处理过的数据  [AUTO-TRANSLATED:106fb6db]
        // Remove the data that has been processed
        it = status.erase(it);
    }
}

string FCI_TWCC::create(uint32_t ref_time, uint8_t fb_pkt_count, TwccPacketStatus &status) {
    string fci;
    fci.resize(FCI_TWCC::kSize);
    FCI_TWCC *ptr = (FCI_TWCC *)(fci.data());
    ptr->base_seq = htons(status.begin()->first);
    ptr->pkt_status_count = htons(status.size());
    ptr->fb_pkt_count = fb_pkt_count;
    ptr->ref_time[0] = (ref_time >> 16) & 0xFF;
    ptr->ref_time[1] = (ref_time >> 8) & 0xFF;
    ptr->ref_time[2] = (ref_time >> 0) & 0xFF;

    string delta_str;
    while (!status.empty()) {
        {
            // 第一个rtp的状态  [AUTO-TRANSLATED:b0efb9ad]
            // The status of the first rtp
            auto symbol = status.begin()->second.first;
            int16_t count = 0;
            for (auto &pr : status) {
                if (pr.second.first != symbol) {
                    // 状态发送变更了，本chunk结束  [AUTO-TRANSLATED:f1c2c6d1]
                    // The status sending changed, this chunk ends
                    break;
                }
                if (++count >= (0xFFFF >> 3)) {
                    // RunLengthChunk 13个bit表明rtp个数，最多可以表述0xFFFF >> 3个rtp状态  [AUTO-TRANSLATED:c6c4de01]
                    // RunLengthChunk 13 bits indicate the number of rtp, at most 0xFFFF >> 3 rtp status can be expressed
                    break;
                }
            }
            if (count >= 7) {
                // 连续状态相同个数大于6个时，使用RunLengthChunk模式比较节省带宽  [AUTO-TRANSLATED:c243c73f]
                // When the number of consecutive states is greater than 6, using RunLengthChunk mode is more bandwidth-saving
                RunLengthChunk chunk(symbol, count);
                fci.append((char *)&chunk, RunLengthChunk::kSize);
                appendDeltaString(delta_str, status, count);
                continue;
            }
        }

        {
            // StatusVecChunk模式  [AUTO-TRANSLATED:612e548c]
            // StatusVecChunk mode
            // symbol_list中元素是1个bit  [AUTO-TRANSLATED:e5f8cbf8]
            // Elements in symbol_list are 1 bit
            auto symbol = 0;
            vector<SymbolStatus> vec;
            for (auto &pr : status) {
                vec.push_back(pr.second.first);
                if (pr.second.first >= SymbolStatus::large_delta) {
                    // symbol_list中元素是2个bit  [AUTO-TRANSLATED:43429094]
                    // Elements in symbol_list are 2 bits
                    symbol = 1;
                }

                if (vec.size() << symbol >= 14) {
                    // symbol为0时，最多存放14个rtp的状态  [AUTO-TRANSLATED:1da4fad6]
                    // When symbol is 0, at most 14 RTP statuses can be stored
                    // symbol为1时，最多存放7个rtp的状态  [AUTO-TRANSLATED:34f39e9a]
                    // When symbol is 1, at most 7 RTP statuses can be stored
                    break;
                }
            }
            vec.resize(MIN(vec.size(), (size_t)14 >> symbol));
            StatusVecChunk chunk(symbol, vec);
            fci.append((char *)&chunk, StatusVecChunk::kSize);
            appendDeltaString(delta_str, status, vec.size());
        }
    }

    // recv delta部分  [AUTO-TRANSLATED:f7e0d5bc]
    // recv delta part
    fci.append(delta_str);
    return fci;
}

} // namespace mediakit