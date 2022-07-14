/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTCP_H
#define ZLMEDIAKIT_RTCP_H

#include "Common/macros.h"
#include "Network/Buffer.h"
#include "Util/util.h"
#include <stdint.h>
#include <vector>

namespace mediakit {

#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

// http://www.networksorcery.com/enp/protocol/rtcp.htm
#define RTCP_PT_MAP(XX)                                                                                                \
    XX(RTCP_FIR, 192)                                                                                                  \
    XX(RTCP_NACK, 193)                                                                                                 \
    XX(RTCP_SMPTETC, 194)                                                                                              \
    XX(RTCP_IJ, 195)                                                                                                   \
    XX(RTCP_SR, 200)                                                                                                   \
    XX(RTCP_RR, 201)                                                                                                   \
    XX(RTCP_SDES, 202)                                                                                                 \
    XX(RTCP_BYE, 203)                                                                                                  \
    XX(RTCP_APP, 204)                                                                                                  \
    XX(RTCP_RTPFB, 205)                                                                                                \
    XX(RTCP_PSFB, 206)                                                                                                 \
    XX(RTCP_XR, 207)                                                                                                   \
    XX(RTCP_AVB, 208)                                                                                                  \
    XX(RTCP_RSI, 209)                                                                                                  \
    XX(RTCP_TOKEN, 210)

// https://tools.ietf.org/html/rfc3550#section-6.5
#define SDES_TYPE_MAP(XX)                                                                                              \
    XX(RTCP_SDES_END, 0)                                                                                               \
    XX(RTCP_SDES_CNAME, 1)                                                                                             \
    XX(RTCP_SDES_NAME, 2)                                                                                              \
    XX(RTCP_SDES_EMAIL, 3)                                                                                             \
    XX(RTCP_SDES_PHONE, 4)                                                                                             \
    XX(RTCP_SDES_LOC, 5)                                                                                               \
    XX(RTCP_SDES_TOOL, 6)                                                                                              \
    XX(RTCP_SDES_NOTE, 7)                                                                                              \
    XX(RTCP_SDES_PRIVATE, 8)

// https://datatracker.ietf.org/doc/rfc4585/?include_text=1
// 6.3.  Payload-Specific Feedback Messages
//
//    Payload-Specific FB messages are identified by the value PT=PSFB as
//    RTCP message type.
//
//    Three payload-specific FB messages are defined so far plus an
//    application layer FB message.  They are identified by means of the
//    FMT parameter as follows:
//
//       0:     unassigned
//       1:     Picture Loss Indication (PLI)
//       2:     Slice Loss Indication (SLI)
//       3:     Reference Picture Selection Indication (RPSI)
//       4:     FIR https://tools.ietf.org/html/rfc5104#section-4.3.1.1
//       5:     TSTR https://tools.ietf.org/html/rfc5104#section-4.3.2.1
//       6:     TSTN https://tools.ietf.org/html/rfc5104#section-4.3.2.1
//       7:     VBCM https://tools.ietf.org/html/rfc5104#section-4.3.4.1
//       8-14:  unassigned
//       15:    REMB / Application layer FB (AFB) message, https://tools.ietf.org/html/draft-alvestrand-rmcat-remb-03
//       16-30: unassigned
//       31:    reserved for future expansion of the sequence number space
#define PSFB_TYPE_MAP(XX)                                                                                              \
    XX(RTCP_PSFB_PLI, 1)                                                                                               \
    XX(RTCP_PSFB_SLI, 2)                                                                                               \
    XX(RTCP_PSFB_RPSI, 3)                                                                                              \
    XX(RTCP_PSFB_FIR, 4)                                                                                               \
    XX(RTCP_PSFB_TSTR, 5)                                                                                              \
    XX(RTCP_PSFB_TSTN, 6)                                                                                              \
    XX(RTCP_PSFB_VBCM, 7)                                                                                              \
    XX(RTCP_PSFB_REMB, 15)

// https://tools.ietf.org/html/rfc4585#section-6.2
// 6.2.   Transport Layer Feedback Messages
//
//    Transport layer FB messages are identified by the value RTPFB as RTCP
//    message type.
//
//    A single general purpose transport layer FB message is defined in
//    this document: Generic NACK.  It is identified by means of the FMT
//    parameter as follows:
//
//    0:     unassigned
//    1:     Generic NACK
//    2:     reserved https://tools.ietf.org/html/rfc5104#section-4.2
//    3:     TMMBR https://tools.ietf.org/html/rfc5104#section-4.2.1.1
//    4:     TMMBN https://tools.ietf.org/html/rfc5104#section-4.2.2.1
//    5-14:  unassigned
//    15     transport-cc https://tools.ietf.org/html/draft-holmer-rmcat-transport-wide-cc-extensions-01
//    16-30: unassigned
//    31:    reserved for future expansion of the identifier number space
#define RTPFB_TYPE_MAP(XX)                                                                                             \
    XX(RTCP_RTPFB_NACK, 1)                                                                                             \
    XX(RTCP_RTPFB_TMMBR, 3)                                                                                            \
    XX(RTCP_RTPFB_TMMBN, 4)                                                                                            \
    XX(RTCP_RTPFB_TWCC, 15)

// rtcp类型枚举
enum class RtcpType : uint8_t {
#define XX(key, value) key = value,
    RTCP_PT_MAP(XX)
#undef XX
};

// sdes类型枚举
enum class SdesType : uint8_t {
#define XX(key, value) key = value,
    SDES_TYPE_MAP(XX)
#undef XX
};

// psfb类型枚举
enum class PSFBType : uint8_t {
#define XX(key, value) key = value,
    PSFB_TYPE_MAP(XX)
#undef XX
};

// rtpfb类型枚举
enum class RTPFBType : uint8_t {
#define XX(key, value) key = value,
    RTPFB_TYPE_MAP(XX)
#undef XX
};

/**
 * RtcpType转描述字符串
 */
const char *rtcpTypeToStr(RtcpType type);

/**
 * SdesType枚举转描述字符串
 */
const char *sdesTypeToStr(SdesType type);

/**
 * psfb枚举转描述字符串
 */
const char *psfbTypeToStr(PSFBType type);

/**
 * rtpfb枚举转描述字符串
 */
const char *rtpfbTypeToStr(RTPFBType type);

class RtcpHeader {
public:
#if __BYTE_ORDER == __BIG_ENDIAN
    // 版本号，固定为2
    uint32_t version : 2;
    // padding，固定为0
    uint32_t padding : 1;
    // reception report count
    uint32_t report_count : 5;
#else
    // reception report count
    uint32_t report_count : 5;
    // padding，末尾是否有追加填充
    uint32_t padding : 1;
    // 版本号，固定为2
    uint32_t version : 2;
#endif
    // rtcp类型,RtcpType
    uint32_t pt : 8;

private:
    // 长度
    uint32_t length : 16;

public:
    /**
     * 解析rtcp并转换网络字节序为主机字节序，返回RtcpHeader派生类列表
     * @param data 数据指针
     * @param size 数据总长度
     * @return rtcp对象列表，无需free
     */
    static std::vector<RtcpHeader *> loadFromBytes(char *data, size_t size);

