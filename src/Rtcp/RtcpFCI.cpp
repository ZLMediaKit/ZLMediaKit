/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtcpFCI.h"
#include "Util/logger.h"
using namespace toolkit;

namespace mediakit {

void FCI_SLI::check(size_t size){
    CHECK(size >= kSize);
}

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

void FCI_FIR::check(size_t size){
    CHECK(size >= kSize);
}

uint32_t FCI_FIR::getSSRC() const{
    return ntohl(ssrc);
}

uint8_t FCI_FIR::getSeq() const{
    return seq_number;
}

uint32_t FCI_FIR::getReserved() const{
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

void FCI_REMB::check(size_t size){
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
    FCI_REMB *thiz = (FCI_REMB *) ret.data();
    memcpy(thiz->magic, kRembMagic, sizeof(magic));

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
    for (auto ssrc : ssrcs) {
        *(ptr++) = htonl(ssrc);
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
    auto ptr = ssrc_feedback;
    while (num_ssrc--) {
        ret.emplace_back(ntohl(*ptr++));
    }
    return ret;
}

string FCI_REMB::dumpString() const {
    _StrPrinter printer;
    printer << "bitrate:" << getBitRate() << ", ssrc:";
    for (auto &ssrc : ((FCI_REMB *) this)->getSSRC()) {
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

void FCI_NACK::check(size_t size){
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
    //nack第一个包丢包
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

class RunLengthChunk {
public:
    static size_t constexpr kSize = 2;
    //  0                   1
    //  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |T| S |       Run Length        |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#if __BYTE_ORDER == __BIG_ENDIAN
    uint16_t type: 1;
    uint16_t symbol: 2;
    uint16_t run_length_high: 5;
#else
    // Run Length 高5位
    uint16_t run_length_high: 5;
    //参考SymbolStatus定义
    uint16_t symbol: 2;
    //固定为0
    uint16_t type: 1;
#endif
    // Run Length 低8位
    uint16_t run_length_low: 8;

    //获取Run Length
    uint16_t getRunLength() const;
    //构造函数
    RunLengthChunk(SymbolStatus status, uint16_t run_length);
    //打印本对象
    string dumpString() const;
} PACKED;

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

class StatusVecChunk {
public:
    static size_t constexpr kSize = 2;
    // 0                   1
    // 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
    // |T|S|       symbol list         |
    // +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
#if __BYTE_ORDER == __BIG_ENDIAN
    uint16_t type: 1;
    uint16_t symbol: 1;
    uint16_t symbol_list_high: 6;
#else
    // symbol_list 高6位
    uint16_t symbol_list_high: 6;
    //symbol_list中元素是1个还是2个bit
    uint16_t symbol: 1;
    //固定为1
    uint16_t type: 1;
#endif
    // symbol_list 低8位
    uint16_t symbol_list_low: 8;

    //获取symbollist
    vector<SymbolStatus> getSymbolList() const;
    //构造函数
    StatusVecChunk(const vector<SymbolStatus> &status);
    //打印本对象
    string dumpString() const;
} PACKED;

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

void FCI_TWCC::check(size_t size){
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
        case SymbolStatus::reserved : {
            //没有时间戳
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
    auto seq = getBaseSeq();
    auto rtp_count = getPacketCount();
    for (uint8_t i = 0; i < rtp_count;) {
        CHECK(ptr + RunLengthChunk::kSize <= end);
        RunLengthChunk *chunk = (RunLengthChunk *) ptr;
        if (!chunk->type) {
            //RunLengthChunk
            for (auto j = 0; j < chunk->getRunLength(); ++j) {
                ret.emplace(seq++, std::make_pair((SymbolStatus) chunk->symbol, 0));
                if (++i >= rtp_count) {
                    break;
                }
            }
        } else {
            //StatusVecChunk
            StatusVecChunk *chunk = (StatusVecChunk *) ptr;
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
        pr.second.second = 250 * getRecvDelta(pr.second.first, ptr, end);
    }
    return ret;
}

string FCI_TWCC::dumpString(size_t total_size) const {
    _StrPrinter printer;
    auto map = getPacketChunkList(total_size);
    printer << "twcc fci, base_seq:" << getBaseSeq() << ", pkt_status_count:" << getPacketCount() << ", ref time:" << getReferenceTime() << ", fb count:" << (int)fb_pkt_count << "\n";
    for (auto &pr : map) {
        printer << "rtp seq:" << pr.first <<", packet status:" << (int)(pr.second.first) << ", delta:" << pr.second.second << "\n";
    }
    return std::move(printer);
}

}//namespace mediakit

#if 1
using namespace mediakit;
void testFCI() {
    {
        FCI_SLI fci(8191, 0, 63);
        InfoL << hexdump(&fci, FCI_SLI::kSize) << fci.dumpString();
    }
    {
        FCI_FIR fci(123456, 139, 456789);
        InfoL << hexdump(&fci, FCI_FIR::kSize) << fci.dumpString();
    }
    {
        auto str = FCI_REMB::create({1234, 2345, 5678}, 4 * 1024 * 1024);
        FCI_REMB *ptr = (FCI_REMB *) str.data();
        InfoL << hexdump(str.data(), str.size()) << ptr->dumpString();
    }
    {
        FCI_NACK nack(1234, vector<bool>({1, 0, 0, 0, 1, 0, 1, 0, 1, 0}));
        InfoL << hexdump(&nack, FCI_NACK::kSize) << nack.dumpString();
    }

    {
        RunLengthChunk chunk(SymbolStatus::large_delta, 8024);
        InfoL << hexdump(&chunk, RunLengthChunk::kSize) << chunk.dumpString();
    }

    auto lam = [](const initializer_list<int> &lst) {
        vector<SymbolStatus> ret;
        for (auto &num : lst) {
            ret.emplace_back((SymbolStatus) num);
        }
        return ret;
    };
    {
        StatusVecChunk chunk(lam({0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1}));
        InfoL << hexdump(&chunk, StatusVecChunk::kSize) << chunk.dumpString();
    }
    {
        StatusVecChunk chunk(lam({0, 1, 2, 2, 0, 1, 2}));
        InfoL <<  hexdump(&chunk, StatusVecChunk::kSize) << chunk.dumpString();
    }
}
#endif