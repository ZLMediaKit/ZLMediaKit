/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTCPFCI_H
#define ZLMEDIAKIT_RTCPFCI_H

#include "Rtcp.h"

namespace mediakit {

/////////////////////////////////////////// PSFB ////////////////////////////////////////////////////

//PSFB fmt = 2
//https://tools.ietf.org/html/rfc4585#section-6.3.2.2
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |            First        |        Number           | PictureID |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//First: 13 bits
//      The macroblock (MB) address of the first lost macroblock.  The MB
//      numbering is done such that the macroblock in the upper left
//      corner of the picture is considered macroblock number 1 and the
//      number for each macroblock increases from left to right and then
//      from top to bottom in raster-scan order (such that if there is a
//      total of N macroblocks in a picture, the bottom right macroblock
//      is considered macroblock number N).
//
//   Number: 13 bits
//      The number of lost macroblocks, in scan order as discussed above.
//
//   PictureID: 6 bits
//      The six least significant bits of the codec-specific identifier
//      that is used to reference the picture in which the loss of the
//      macroblock(s) has occurred.  For many video codecs, the PictureID
//      is identical to the Temporal Reference.
class FCI_SLI {
public:
    static size_t constexpr kSize = 4;

    FCI_SLI(uint16_t first, uint16_t number, uint8_t pic_id);
    uint16_t getFirst() const;
    uint16_t getNumber() const;
    uint8_t getPicID() const;
    void net2Host();
    string dumpString() const;

private:
    uint32_t data;
} PACKED;

//PSFB fmt = 3
//https://tools.ietf.org/html/rfc4585#section-6.3.3.2
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |      PB       |0| Payload Type|    Native RPSI bit string     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   defined per codec          ...                | Padding (0) |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class FCI_RPSI {
public:
    //The number of unused bits required to pad the length of the RPSI
    //      message to a multiple of 32 bits.
    uint8_t pb;

#if __BYTE_ORDER == __BIG_ENDIAN
    //0:  1 bit
    //      MUST be set to zero upon transmission and ignored upon reception.
    uint8_t zero : 1;
    //Payload Type: 7 bits
    //      Indicates the RTP payload type in the context of which the native
    //      RPSI bit string MUST be interpreted.
    uint8_t pt : 7;
#else
    uint8_t pt: 7;
    uint8_t zero: 1;
#endif

    // Native RPSI bit string: variable length
    //      The RPSI information as natively defined by the video codec.
    char bit_string[5];

    //Padding: #PB bits
    //      A number of bits set to zero to fill up the contents of the RPSI
    //      message to the next 32-bit boundary.  The number of padding bits
    //      MUST be indicated by the PB field.
    uint8_t padding;

    static size_t constexpr kSize = 8;
} PACKED;

//PSFB fmt = 4
//https://tools.ietf.org/html/rfc5104#section-4.3.1.1
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   | Seq nr.       |    Reserved                                   |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class FCI_FIR {
public:
    uint32_t ssrc;
    uint32_t seq_number: 8;
    uint32_t reserved: 24;

    static size_t constexpr kSize = 8;
    FCI_FIR(uint32_t ssrc, uint8_t seq_number, uint32_t reserved = 0);
    void net2Host();
    string dumpString() const;

} PACKED;

//PSFB fmt = 5
//https://tools.ietf.org/html/rfc5104#section-4.3.2.1
// 0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  Seq nr.      |  Reserved                           | Index   |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class FCI_TSTR {
public:
    static size_t constexpr kSize = 8;
} PACKED;

//PSFB fmt = 6
//https://tools.ietf.org/html/rfc5104#section-4.3.2.1
// 0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  Seq nr.      |  Reserved                           | Index   |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class FCI_TSTN : public FCI_TSTR{

} PACKED;

//PSFB fmt = 7
//https://tools.ietf.org/html/rfc5104#section-4.3.4.1
//0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   | Seq nr.       |0| Payload Type| Length                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                    VBCM Octet String....      |    Padding    |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class FCI_VBCM {
public:
    static size_t constexpr kSize = 12;
} PACKED;