    /**
     * rtcp包转Buffer对象
     * @param rtcp rtcp包对象智能指针
     * @return Buffer对象
     */
    static toolkit::Buffer::Ptr toBuffer(std::shared_ptr<RtcpHeader> rtcp);

    /**
     * 打印rtcp相关字段详情(调用派生类的dumpString函数)
     * 内部会判断是什么类型的派生类
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 根据length字段获取rtcp总长度
     */
    size_t getSize() const;

    /**
     * 后面追加padding数据长度
     */
    size_t getPaddingSize() const;

    /**
     * 设置rtcp length字段
     * @param size rtcp总长度，单位字节
     */
    void setSize(size_t size);

protected:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpHeader() const;

private:
    /**
     * 调用派生类的net2Host函数
     * @param size rtcp字符长度
     */
    void net2Host(size_t size);

} PACKED;

/////////////////////////////////////////////////////////////////////////////

// ReportBlock
class ReportItem {
public:
    friend class RtcpSR;
    friend class RtcpRR;

    uint32_t ssrc;
    // Fraction lost
    uint32_t fraction : 8;
    // Cumulative number of packets lost
    uint32_t cumulative : 24;
    // Sequence number cycles count
    uint16_t seq_cycles;
    // Highest sequence number received
    uint16_t seq_max;
    // Interarrival jitter
    uint32_t jitter;
    // Last SR timestamp, NTP timestamp,(ntpmsw & 0xFFFF) << 16  | (ntplsw >> 16) & 0xFFFF)
    uint32_t last_sr_stamp;
    // Delay since last SR timestamp,expressed in units of 1/65536 seconds
    uint32_t delay_since_last_sr;

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     */
    void net2Host();
} PACKED;

/*
 * 6.4.1 SR: Sender Report RTCP Packet

        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    RC   |   PT=SR=200   |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         SSRC of sender                        |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
sender |              NTP timestamp, most significant word             |
info   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |             NTP timestamp, least significant word             |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         RTP timestamp                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                     sender's packet count                     |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      sender's octet count                     |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
report |                 SSRC_1 (SSRC of first source)                 |
block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  1    | fraction lost |       cumulative number of packets lost       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |           extended highest sequence number received           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      interarrival jitter                      |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         last SR (LSR)                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                   delay since last SR (DLSR)                  |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
report |                 SSRC_2 (SSRC of second source)                |
block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  2    :                               ...                             :
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
       |                  profile-specific extensions                  |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */
// sender report
class RtcpSR : public RtcpHeader {
public:
    friend class RtcpHeader;
    uint32_t ssrc;
    // ntp timestamp MSW(in second)
    uint32_t ntpmsw;
    // ntp timestamp LSW(in picosecond)
    uint32_t ntplsw;
    // rtp timestamp
    uint32_t rtpts;
    // sender packet count
    uint32_t packet_count;
    // sender octet count
    uint32_t octet_count;
    // 可能有很多个
    ReportItem items;

public:
    /**
     * 创建SR包，只赋值了RtcpHeader部分(网络字节序)
     * @param item_count ReportItem对象个数
     * @return SR包
     */
    static std::shared_ptr<RtcpSR> create(size_t item_count);

