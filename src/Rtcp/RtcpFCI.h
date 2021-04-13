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

//https://tools.ietf.org/html/rfc4585

namespace mediakit {

//https://tools.ietf.org/html/rfc4585#section-6.2.1
// 0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |            PID                |             BLP               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//               Figure 4: Syntax for the Generic NACK message

class FCI_NACK {
public:
    // The PID field is used to specify a lost packet.  The PID field
    //      refers to the RTP sequence number of the lost packet.
    uint16_t pid;
    // bitmask of following lost packets (BLP): 16 bits
    uint16_t blp;
} PACKED;

//https://tools.ietf.org/html/rfc4585#section-6.3.2.2
// The Slice Loss Indication uses one additional FCI field, the content
//   of which is depicted in Figure 6.  The length of the FB message MUST
//   be set to 2+n, with n being the number of SLIs contained in the FCI
//   field.
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |            First        |        Number           | PictureID |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//            Figure 6: Syntax of the Slice Loss Indication (SLI)
//
class FCI_SLI {
public:
    uint32_t first : 13;
    uint32_t number : 13;
    uint32_t pic_id  : 6;
} PACKED;

//https://tools.ietf.org/html/rfc4585#section-6.3.3.2
//6.3.3.2.  Format
//
//   The FCI for the RPSI message follows the format depicted in Figure 7:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |      PB       |0| Payload Type|    Native RPSI bit string     |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   defined per codec          ...                | Padding (0) |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//
//   Figure 7: Syntax of the Reference Picture Selection Indication (RPSI)
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
    char str[1];

    //Padding: #PB bits
    //      A number of bits set to zero to fill up the contents of the RPSI
    //      message to the next 32-bit boundary.  The number of padding bits
    //      MUST be indicated by the PB field.
} PACKED;

//tools.ietf.org/html/draft-alvestrand-rmcat-remb-03
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P| FMT=15  |   PT=206      |             length            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                  SSRC of packet sender                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                  SSRC of media source                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  Unique identifier 'R' 'E' 'M' 'B'                            |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  Num SSRC     | BR Exp    |  BR Mantissa                      |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |   SSRC feedback                                               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |  ...                                                          |
class FCI_REMB {
public:
    //Unique identifier 'R' 'E' 'M' 'B'
    char magic[4];

    //Number of SSRCs in this message.
    uint32_t num_ssrc : 8;

    // BR Exp (6 bits):   The exponential scaling of the mantissa for the
    //               maximum total media bit rate value, ignoring all packet
    //               overhead.  The value is an unsigned integer [0..63], as
    //               in RFC 5104 section 4.2.2.1.
    uint32_t br_exp : 6;

    //BR Mantissa (18 bits):   The mantissa of the maximum total media bit
    //               rate (ignoring all packet overhead) that the sender of
    //               the REMB estimates.  The BR is the estimate of the
    //               traveled path for the SSRCs reported in this message.
    //               The value is an unsigned integer in number of bits per
    //               second.
    uint32_t br_mantissa : 18;

    // SSRC feedback (32 bits)  Consists of one or more SSRC entries which
    //               this feedback message applies to.
    uint32_t ssrc_feedback;

} PACKED;

} //namespace mediakit
#endif //ZLMEDIAKIT_RTCPFCI_H
