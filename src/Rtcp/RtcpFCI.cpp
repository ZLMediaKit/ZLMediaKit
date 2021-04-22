/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */


#include "assert.h"
#include "RtcpFCI.h"
#define CHECK(exp) Assert_Throw(!(exp), #exp, __FUNCTION__, __FILE__, __LINE__);

namespace mediakit {

FCI_SLI::FCI_SLI(uint16_t first, uint16_t number, uint8_t pic_id) {
    //13 bits
    first &= 0x1FFF;
    //13 bits
    number &= 0x1FFF;
    //6 bits
    pic_id &= 0x3F;
    data = (first << 19) | (number << 6) | pic_id;
    data = htonl(data);
}

uint16_t FCI_SLI::getFirst() const {
    return data >> 19;
}

uint16_t FCI_SLI::getNumber() const {
    return (data >> 6) & 0x1FFF;
}

uint8_t FCI_SLI::getPicID() const {
    return data & 0x3F;
}

void FCI_SLI::net2Host() {
    data = ntohl(data);
}

string FCI_SLI::dumpString() const {
    return StrPrinter << "First:" << getFirst() << ", Number:" << getNumber() << ", PictureID:" << (int)getPicID();
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FCI_FIR::net2Host() {
    ssrc = ntohl(ssrc);
    reserved = ntohl(reserved) >> 8;
}

string FCI_FIR::dumpString() const {
    return StrPrinter << "ssrc:" << ssrc << ", seq_number:" << seq_number << ", reserved:" << reserved;
}

FCI_FIR::FCI_FIR(uint32_t ssrc, uint8_t seq_number, uint32_t reserved) {
    this->ssrc = htonl(ssrc);
    this->seq_number = seq_number;
    this->reserved = htonl(reserved) >> 8;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

string FCI_REMB::create(const initializer_list<uint32_t> &ssrcs, uint32_t bitrate) {
    CHECK(ssrcs.size() > 0 && ssrcs.size() <= 0xFF);
    string ret;
    ret.resize(kSize + ssrcs.size() * 4);
    FCI_REMB *thiz = (FCI_REMB *) ret.data();
    memcpy(thiz->magic, "REMB", 4);

    /* bitrate --> BR Exp/BR Mantissa */
    uint8_t b = 0;
    uint8_t exp = 0;
    uint32_t mantissa = 0;
    for (b = 0; b < 32; b++) {
        if (bitrate <= ((uint32_t) 0x3FFFF << b)) {
            exp = b;
            break;
        }
    }
    if (b > 31) {
        b = 31;
    }
    mantissa = bitrate >> b;
    //Num SSRC (8 bits)
    thiz->bitrate[0] = ssrcs.size() & 0xFF;
    //BR Exp (6 bits)/BR Mantissa (18 bits)
    thiz->bitrate[1] = (uint8_t) ((exp << 2) + ((mantissa >> 16) & 0x03));
    //BR Mantissa (18 bits)
    thiz->bitrate[2] = (uint8_t) (mantissa >> 8);
    //BR Mantissa (18 bits)
    thiz->bitrate[3] = (uint8_t) (mantissa);

    //设置ssrc列表
    auto ptr = thiz->ssrc_feedback;
    for (auto &ssrc : ssrcs) {
        *(ptr++) = htonl(ssrc);
    }
    return ret;
}

void FCI_REMB::net2Host(size_t total_size) {
    CHECK(total_size >= kSize);
    CHECK(memcmp(magic, "REMB", 4) == 0);
    auto num_ssrc = bitrate[0];
    auto expect_size = kSize + 4 * num_ssrc;
    CHECK(total_size == expect_size);
    auto ptr = ssrc_feedback;
    while (num_ssrc--) {
        *(ptr++) = ntohl(*ptr);
    }
}

uint32_t FCI_REMB::getBitRate() const {
    uint8_t exp = (bitrate[1] >> 2) & 0x3F;
    uint32_t mantissa = (bitrate[1] & 0x03) << 16;
    mantissa += (bitrate[2] << 8);
    mantissa += (bitrate[3]);
    return mantissa << exp;
}

vector<uint32_t *> FCI_REMB::getSSRC() {
    vector<uint32_t *> ret;
    auto num_ssrc = bitrate[0];
    auto ptr = ssrc_feedback;
    while (num_ssrc--) {
        ret.emplace_back(ptr++);
    }
    return ret;
}

string FCI_REMB::dumpString() const {
    _StrPrinter printer;
    printer << "bitrate:" << getBitRate() << ", ssrc:";
    for (auto &ssrc : ((FCI_REMB *) this)->getSSRC()) {
        printer << *ssrc << " ";
    }
    return printer;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void FCI_NACK::net2Host() {
    pid = ntohs(pid);
    blp = ntohs(blp);
}

vector<bool> FCI_NACK::getBitArray() const {
    vector<bool> ret;
    ret.resize(kBitSize);
    for (size_t i = 0; i < kBitSize; ++i) {
        ret[i] = blp & (1 << (kBitSize - i - 1));
    }
    return ret;
}

string FCI_NACK::dumpString() const {
    _StrPrinter printer;
    printer << "pid:" << pid << ",blp:";
    for (auto &flag : getBitArray()) {
        printer << flag << " ";
    }
    return std::move(printer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

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

string RunLengthChunk::dumpString() const{
    _StrPrinter printer;
    printer << "run length chunk, symbol:" << (int)symbol << ", run length:" << getRunLength();
    return std::move(printer);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

StatusVecChunk::StatusVecChunk(const vector<SymbolStatus> &status) {
    uint16_t value = 0;
    type = 1;
    if (status.size() == 14) {
        symbol = 0;
    } else if (status.size() == 7) {
        symbol = 1;
    } else {
        //非法
        CHECK(0);
    }
    int i = 13;
    for (auto &item : status) {
        CHECK(item <= SymbolStatus::reserved);
        if (!symbol) {
            CHECK(item <= SymbolStatus::small_delta);
            value |= (int) item << i;
            --i;
        } else {
            value |= (int) item << (i - 1);
            i -= 2;
        }
    }
    symbol_list_low = value & 0xFF;
    symbol_list_high = (value >> 8 ) & 0x1F;
}

vector<SymbolStatus> StatusVecChunk::getSymbolList() const {
    CHECK(type == 1);
    vector<SymbolStatus> ret;
    auto thiz = ntohs(*((uint16_t *) this));
    if (symbol == 0) {
        //s = 0 时，表示symbollist的每一个bit能表示一个数据包的到达状态
        for (int i = 13; i >= 0; --i) {
            SymbolStatus status = (SymbolStatus) ((bool) (thiz & (1 << i)));
            ret.emplace_back(status);
        }
    } else {
        //s = 1 时，表示symbollist每两个bit表示一个数据包的状态
        for (int i = 12; i >= 0; i -= 2) {
            SymbolStatus status = (SymbolStatus) ((thiz & (3 << i)) >> i);
            ret.emplace_back(status);
        }
    }
    return ret;
}

string StatusVecChunk::dumpString() const {
    _StrPrinter printer;
    printer << "status vector chunk, symbol:" << (int) symbol << ", symbol list:";
    auto vec = getSymbolList();
    for (auto &item : vec) {
        printer << (int) item << " ";
    }
    return std::move(printer);
}

///////////////////////////////////////////////////////

void FCI_TWCC::net2Host(size_t total_size) {
    CHECK(total_size >= kSize);
    base_seq = ntohs(base_seq);
    pkt_status_count = ntohs(pkt_status_count);
}

uint32_t FCI_TWCC::getReferenceTime() const {
    uint32_t ret = 0;
    ret |= ref_time[0] << 16;
    ret |= ref_time[1] << 8;
    ret |= ref_time[2];
    return ret;
}
//3.1.5.  Receive Delta
//
//   Deltas are represented as multiples of 250us:
//
//   o  If the "Packet received, small delta" symbol has been appended to
//      the status list, an 8-bit unsigned receive delta will be appended
//      to recv delta list, representing a delta in the range [0, 63.75]
//      ms.
//
//   o  If the "Packet received, large or negative delta" symbol has been
//      appended to the status list, a 16-bit signed receive delta will be
//      appended to recv delta list, representing a delta in the range
//      [-8192.0, 8191.75] ms.
//
//   o  If the delta exceeds even the larger limits, a new feedback
//      message must be used, where the 24-bit base receive delta can
//      cover very large gaps.
//
//   The smaller receive delta upper bound of 63.75 ms means that this is
//   only viable at about 1000/25.5 ~= 16 packets per second and above.
//   With a packet size of 1200 bytes/packet that amounts to a bitrate of
//   about 150 kbit/s.
//
//   The 0.25 ms resolution means that up to 4000 packets per second can
//   be represented.  With a 1200 bytes/packet payload, that amounts to
//   38.4 Mbit/s payload bandwidth.

static int16_t getRecvDelta(SymbolStatus status, uint8_t *&ptr, const uint8_t *end){
    int16_t delta = 0;
    switch (status) {
        case SymbolStatus::not_received : {
            //丢包， recv delta为0个字节
            delta = 0;
            break;
        }
        case SymbolStatus::small_delta : {
            CHECK(ptr + 1 <= end);
            //时间戳增量小于256， recv delta为1个字节
            delta = *ptr;
            ptr += 1;
            break;
        }
        case SymbolStatus::large_delta : {
            CHECK(ptr + 2 <= end);
            //时间戳增量256~65535间，recv delta为2个字节
            delta = *ptr << 8 | *(ptr + 1);
            ptr += 2;
            break;
        }
        default:
            //这个逻辑分支不可达到
            CHECK(0);
            break;
    }
    return delta;
}

map<uint16_t, std::pair<SymbolStatus, uint32_t/*stamp*/> > FCI_TWCC::getPacketChunkList(size_t total_size) const {
    map<uint16_t, std::pair<SymbolStatus, uint32_t> > ret;
    auto ptr = (uint8_t *) this + kSize;
    auto end = (uint8_t *) this + total_size;
    CHECK(ptr < end);
    auto seq = base_seq;

    for (uint8_t i = 0; i < pkt_status_count;) {
        CHECK(ptr + RunLengthChunk::kSize <= end)
        RunLengthChunk *chunk = (RunLengthChunk *) ptr;
        if (!chunk->type) {
            //RunLengthChunk
            for (auto j = 0; j < chunk->getRunLength(); ++j) {
                ret.emplace(seq++, std::make_pair((SymbolStatus) chunk->symbol, 0));
                ++i;
            }
        } else {
            //StatusVecChunk
            StatusVecChunk *chunk = (StatusVecChunk *) ptr;
            for (auto &symbol : chunk->getSymbolList()) {
                ret.emplace(seq++, std::make_pair(symbol, 0));
                ++i;
            }
        }
        ptr += 2;
    }
    for (auto &pr : ret) {
        CHECK(ptr <= end)
        pr.second.second = 250 * getRecvDelta(pr.second.first, ptr, end);
    }
    return ret;
}

string FCI_TWCC::dumpString(size_t total_size) const {
    _StrPrinter printer;
    auto map = getPacketChunkList(total_size);
    printer << "twcc fci, base_seq:" << base_seq << ",pkt_status_count:" << pkt_status_count << ", ref time:" << getReferenceTime() << ", fb count:" << (int)fb_pkt_count << "\n";
    for (auto &pr : map) {
        printer << "rtp seq:" << pr.first <<", packet status:" << (int)(pr.second.first) << ", delta:" << pr.second.second << "\n";
    }
    return std::move(printer);
}

}//namespace mediakit