    /**
     * 设置ntpmsw与ntplsw字段为网络字节序
     * @param tv 时间
     */
    void setNtpStamp(struct timeval tv);
    void setNtpStamp(uint64_t unix_stamp_ms);

    /**
     * 返回ntp时间的字符串
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string getNtpStamp() const;
    uint64_t getNtpUnixStampMS() const;

    /**
     * 获取ReportItem对象指针列表
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::vector<ReportItem *> getItemList();

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     * @param size 字节长度，防止内存越界
     */
    void net2Host(size_t size);
} PACKED;

/////////////////////////////////////////////////////////////////////////////

/*
 * 6.4.2 RR: Receiver Report RTCP Packet

        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    RC   |   PT=RR=201   |             length            |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                     SSRC of packet sender                     |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
report |                 SSRC_1 (SSRC of first source)                 |
block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  1    | fraction lost |       cumulative number of packets lost       |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |           extended highest sequence number received           |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                      interarrival jitter                      |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                         last SR (LSR)                         |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                   delay since last SR (DLSR)                  |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
report |                 SSRC_2 (SSRC of second source)                |
block  +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
  2    :                               ...                             :
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
       |                  profile-specific extensions                  |
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 */

// Receiver Report
class RtcpRR : public RtcpHeader {
public:
    friend class RtcpHeader;

