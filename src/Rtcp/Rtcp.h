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

#include <stdint.h>
#include <vector>
#include "Util/util.h"
#include "Network/Buffer.h"
#include "Common/macros.h"
using namespace std;
using namespace toolkit;

namespace mediakit {

#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

//https://datatracker.ietf.org/doc/rfc3550

#define RTCP_PT_MAP(XX) \
        XX(RTCP_FIR, 192) \
        XX(RTCP_NACK, 193) \
        XX(RTCP_SMPTETC, 194) \
        XX(RTCP_IJ, 195) \
        XX(RTCP_SR, 200) \
        XX(RTCP_RR, 201) \
        XX(RTCP_SDES, 202) \
        XX(RTCP_BYE, 203) \
        XX(RTCP_APP, 204) \
        XX(RTCP_RTPFB, 205) \
        XX(RTCP_PSFB, 206) \
        XX(RTCP_XR, 207) \
        XX(RTCP_AVB, 208) \
        XX(RTCP_RSI, 209) \
        XX(RTCP_TOKEN, 210)
                        
#define SDES_TYPE_MAP(XX) \
        XX(RTCP_SDES_END, 0) \
        XX(RTCP_SDES_CNAME, 1) \
        XX(RTCP_SDES_NAME, 2) \
        XX(RTCP_SDES_EMAIL, 3) \
        XX(RTCP_SDES_PHONE, 4) \
        XX(RTCP_SDES_LOC, 5) \
        XX(RTCP_SDES_TOOL, 6) \
        XX(RTCP_SDES_NOTE, 7) \
        XX(RTCP_SDES_PRIVATE, 8)

//rtcp类型枚举
enum class RtcpType : uint8_t {
#define XX(key, value) key = value,
    RTCP_PT_MAP(XX)
#undef XX
};

//sdes类型枚举
enum class SdesType : uint8_t {
#define XX(key, value) key = value,
    SDES_TYPE_MAP(XX)
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

class RtcpHeader {
public:
#if __BYTE_ORDER == __BIG_ENDIAN
    //版本号，固定为2
    uint32_t version: 2;
    //padding，固定为0
    uint32_t padding: 1;
    //reception report count
    uint32_t report_count: 5;
#else
    //reception report count
    uint32_t report_count: 5;
    //padding，固定为0
    uint32_t padding: 1;
    //版本号，固定为2
    uint32_t version: 2;
#endif
    //rtcp类型,RtcpType
    uint32_t pt: 8;
    //长度
    uint32_t length: 16;

public:
    /**
     * 解析rtcp并转换网络字节序为主机字节序，返回RtcpHeader派生类列表
     * @param data 数据指针
     * @param size 数据总长度
     * @return rtcp对象列表，无需free
     */
    static vector<RtcpHeader *> loadFromBytes(char *data, size_t size);

    /**
     * rtcp包转Buffer对象
     * @param rtcp rtcp包对象智能指针
     * @return Buffer对象
     */
    static Buffer::Ptr toBuffer(std::shared_ptr<RtcpHeader> rtcp);

    /**
     * 打印rtcp相关字段详情(调用派生类的dumpString函数)
     * 内部会判断是什么类型的派生类
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    string dumpString() const;

protected:
    /**
     * 网络字节序转换为主机字节序
     */
    void net2Host();

    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    string dumpHeader() const;

private:
    /**
     * 调用派生类的net2Host函数
     * @param size rtcp字符长度
     */
    void net2Host(size_t size);

} PACKED;

/////////////////////////////////////////////////////////////////////////////

//ReportBlock
class ReportItem {
public:
    friend class RtcpSR;
    friend class RtcpRR;

    uint32_t ssrc;
    //Fraction lost
    uint32_t fraction: 8;
    //Cumulative number of packets lost
    uint32_t cumulative: 24;
    //Sequence number cycles count
    uint16_t seq_cycles;
    //Highest sequence number received
    uint16_t seq_max;
    //Interarrival jitter
    uint32_t jitter;
    //Last SR timestamp, NTP timestamp,(ntpmsw & 0xFFFF) << 16  | (ntplsw >> 16) & 0xFFFF)
    uint32_t last_sr_stamp;
    //Delay since last SR timestamp,expressed in units of 1/65536 seconds
    uint32_t delay_since_last_sr;

private:
    /**
     * 打印字段详情
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     */
    void net2Host();
}PACKED;

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
    //可能有很多个
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

    /**
     * 返回ntp时间的字符串
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    string getNtpStamp() const;

    /**
     * 获取ReportItem对象指针列表
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    vector<ReportItem*> getItemList();

private:
    /**
    * 打印字段详情
    * 使用net2Host转换成主机字节序后才可使用此函数
    */
    string dumpString() const;

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

//Receiver Report
class RtcpRR : public RtcpHeader {
public:
    friend class RtcpHeader;

    uint32_t ssrc;
    //可能有很多个
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
    vector<ReportItem*> getItemList();

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
    string dumpString() const;

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

//Source description item
class SdesItem {
public:
    friend class RtcpSdes;

    uint32_t ssrc;
    //SdesType
    uint8_t type;
    //text长度股，可以为0
    uint8_t length;
    //不定长
    char text;
    //最后以RTCP_SDES_END结尾
    //只字段为占位字段，不代表真实位置
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
    string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
    */
    void net2Host();
} PACKED;

//Source description
class RtcpSdes : public RtcpHeader {
public:
    friend class RtcpHeader;

    //可能有很多个
    SdesItem items;

public:
    /**
     * 创建SDES包，只赋值了RtcpHeader以及SdesItem对象的length和text部分
     * @param item_text SdesItem列表，只赋值length和text部分
     * @return SDES包
     */
    static std::shared_ptr<RtcpSdes> create(const std::initializer_list<string> &item_text);

    /**
     * 获取SdesItem对象指针列表
     * 使用net2Host转换成主机字节序后才可使用此函数
     */
    vector<SdesItem*> getItemList();

private:
    /**
    * 打印字段详情
    * 使用net2Host转换成主机字节序后才可使用此函数
    */
    string dumpString() const;

    /**
     * 网络字节序转换为主机字节序
     * @param size 字节长度，防止内存越界
     */
    void net2Host(size_t size);
} PACKED;

#if defined(_WIN32)
#pragma pack(pop)
#endif // defined(_WIN32)

} //namespace mediakit
#endif //ZLMEDIAKIT_RTCP_H