//PSFB fmt = 15
//https://tools.ietf.org/html/draft-alvestrand-rmcat-remb-03
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  Unique identifier 'R' 'E' 'M' 'B'                            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  Num SSRC     | BR Exp    |  BR Mantissa                      |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   SSRC feedback                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ...                                                          |
// Num SSRC (8 bits):  Number of SSRCs in this message.
//
//   BR Exp (6 bits):   The exponential scaling of the mantissa for the
//               maximum total media bit rate value, ignoring all packet
//               overhead.  The value is an unsigned integer [0..63], as
//               in RFC 5104 section 4.2.2.1.
//
//   BR Mantissa (18 bits):   The mantissa of the maximum total media bit
//               rate (ignoring all packet overhead) that the sender of
//               the REMB estimates.  The BR is the estimate of the
//               traveled path for the SSRCs reported in this message.
//               The value is an unsigned integer in number of bits per
//               second.
//
//   SSRC feedback (32 bits)  Consists of one or more SSRC entries which
//               this feedback message applies to.
class FCI_REMB {
public:
    static size_t constexpr kSize = 8;

    static string create(const std::initializer_list<uint32_t> &ssrcs, uint32_t bitrate);
    void net2Host(size_t total_size);
    string dumpString() const;
    uint32_t getBitRate() const;
    vector<uint32_t *> getSSRC();

    //Unique identifier 'R' 'E' 'M' 'B'
    char magic[4];
    //Num SSRC (8 bits)/BR Exp (6 bits)/ BR Mantissa (18 bits)
    uint8_t bitrate[4];
    // SSRC feedback (32 bits)  Consists of one or more SSRC entries which
    //               this feedback message applies to.
    uint32_t ssrc_feedback[1];

} PACKED;

/////////////////////////////////////////// RTPFB ////////////////////////////////////////////////////

//RTPFB fmt = 1
//https://tools.ietf.org/html/rfc4585#section-6.2.1
// 0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |            PID                |             BLP               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class FCI_NACK {
public:
    // The PID field is used to specify a lost packet.  The PID field
    //      refers to the RTP sequence number of the lost packet.
    uint16_t pid;
    // bitmask of following lost packets (BLP): 16 bits
    uint16_t blp;
} PACKED;

//RTPFB fmt = 3
//https://tools.ietf.org/html/rfc5104#section-4.2.1.1
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   | MxTBR Exp |  MxTBR Mantissa                 |Measured Overhead|
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class FCI_TMMBR {
public:
    //SSRC (32 bits): The SSRC value of the media sender that is
    //              requested to obey the new maximum bit rate.
    uint32_t ssrc;

    //     MxTBR Exp (6 bits): The exponential scaling of the mantissa for the
    //              maximum total media bit rate value.  The value is an
    //              unsigned integer [0..63].
    uint32_t max_tbr_exp: 6;

    //     MxTBR Mantissa (17 bits): The mantissa of the maximum total media
    //              bit rate value as an unsigned integer.
    uint32_t max_mantissa: 17;

    //     Measured Overhead (9 bits): The measured average packet overhead
    //              value in bytes.  The measurement SHALL be done according
    //              to the description in section 4.2.1.2. The value is an
    //              unsigned integer [0..511].
    uint32_t measured_overhead: 9;
} PACKED;

//RTPFB fmt = 4
// https://tools.ietf.org/html/rfc5104#section-4.2.2.1
// 0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                              SSRC                             |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   | MxTBR Exp |  MxTBR Mantissa                 |Measured Overhead|
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class FCI_TMMBN : public FCI_TMMBR{
public:

} PACKED;

//RTPFB fmt = 15
//https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01#section-3.1
//https://zhuanlan.zhihu.com/p/206656654
//0                   1                   2                   3
//        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |      base sequence number     |      packet status count      |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |                 reference time                | fb pkt. count |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |          packet chunk         |         packet chunk          |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       .                                                               .
//       .                                                               .
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |         packet chunk          |  recv delta   |  recv delta   |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       .                                                               .
//       .                                                               .
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//       |           recv delta          |  recv delta   | zero padding  |
//       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
class FCI_TWCC{
public:

} PACKED;

} //namespace mediakit
#endif //ZLMEDIAKIT_RTCPFCI_H