    uint32_t ssrc;
    // 可能有很多个
    ReportItem items;

public:
    /**
     * 创建RR包，只赋值了RtcpHeader部分
     * @param item_count ReportItem对象个数
     * @return RR包
     */
    static std::shared_ptr<RtcpRR> create(size_t item_count);

    /**
     * 获取ReportItem对象指针列表
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::vector<ReportItem *> getItemList();

private:
    /**
     * 网络字节序转换为主机字节序
     * @param size 字节长度，防止内存越界
     */
    void net2Host(size_t size);

    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

} PACKED;

/////////////////////////////////////////////////////////////////////////////

/*
 *      6.5 SDES: Source Description RTCP Packet
        0                   1                   2                   3
        0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
       +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
header |V=2|P|    SC   |  PT=SDES=202  |             length            |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
chunk  |                          SSRC/CSRC_1                          |
  1    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                           SDES items                          |
       |                              ...                              |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
chunk  |                          SSRC/CSRC_2                          |
  2    +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
       |                           SDES items                          |
       |                              ...                              |
       +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 */

/*

SDES items 定义
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|   SdesType  |     length    | user and domain name        ...
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

// Source description Chunk
class SdesChunk {
public:
    friend class RtcpSdes;

    uint32_t ssrc;
    // SdesType
    uint8_t type;
    // text长度股，可以为0
    uint8_t txt_len;
    // 不定长
    char text[1];
    // 最后以RTCP_SDES_END结尾
    // 只字段为占位字段，不代表真实位置
    uint8_t end;

public:
    /**
     * 返回改对象字节长度
     */
    size_t totalBytes() const;

    /**
     * 本对象最少长度
     */
    static size_t minSize();

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     */
    void net2Host();
} PACKED;

// Source description
class RtcpSdes : public RtcpHeader {
public:
    friend class RtcpHeader;

    // 可能有很多个
    SdesChunk chunks;

public:
    /**
     * 创建SDES包，只赋值了RtcpHeader以及SdesChunk对象的length和text部分
     * @param item_text SdesChunk列表，只赋值length和text部分
     * @return SDES包
     */
    static std::shared_ptr<RtcpSdes> create(const std::vector<std::string> &item_text);

    /**
     * 获取SdesChunk对象指针列表
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::vector<SdesChunk *> getChunkList();

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     * @param size 字节长度，防止内存越界
     */
    void net2Host(size_t size);
} PACKED;

// https://tools.ietf.org/html/rfc4585#section-6.1
// 6.1.   Common Packet Format for Feedback Messages
//
//   All FB messages MUST use a common packet format that is depicted in
//   Figure 3:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|   FMT   |       PT      |          length               |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                  SSRC of packet sender                        |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                  SSRC of media source                         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   :            Feedback Control Information (FCI)                 :
//   :                                                               :
// rtcpfb和psfb的数据结构一致
class RtcpFB : public RtcpHeader {
public:
    friend class RtcpHeader;
    uint32_t ssrc;
    uint32_t ssrc_media;

public:
    /**
     * 创建psfb类型的反馈包
     */
    static std::shared_ptr<RtcpFB> create(PSFBType fmt, const void *fci = nullptr, size_t fci_len = 0);

    /**
     * 创建rtpfb类型的反馈包
     */
    static std::shared_ptr<RtcpFB> create(RTPFBType fmt, const void *fci = nullptr, size_t fci_len = 0);

    /**
     * fci转换成某对象指针
     * @tparam Type 对象类型
     * @return 对象指针
     */
    template <typename Type>
    const Type &getFci() const {
        auto fci_data = getFciPtr();
        auto fci_len = getFciSize();
        Type *fci = (Type *)fci_data;
        fci->check(fci_len);
        return *fci;
    }

    /**
     * 获取fci指针
     */
    const void *getFciPtr() const;

