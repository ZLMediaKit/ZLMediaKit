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

}//namespace mediakit