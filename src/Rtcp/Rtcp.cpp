/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <stddef.h>
#include "Rtcp.h"
#include "Util/logger.h"

namespace mediakit {

const char *rtcpTypeToStr(RtcpType type){
    switch (type){
#define SWITCH_CASE(key, value) case RtcpType::key :  return #value "(" #key ")";
        RTCP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return "unknown rtcp pt";
    }
}

const char *sdesTypeToStr(SdesType type){
    switch (type){
#define SWITCH_CASE(key, value) case SdesType::key :  return #value "(" #key ")";
        SDES_TYPE_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return "unknown source description type";
    }
}

static size_t alignSize(size_t bytes) {
    return (size_t)((bytes + 3) / 4) << 2;
}

static void setupHeader(RtcpHeader *rtcp, RtcpType type, size_t report_count, size_t total_bytes) {
    rtcp->version = 2;
    rtcp->padding = 0;
    if (report_count > 0x1F) {
        throw std::invalid_argument(StrPrinter << "rtcp report_count最大赋值为31,当前为:" << report_count);
    }
    //items总个数
    rtcp->report_count = report_count;
    rtcp->pt = (uint8_t) type;
    //不包含rtcp头的长度
    rtcp->length = htons((uint16_t)((total_bytes / 4) - 1));
}

/////////////////////////////////////////////////////////////////////////////

void RtcpHeader::net2Host() {
    length = ntohs(length);
}

string RtcpHeader::dumpHeader() const{
    _StrPrinter printer;
    printer << "version:" << version << "\r\n";
    printer << "padding:" << padding << "\r\n";
    printer << "report_count:" << report_count << "\r\n";
    printer << "pt:" << rtcpTypeToStr((RtcpType)pt) << "\r\n";
    printer << "length:" << length << "\r\n";
    printer << "--------\r\n";
    return std::move(printer);
}

string RtcpHeader::dumpString() const {
    switch ((RtcpType)pt) {
        case RtcpType::RTCP_SR: {
            RtcpSR *rtcp = (RtcpSR *)this;
            return rtcp->dumpString();
        }

        case RtcpType::RTCP_RR: {
            RtcpRR *rtcp = (RtcpRR *)this;
            return rtcp->dumpString();
        }

        case RtcpType::RTCP_SDES: {
            RtcpSdes *rtcp = (RtcpSdes *)this;
            return rtcp->dumpString();
        }
        default: return StrPrinter << dumpHeader() << hexdump((char *)this + sizeof(*this), length << 2);
    }
}

void RtcpHeader::net2Host(size_t len){
    switch ((RtcpType)pt) {
        case RtcpType::RTCP_SR: {
            RtcpSR *sr = (RtcpSR *)this;
            sr->net2Host(len);
            break;
        }

        case RtcpType::RTCP_RR: {
            RtcpRR *rr = (RtcpRR *)this;
            rr->net2Host(len);
            break;
        }

        case RtcpType::RTCP_SDES: {
            RtcpSdes *sdes = (RtcpSdes *)this;
            sdes->net2Host(len);
            break;
        }
        default: throw std::runtime_error(StrPrinter << "未处理的rtcp包:" << rtcpTypeToStr((RtcpType) this->pt));
    }
}

vector<RtcpHeader *> RtcpHeader::loadFromBytes(char *data, size_t len){
    vector<RtcpHeader *> ret;
    ssize_t remain = len;
    char *ptr = data;
    while (remain > (ssize_t) sizeof(RtcpHeader)) {
        RtcpHeader *rtcp = (RtcpHeader *) ptr;
        auto rtcp_len = (1 + ntohs(rtcp->length)) << 2;
        try {
            rtcp->net2Host(rtcp_len);
            ret.emplace_back(rtcp);
        } catch (std::exception &ex) {
            //不能处理的rtcp包，或者无法解析的rtcp包，忽略掉
            WarnL << ex.what();
        }
        ptr += rtcp_len;
        remain -= rtcp_len;
    }
    return ret;
}

class BufferRtcp : public Buffer {
public:
    BufferRtcp(std::shared_ptr<RtcpHeader> rtcp) {
        _rtcp = std::move(rtcp);
        _size = (htons(_rtcp->length) + 1) << 2;
    }

    ~BufferRtcp() override {}

    char *data() const override {
        return (char *) _rtcp.get();
    }

    size_t size() const override {
        return _size;
    }

private:
    std::size_t _size;
    std::shared_ptr<RtcpHeader> _rtcp;
};

Buffer::Ptr RtcpHeader::toBuffer(std::shared_ptr<RtcpHeader> rtcp) {
    return std::make_shared<BufferRtcp>(std::move(rtcp));
}

/////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RtcpSR> RtcpSR::create(size_t item_count) {
    auto bytes = alignSize(sizeof(RtcpSR) - sizeof(ReportItem) + item_count * sizeof(ReportItem));
    auto ptr = (RtcpSR *) new char[bytes];
    setupHeader(ptr, RtcpType::RTCP_SR, item_count, bytes);
    return std::shared_ptr<RtcpSR>(ptr, [](RtcpSR *ptr) {
        delete[] (char *) ptr;
    });
}