    /**
     * 获取fci数据长度
     */
    size_t getFciSize() const;

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     * @param size 字节长度，防止内存越界
     */
    void net2Host(size_t size);

private:
    static std::shared_ptr<RtcpFB> create_l(RtcpType type, int fmt, const void *fci, size_t fci_len);
} PACKED;

// BYE
/*
       0                   1                   2                   3
       0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |V=2|P|    SC   |   PT=BYE=203  |             length            |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      |                           SSRC/CSRC                           |
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
      :                              ...                              :
      +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
(opt) |     length    |               reason for leaving            ...
      +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
*/

class RtcpBye : public RtcpHeader {
public:
    friend class RtcpHeader;
    /* 变长，根据count决定有多少个ssrc */
    uint32_t ssrc[1];

    /** 中间可能有若干个 ssrc **/

    /* 可选 */
    uint8_t reason_len;
    char reason[1];

public:
    /**
     * 创建bye包
     * @param ssrc ssrc列表
     * @param reason 原因
     * @return rtcp bye包
     */
    static std::shared_ptr<RtcpBye> create(const std::vector<uint32_t> &ssrc, const std::string &reason);

    /**
     * 获取ssrc列表
     */
    std::vector<uint32_t *> getSSRC();

    /**
     * 获取原因
     */
    std::string getReason() const;

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     * @param size 字节长度，防止内存越界
     */
    void net2Host(size_t size);
} PACKED;

/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|V=2|P|reserved |   PT=XR=207   |             length            |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                              SSRC                             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
:                         report blocks                         :
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/
/*

    0                   1                   2                   3
    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |     BT=4      |   reserved    |       block length = 2        |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |              NTP timestamp, most significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
   |             NTP timestamp, least significant word             |
   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+

*/
class RtcpXRRRTR : public RtcpHeader {
public:
    friend class RtcpHeader;
    uint32_t ssrc;
    // 4
    uint8_t bt;
    uint8_t reserved;
    // 2
    uint16_t block_length;
    // ntp timestamp MSW(in second)
    uint32_t ntpmsw;
    // ntp timestamp LSW(in picosecond)
    uint32_t ntplsw;

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     * @param size 字节长度，防止内存越界
     */
    void net2Host(size_t size);

} PACKED;

/*

  0                   1                   2                   3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |     BT=5      |   reserved    |         block length          |
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 |                 SSRC_1 (SSRC of first receiver)               | sub-
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
 |                         last RR (LRR)                         |   1
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
 |                   delay since last RR (DLRR)                  |
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
 |                 SSRC_2 (SSRC of second receiver)              | sub-
 +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+ block
 :                               ...                             :   2
 +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
*/
class RtcpXRDLRRReportItem {
public:
    friend class RtcpXRDLRR;
    uint32_t ssrc;
    uint32_t lrr;
    uint32_t dlrr;

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     * @param size 字节长度，防止内存越界
     */
    void net2Host();
} PACKED;

class RtcpXRDLRR : public RtcpHeader {
public:
    friend class RtcpHeader;
    uint32_t ssrc;
    uint8_t bt;
    uint8_t reserved;
    uint16_t block_length;
    RtcpXRDLRRReportItem items;

    /**
     * 创建RtcpXRDLRR包，只赋值了RtcpHeader部分(网络字节序)
     * @param item_count RtcpXRDLRRReportItem对象个数
     * @return RtcpXRDLRR包
     */
    static std::shared_ptr<RtcpXRDLRR> create(size_t item_count);

    /**
     * 获取RtcpXRDLRRReportItem对象指针列表
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::vector<RtcpXRDLRRReportItem *> getItemList();

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    std::string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     * @param size 字节长度，防止内存越界
     */
    void net2Host(size_t size);

} PACKED;

#if defined(_WIN32)
#pragma pack(pop)
#endif // defined(_WIN32)

} // namespace mediakit
#endif // ZLMEDIAKIT_RTCP_H