string RtcpSR::getNtpStamp() const{
    struct timeval tv;
    tv.tv_sec = ntpmsw - 0x83AA7E80;
    tv.tv_usec = (decltype(tv.tv_usec))(ntplsw / ((double) (((uint64_t) 1) << 32) * 1.0e-6));
    return LogChannel::printTime(tv);
}

void RtcpSR::setNtpStamp(struct timeval tv) {
    ntpmsw = htonl(tv.tv_sec + 0x83AA7E80); /* 0x83AA7E80 is the number of seconds from 1900 to 1970 */
    ntplsw = htonl((uint32_t) ((double) tv.tv_usec * (double) (((uint64_t) 1) << 32) * 1.0e-6));
}

string RtcpSR::dumpString() const{
    _StrPrinter printer;
    printer << RtcpHeader::dumpHeader();
    printer << "ssrc:" << ssrc << "\r\n";
    printer << "ntpmsw:" << ntpmsw << "\r\n";
    printer << "ntplsw:" << ntplsw << "\r\n";
    printer << "ntp time:" << getNtpStamp() << "\r\n";
    printer << "rtpts:" << rtpts << "\r\n";
    printer << "packet_count:" << packet_count << "\r\n";
    printer << "octet_count:" << octet_count << "\r\n";
    auto items = ((RtcpSR *)this)->getItemList();
    auto i = 0;
    for (auto &item : items) {
        printer << "---- item:" << i++ << " ----\r\n";
        printer << item->dumpString();
    }
    return std::move(printer);
}

#define CHECK_MIN_SIZE(size, kMinSize) \
if (size < kMinSize) { \
    throw std::out_of_range(StrPrinter << rtcpTypeToStr((RtcpType)pt) << " 长度不足:" << size << " < " << kMinSize); \
}

#define CHECK_LENGTH(size, item_count) \
/*修正个数，防止getItemList时内存越界*/ \
if (report_count != item_count) { \
    WarnL << rtcpTypeToStr((RtcpType)pt) << " report_count 字段不正确,已修正为:" << (int)report_count << " -> " << item_count; \
    report_count = item_count; \
} \
if ((size_t) (length + 1) << 2 != size) { \
    WarnL << rtcpTypeToStr((RtcpType)pt) << " length字段不正确:" << (size_t) (length + 1) << 2 << " != " << size; \
}

void RtcpSR::net2Host(size_t size) {
    static const size_t kMinSize = sizeof(RtcpSR) - sizeof(items);
    CHECK_MIN_SIZE(size, kMinSize);

    RtcpHeader::net2Host();
    ssrc = ntohl(ssrc);
    ntpmsw = ntohl(ntpmsw);
    ntplsw = ntohl(ntplsw);
    rtpts = ntohl(rtpts);
    packet_count = ntohl(packet_count);
    octet_count = ntohl(octet_count);

    ReportItem *ptr = &items;
    int item_count = 0;
    for(int i = 0; i < (int)report_count && (char *)(ptr) + sizeof(ReportItem) <= (char *)(this) + size; ++i){
        ptr->net2Host();
        ++ptr;
        ++item_count;
    }
    CHECK_LENGTH(size, item_count);
}

vector<ReportItem*> RtcpSR::getItemList(){
    vector<ReportItem *> ret;
    ReportItem *ptr = &items;
    for (int i = 0; i < (int) report_count; ++i) {
        ret.emplace_back(ptr);
        ++ptr;
    }
    return ret;
}

/////////////////////////////////////////////////////////////////////////////

string ReportItem::dumpString() const{
    _StrPrinter printer;
    printer << "ssrc:" << ssrc << "\r\n";
    printer << "fraction:" << fraction << "\r\n";
    printer << "cumulative:" << cumulative << "\r\n";
    printer << "seq_cycles:" << seq_cycles << "\r\n";
    printer << "seq_max:" << seq_max << "\r\n";
    printer << "jitter:" << jitter << "\r\n";
    printer << "last_sr_stamp:" << last_sr_stamp << "\r\n";
    printer << "delay_since_last_sr:" << delay_since_last_sr << "\r\n";
    return std::move(printer);
}

void ReportItem::net2Host() {
    ssrc = ntohl(ssrc);
    cumulative = ntohl(cumulative ) >> 8;
    seq_cycles = ntohs(seq_cycles);
    seq_max = ntohs(seq_max);
    jitter = ntohl(jitter);
    last_sr_stamp = ntohl(last_sr_stamp);
    delay_since_last_sr = ntohl(delay_since_last_sr);
}

/////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RtcpRR> RtcpRR::create(size_t item_count) {
    auto bytes = alignSize(sizeof(RtcpRR) - sizeof(ReportItem) + item_count * sizeof(ReportItem));
    auto ptr = (RtcpRR *) new char[bytes];
    setupHeader(ptr, RtcpType::RTCP_RR, item_count, bytes);
    return std::shared_ptr<RtcpRR>(ptr, [](RtcpRR *ptr) {
        delete[] (char *) ptr;
    });
}

string RtcpRR::dumpString() const{
    _StrPrinter printer;
    printer << RtcpHeader::dumpHeader();
    printer << "ssrc:" << ssrc << "\r\n";
    auto items = ((RtcpRR *)this)->getItemList();
    auto i = 0;
    for (auto &item : items) {
        printer << "---- item:" << i++ << " ----\r\n";
        printer << item->dumpString();
    }
    return std::move(printer);
}

void RtcpRR::net2Host(size_t size) {
    static const size_t kMinSize = sizeof(RtcpRR) - sizeof(items);
    CHECK_MIN_SIZE(size, kMinSize);
    RtcpHeader::net2Host();
    ssrc = ntohl(ssrc);

    ReportItem *ptr = &items;
    int item_count = 0;
    for(int i = 0; i < (int)report_count && (char *)(ptr) + sizeof(ReportItem) <= (char *)(this) + size; ++i){
        ptr->net2Host();
        ++ptr;
        ++item_count;
    }
    CHECK_LENGTH(size, item_count);
}

vector<ReportItem*> RtcpRR::getItemList() {
    vector<ReportItem *> ret;
    ReportItem *ptr = &items;
    for (int i = 0; i < (int) report_count; ++i) {
        ret.emplace_back(ptr);
        ++ptr;
    }
    return ret;
}

/////////////////////////////////////////////////////////////////////////////

void SdesItem::net2Host() {
    ssrc = ntohl(ssrc);
}

size_t SdesItem::totalBytes() const{
    return alignSize(minSize() + length);
}

size_t SdesItem::minSize() {
    return sizeof(SdesItem) - sizeof(text);
}

string SdesItem::dumpString() const{
    _StrPrinter printer;
    printer << "ssrc:" << ssrc << "\r\n";
    printer << "type:" << sdesTypeToStr((SdesType) type) << "\r\n";
    printer << "length:" << (int) length << "\r\n";
    printer << "text:" << (length ? string(&text, length) : "") << "\r\n";
    return std::move(printer);
}

/////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RtcpSdes> RtcpSdes::create(const std::initializer_list<string> &item_text) {
    size_t item_total_size = 0;
    for (auto &text : item_text) {
        //统计所有SdesItem对象占用的空间
        item_total_size += alignSize(SdesItem::minSize() + (0xFF & text.size()));
    }
    auto bytes = alignSize(sizeof(RtcpSdes) - sizeof(SdesItem) + item_total_size);
    auto ptr = (RtcpSdes *) new char[bytes];
    auto item_ptr = &ptr->items;
    for (auto &text : item_text) {
        item_ptr->length = (0xFF & text.size());
        //确保赋值\0为RTCP_SDES_END
        memcpy(&(item_ptr->text), text.data(), item_ptr->length + 1);
        item_ptr = (SdesItem *) ((char *) item_ptr + item_ptr->totalBytes());
    }

    setupHeader(ptr, RtcpType::RTCP_SDES, item_text.size(), bytes);
    return std::shared_ptr<RtcpSdes>(ptr, [](RtcpSdes *ptr) {
        delete [] (char *) ptr;
    });
}

string RtcpSdes::dumpString() const {
    _StrPrinter printer;
    printer << RtcpHeader::dumpHeader();
    auto items = ((RtcpSdes *)this)->getItemList();
    auto i = 0;
    for (auto &item : items) {
        printer << "---- item:" << i++ << " ----\r\n";
        printer << item->dumpString();
    }
    return std::move(printer);
}

void RtcpSdes::net2Host(size_t size) {
    static const size_t kMinSize = sizeof(RtcpSdes) - sizeof(items);
    CHECK_MIN_SIZE(size, kMinSize);
    RtcpHeader::net2Host();
    SdesItem *ptr = &items;
    int item_count = 0;
    for(int i = 0; i < (int)report_count && (char *)(ptr) + SdesItem::minSize() <= (char *)(this) + size; ++i){
        ptr->net2Host();
        ptr = (SdesItem *) ((char *) ptr + ptr->totalBytes());
        ++item_count;
    }
    CHECK_LENGTH(size, item_count);
}

vector<SdesItem *> RtcpSdes::getItemList() {
    vector<SdesItem *> ret;
    SdesItem *ptr = &items;
    for (int i = 0; i < (int) report_count; ++i) {
        ret.emplace_back(ptr);
        ptr = (SdesItem *) ((char *) ptr + ptr->totalBytes());
    }
    return ret;
}

}//namespace mediakit