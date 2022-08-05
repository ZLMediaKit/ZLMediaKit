/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Rtcp.h"
#include "RtcpFCI.h"
#include "Util/logger.h"
#include <assert.h>
#include <stddef.h>

using namespace std;
using namespace toolkit;

namespace mediakit {

const char *rtcpTypeToStr(RtcpType type) {
    switch (type) {
#define SWITCH_CASE(key, value)                                                                                        \
    case RtcpType::key: return #value "(" #key ")";
        RTCP_PT_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return "unknown rtcp pt";
    }
}

const char *sdesTypeToStr(SdesType type) {
    switch (type) {
#define SWITCH_CASE(key, value)                                                                                        \
    case SdesType::key: return #value "(" #key ")";
        SDES_TYPE_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return "unknown source description type";
    }
}

const char *psfbTypeToStr(PSFBType type) {
    switch (type) {
#define SWITCH_CASE(key, value)                                                                                        \
    case PSFBType::key: return #value "(" #key ")";
        PSFB_TYPE_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return "unknown payload-specific fb message fmt type";
    }
}

const char *rtpfbTypeToStr(RTPFBType type) {
    switch (type) {
#define SWITCH_CASE(key, value)                                                                                        \
    case RTPFBType::key: return #value "(" #key ")";
        RTPFB_TYPE_MAP(SWITCH_CASE)
#undef SWITCH_CASE
        default: return "unknown transport layer feedback messages fmt type";
    }
}

static size_t alignSize(size_t bytes) {
    return (size_t)((bytes + 3) >> 2) << 2;
}

static void setupHeader(RtcpHeader *rtcp, RtcpType type, size_t report_count, size_t total_bytes) {
    rtcp->version = 2;
    rtcp->padding = 0;
    if (report_count > 0x1F) {
        throw std::invalid_argument(StrPrinter << "rtcp report_count最大赋值为31,当前为:" << report_count);
    }
    // items总个数
    rtcp->report_count = report_count;
    rtcp->pt = (uint8_t)type;
    rtcp->setSize(total_bytes);
}

static void setupPadding(RtcpHeader *rtcp, size_t padding_size) {
    if (padding_size) {
        rtcp->padding = 1;
        ((uint8_t *)rtcp)[rtcp->getSize() - 1] = padding_size & 0xFF;
    } else {
        rtcp->padding = 0;
    }
}

/////////////////////////////////////////////////////////////////////////////

string RtcpHeader::dumpHeader() const {
    _StrPrinter printer;
    printer << "version:" << version << "\r\n";
    if (padding) {
        printer << "padding:" << padding << " " << getPaddingSize() << "\r\n";
    } else {
        printer << "padding:" << padding << "\r\n";
    }

    switch ((RtcpType)pt) {
        case RtcpType::RTCP_RTPFB: {
            printer << "report_count:" << rtpfbTypeToStr((RTPFBType)report_count) << "\r\n";
            break;
        }
        case RtcpType::RTCP_PSFB: {
            printer << "report_count:" << psfbTypeToStr((PSFBType)report_count) << "\r\n";
            break;
        }
        default: {
            printer << "report_count:" << report_count << "\r\n";
            break;
        }
    }

    printer << "pt:" << rtcpTypeToStr((RtcpType)pt) << "\r\n";
    printer << "size:" << getSize() << "\r\n";
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

        case RtcpType::RTCP_RTPFB:
        case RtcpType::RTCP_PSFB: {
            RtcpFB *rtcp = (RtcpFB *)this;
            return rtcp->dumpString();
        }

        case RtcpType::RTCP_BYE: {
            RtcpBye *rtcp = (RtcpBye *)this;
            return rtcp->dumpString();
        }

        default: return StrPrinter << dumpHeader() << hexdump((char *)this + sizeof(*this), getSize() - sizeof(*this));
    }
}

size_t RtcpHeader::getSize() const {
    // 加上rtcp头长度
    return (1 + ntohs(length)) << 2;
}

size_t RtcpHeader::getPaddingSize() const {
    if (!padding) {
        return 0;
    }
    return ((uint8_t *)this)[getSize() - 1];
}

void RtcpHeader::setSize(size_t size) {
    // 不包含rtcp头的长度
    length = htons((uint16_t)((size >> 2) - 1));
}

void RtcpHeader::net2Host(size_t len) {
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

        case RtcpType::RTCP_RTPFB:
        case RtcpType::RTCP_PSFB: {
            RtcpFB *fb = (RtcpFB *)this;
            fb->net2Host(len);
            break;
        }

        case RtcpType::RTCP_BYE: {
            RtcpBye *bye = (RtcpBye *)this;
            bye->net2Host(len);
            break;
        }
        case RtcpType::RTCP_XR: {
            RtcpXRRRTR *xr = (RtcpXRRRTR *)this;
            if (xr->bt == 4) {
                xr->net2Host(len);
                // TraceL<<xr->dumpString();
            } else if (xr->bt == 5) {
                RtcpXRDLRR *dlrr = (RtcpXRDLRR *)this;
                dlrr->net2Host(len);
                TraceL << dlrr->dumpString();
            } else {
                throw std::runtime_error(StrPrinter << "rtcp xr bt " << xr->bt << " not support");
            }
            break;
        }
        default: throw std::runtime_error(StrPrinter << "未处理的rtcp包:" << rtcpTypeToStr((RtcpType)this->pt));
    }
}

vector<RtcpHeader *> RtcpHeader::loadFromBytes(char *data, size_t len) {
    vector<RtcpHeader *> ret;
    ssize_t remain = len;
    char *ptr = data;
    while (remain > (ssize_t)sizeof(RtcpHeader)) {
        RtcpHeader *rtcp = (RtcpHeader *)ptr;
        auto rtcp_len = rtcp->getSize();
        if (remain < (ssize_t)rtcp_len) {
            WarnL << "非法的rtcp包,声明的长度超过实际数据长度";
            break;
        }
        try {
            rtcp->net2Host(rtcp_len);
            ret.emplace_back(rtcp);
        } catch (std::exception &ex) {
            // 不能处理的rtcp包，或者无法解析的rtcp包，忽略掉
            WarnL << ex.what() << ",长度为:" << rtcp_len;
        }
        ptr += rtcp_len;
        remain -= rtcp_len;
    }
    return ret;
}

class BufferRtcp : public Buffer {
public:
    BufferRtcp(std::shared_ptr<RtcpHeader> rtcp) { _rtcp = std::move(rtcp); }

    ~BufferRtcp() override {}

    char *data() const override { return (char *)_rtcp.get(); }

    size_t size() const override { return _rtcp->getSize(); }

private:
    std::shared_ptr<RtcpHeader> _rtcp;
};

Buffer::Ptr RtcpHeader::toBuffer(std::shared_ptr<RtcpHeader> rtcp) {
    return std::make_shared<BufferRtcp>(std::move(rtcp));
}

/////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RtcpSR> RtcpSR::create(size_t item_count) {
    auto real_size = sizeof(RtcpSR) - sizeof(ReportItem) + item_count * sizeof(ReportItem);
    auto bytes = alignSize(real_size);
    auto ptr = (RtcpSR *)new char[bytes];
    setupHeader(ptr, RtcpType::RTCP_SR, item_count, bytes);
    setupPadding(ptr, bytes - real_size);
    return std::shared_ptr<RtcpSR>(ptr, [](RtcpSR *ptr) { delete[](char *) ptr; });
}

string RtcpSR::getNtpStamp() const {
    struct timeval tv;
    tv.tv_sec = ntpmsw - 0x83AA7E80;
    tv.tv_usec = (decltype(tv.tv_usec))(ntplsw / ((double)(((uint64_t)1) << 32) * 1.0e-6));
    return LogChannel::printTime(tv);
}

uint64_t RtcpSR::getNtpUnixStampMS() const {
    if (ntpmsw < 0x83AA7E80) {
        // ntp时间戳起始时间为1900年，但是utc时间戳起始时间为1970年，两者相差0x83AA7E80秒
        // ntp时间戳不得早于1970年，否则无法转换为utc时间戳
        return 0;
    }
    struct timeval tv;
    tv.tv_sec = ntpmsw - 0x83AA7E80;
    tv.tv_usec = (decltype(tv.tv_usec))(ntplsw / ((double)(((uint64_t)1) << 32) * 1.0e-6));
    return 1000 * tv.tv_sec + tv.tv_usec / 1000;
}

void RtcpSR::setNtpStamp(struct timeval tv) {
    ntpmsw = htonl(tv.tv_sec + 0x83AA7E80); /* 0x83AA7E80 is the number of seconds from 1900 to 1970 */
    ntplsw = htonl((uint32_t)((double)tv.tv_usec * (double)(((uint64_t)1) << 32) * 1.0e-6));
}

void RtcpSR::setNtpStamp(uint64_t unix_stamp_ms) {
    struct timeval tv;
    tv.tv_sec = unix_stamp_ms / 1000;
    tv.tv_usec = (unix_stamp_ms % 1000) * 1000;
    setNtpStamp(tv);
}

string RtcpSR::dumpString() const {
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

#define CHECK_MIN_SIZE(size, kMinSize)                                                                                 \
    if (size < kMinSize) {                                                                                             \
        throw std::out_of_range(                                                                                       \
            StrPrinter << rtcpTypeToStr((RtcpType)pt) << " 长度不足:" << size << " < " << kMinSize);                   \
    }

#define CHECK_REPORT_COUNT(item_count)                                                                                 \
    /*修正个数，防止getItemList时内存越界*/                                                                \
    if (report_count != item_count) {                                                                                  \
        WarnL << rtcpTypeToStr((RtcpType)pt) << " report_count 字段不正确,已修正为:" << (int)report_count << " -> "    \
              << item_count;                                                                                           \
        report_count = item_count;                                                                                     \
    }

void RtcpSR::net2Host(size_t size) {
    static const size_t kMinSize = sizeof(RtcpSR) - sizeof(items);
    CHECK_MIN_SIZE(size, kMinSize);

    ssrc = ntohl(ssrc);
    ntpmsw = ntohl(ntpmsw);
    ntplsw = ntohl(ntplsw);
    rtpts = ntohl(rtpts);
    packet_count = ntohl(packet_count);
    octet_count = ntohl(octet_count);

    ReportItem *ptr = &items;
    int item_count = 0;
    for (int i = 0; i < (int)report_count && (char *)(ptr) + sizeof(ReportItem) <= (char *)(this) + size; ++i) {
        ptr->net2Host();
        ++ptr;
        ++item_count;
    }
    CHECK_REPORT_COUNT(item_count);
}

vector<ReportItem *> RtcpSR::getItemList() {
    vector<ReportItem *> ret;
    ReportItem *ptr = &items;
    for (int i = 0; i < (int)report_count; ++i) {
        ret.emplace_back(ptr);
        ++ptr;
    }
    return ret;
}

/////////////////////////////////////////////////////////////////////////////

string ReportItem::dumpString() const {
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
    cumulative = ntohl(cumulative) >> 8;
    seq_cycles = ntohs(seq_cycles);
    seq_max = ntohs(seq_max);
    jitter = ntohl(jitter);
    last_sr_stamp = ntohl(last_sr_stamp);
    delay_since_last_sr = ntohl(delay_since_last_sr);
}

/////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RtcpRR> RtcpRR::create(size_t item_count) {
    auto real_size = sizeof(RtcpRR) - sizeof(ReportItem) + item_count * sizeof(ReportItem);
    auto bytes = alignSize(real_size);
    auto ptr = (RtcpRR *)new char[bytes];
    setupHeader(ptr, RtcpType::RTCP_RR, item_count, bytes);
    setupPadding(ptr, bytes - real_size);
    return std::shared_ptr<RtcpRR>(ptr, [](RtcpRR *ptr) { delete[](char *) ptr; });
}

string RtcpRR::dumpString() const {
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
    ssrc = ntohl(ssrc);

    ReportItem *ptr = &items;
    int item_count = 0;
    for (int i = 0; i < (int)report_count && (char *)(ptr) + sizeof(ReportItem) <= (char *)(this) + size; ++i) {
        ptr->net2Host();
        ++ptr;
        ++item_count;
    }
    CHECK_REPORT_COUNT(item_count);
}

vector<ReportItem *> RtcpRR::getItemList() {
    vector<ReportItem *> ret;
    ReportItem *ptr = &items;
    for (int i = 0; i < (int)report_count; ++i) {
        ret.emplace_back(ptr);
        ++ptr;
    }
    return ret;
}

/////////////////////////////////////////////////////////////////////////////

void SdesChunk::net2Host() {
    ssrc = ntohl(ssrc);
}

size_t SdesChunk::totalBytes() const {
    return alignSize(minSize() + txt_len);
}

size_t SdesChunk::minSize() {
    return sizeof(SdesChunk) - sizeof(text);
}

string SdesChunk::dumpString() const {
    _StrPrinter printer;
    printer << "ssrc:" << ssrc << "\r\n";
    printer << "type:" << sdesTypeToStr((SdesType)type) << "\r\n";
    printer << "txt_len:" << (int)txt_len << "\r\n";
    printer << "text:" << (txt_len ? string(text, txt_len) : "") << "\r\n";
    return std::move(printer);
}

/////////////////////////////////////////////////////////////////////////////

std::shared_ptr<RtcpSdes> RtcpSdes::create(const std::vector<string> &item_text) {
    size_t item_total_size = 0;
    for (auto &text : item_text) {
        // 统计所有SdesChunk对象占用的空间
        item_total_size += alignSize(SdesChunk::minSize() + (0xFF & text.size()));
    }
    auto real_size = sizeof(RtcpSdes) - sizeof(SdesChunk) + item_total_size;
    auto bytes = alignSize(real_size);
    auto ptr = (RtcpSdes *)new char[bytes];
    memset(ptr, 0x00, bytes);
    auto item_ptr = &ptr->chunks;
    for (auto &text : item_text) {
        item_ptr->txt_len = (0xFF & text.size());
        // 确保赋值\0为RTCP_SDES_END
        memcpy(item_ptr->text, text.data(), item_ptr->txt_len + 1);
        item_ptr = (SdesChunk *)((char *)item_ptr + item_ptr->totalBytes());
    }

    setupHeader(ptr, RtcpType::RTCP_SDES, item_text.size(), bytes);
    setupPadding(ptr, bytes - real_size);
    return std::shared_ptr<RtcpSdes>(ptr, [](RtcpSdes *ptr) { delete[](char *) ptr; });
}

string RtcpSdes::dumpString() const {
    _StrPrinter printer;
    printer << RtcpHeader::dumpHeader();
    auto items = ((RtcpSdes *)this)->getChunkList();
    auto i = 0;
    for (auto &item : items) {
        printer << "---- item:" << i++ << " ----\r\n";
        printer << item->dumpString();
    }
    return std::move(printer);
}

void RtcpSdes::net2Host(size_t size) {
    static const size_t kMinSize = sizeof(RtcpSdes) - sizeof(chunks);
    CHECK_MIN_SIZE(size, kMinSize);
    SdesChunk *ptr = &chunks;
    int item_count = 0;
    for (int i = 0; i < (int)report_count && (char *)(ptr) + SdesChunk::minSize() <= (char *)(this) + size; ++i) {
        ptr->net2Host();
        ptr = (SdesChunk *)((char *)ptr + ptr->totalBytes());
        ++item_count;
    }
    CHECK_REPORT_COUNT(item_count);
}

vector<SdesChunk *> RtcpSdes::getChunkList() {
    vector<SdesChunk *> ret;
    SdesChunk *ptr = &chunks;
    for (int i = 0; i < (int)report_count; ++i) {
        ret.emplace_back(ptr);
        ptr = (SdesChunk *)((char *)ptr + ptr->totalBytes());
    }
    return ret;
}

////////////////////////////////////////////////////////////////////

std::shared_ptr<RtcpFB> RtcpFB::create_l(RtcpType type, int fmt, const void *fci, size_t fci_len) {
    if (!fci) {
        fci_len = 0;
    }
    auto real_size = sizeof(RtcpFB) + fci_len;
    auto bytes = alignSize(real_size);
    auto ptr = (RtcpFB *)new char[bytes];
    if (fci && fci_len) {
        memcpy((char *)ptr + sizeof(RtcpFB), fci, fci_len);
    }
    setupHeader(ptr, type, fmt, bytes);
    setupPadding(ptr, bytes - real_size);
    return std::shared_ptr<RtcpFB>((RtcpFB *)ptr, [](RtcpFB *ptr) { delete[](char *) ptr; });
}

std::shared_ptr<RtcpFB> RtcpFB::create(PSFBType fmt, const void *fci, size_t fci_len) {
    return RtcpFB::create_l(RtcpType::RTCP_PSFB, (int)fmt, fci, fci_len);
}

std::shared_ptr<RtcpFB> RtcpFB::create(RTPFBType fmt, const void *fci, size_t fci_len) {
    return RtcpFB::create_l(RtcpType::RTCP_RTPFB, (int)fmt, fci, fci_len);
}

const void *RtcpFB::getFciPtr() const {
    return (uint8_t *)&ssrc_media + sizeof(ssrc_media);
}

size_t RtcpFB::getFciSize() const {
    auto fci_len = (ssize_t)getSize() - getPaddingSize() - sizeof(RtcpFB);
    CHECK(fci_len >= 0);
    return fci_len;
}

string RtcpFB::dumpString() const {
    _StrPrinter printer;
    printer << RtcpHeader::dumpHeader();
    printer << "ssrc:" << ssrc << "\r\n";
    printer << "ssrc_media:" << ssrc_media << "\r\n";
    switch ((RtcpType)pt) {
        case RtcpType::RTCP_PSFB: {
            switch ((PSFBType)report_count) {
                case PSFBType::RTCP_PSFB_SLI: {
                    auto &fci = getFci<FCI_SLI>();
                    printer << "fci:" << psfbTypeToStr((PSFBType)report_count) << " " << fci.dumpString();
                    break;
                }
                case PSFBType::RTCP_PSFB_PLI: {
                    getFciSize();
                    printer << "fci:" << psfbTypeToStr((PSFBType)report_count);
                    break;
                }

                case PSFBType::RTCP_PSFB_FIR: {
                    auto &fci = getFci<FCI_FIR>();
                    printer << "fci:" << psfbTypeToStr((PSFBType)report_count) << " " << fci.dumpString();
                    break;
                }

                case PSFBType::RTCP_PSFB_REMB: {
                    auto &fci = getFci<FCI_REMB>();
                    printer << "fci:" << psfbTypeToStr((PSFBType)report_count) << " " << fci.dumpString();
                    break;
                }
                default: {
                    printer << "fci:" << psfbTypeToStr((PSFBType)report_count) << " "
                            << hexdump(getFciPtr(), getFciSize());
                    break;
                }
            }
            break;
        }
        case RtcpType::RTCP_RTPFB: {
            switch ((RTPFBType)report_count) {
                case RTPFBType::RTCP_RTPFB_NACK: {
                    auto &fci = getFci<FCI_NACK>();
                    printer << "fci:" << rtpfbTypeToStr((RTPFBType)report_count) << " " << fci.dumpString();
                    break;
                }
                case RTPFBType::RTCP_RTPFB_TWCC: {
                    auto &fci = getFci<FCI_TWCC>();
                    printer << "fci:" << rtpfbTypeToStr((RTPFBType)report_count) << " " << fci.dumpString(getFciSize());
                    break;
                }
                default: {
                    printer << "fci:" << rtpfbTypeToStr((RTPFBType)report_count) << " "
                            << hexdump(getFciPtr(), getFciSize());
                    break;
                }
            }
            break;
        }
        default: /*不可达*/ assert(0); break;
    }
    return std::move(printer);
}

void RtcpFB::net2Host(size_t size) {
    static const size_t kMinSize = sizeof(RtcpFB);
    CHECK_MIN_SIZE(size, kMinSize);
    ssrc = ntohl(ssrc);
    ssrc_media = ntohl(ssrc_media);
}

////////////////////////////////////////////////////////////////////

std::shared_ptr<RtcpBye> RtcpBye::create(const std::vector<uint32_t> &ssrcs, const string &reason) {
    assert(reason.size() <= 0xFF);
    auto real_size = sizeof(RtcpHeader) + sizeof(uint32_t) * ssrcs.size() + 1 + reason.size();
    auto bytes = alignSize(real_size);
    auto ptr = (RtcpBye *)new char[bytes];
    setupHeader(ptr, RtcpType::RTCP_BYE, ssrcs.size(), bytes);
    setupPadding(ptr, bytes - real_size);

    int i = 0;
    for (auto ssrc : ssrcs) {
        ((RtcpBye *)ptr)->ssrc[i++] = htonl(ssrc);
    }

    if (!reason.empty()) {
        uint8_t *reason_len_ptr = (uint8_t *)ptr + sizeof(RtcpHeader) + sizeof(uint32_t) * ssrcs.size();
        *reason_len_ptr = reason.size() & 0xFF;
        memcpy(reason_len_ptr + 1, reason.data(), *reason_len_ptr);
    }

    return std::shared_ptr<RtcpBye>(ptr, [](RtcpBye *ptr) { delete[](char *) ptr; });
}

vector<uint32_t *> RtcpBye::getSSRC() {
    vector<uint32_t *> ret;
    for (size_t i = 0; i < report_count; ++i) {
        ret.emplace_back(&(ssrc[i]));
    }
    return ret;
}

string RtcpBye::getReason() const {
    auto *reason_len_ptr = &reason_len + sizeof(ssrc) * (report_count - 1);
    if (reason_len_ptr + 1 >= (uint8_t *)this + getSize()) {
        return "";
    }
    return string((char *)reason_len_ptr + 1, *reason_len_ptr);
}

string RtcpBye::dumpString() const {
    _StrPrinter printer;
    printer << RtcpHeader::dumpHeader();
    for (auto ssrc : ((RtcpBye *)this)->getSSRC()) {
        printer << "ssrc:" << *ssrc << "\r\n";
    }
    printer << "reason:" << getReason();
    return std::move(printer);
}

void RtcpBye::net2Host(size_t size) {
    static const size_t kMinSize = sizeof(RtcpHeader);
    CHECK_MIN_SIZE(size, kMinSize);
    size_t offset = kMinSize;
    size_t i = 0;
    for (; i < report_count && offset + sizeof(ssrc) <= size; ++i) {
        ssrc[i] = ntohl(ssrc[i]);
        offset += sizeof(ssrc);
    }
    // 修正ssrc个数
    CHECK_REPORT_COUNT(i);

    if (offset < size) {
        uint8_t *reason_len_ptr = &reason_len + sizeof(ssrc) * (report_count - 1);
        if (reason_len_ptr + 1 + *reason_len_ptr > (uint8_t *)this + size) {
            WarnL << "invalid rtcp bye reason length";
            // 修正reason_len长度
            *reason_len_ptr = ((uint8_t *)this + size - reason_len_ptr - 1) & 0xFF;
        }
    }
}
////////////////////////////////////////////
string RtcpXRRRTR::dumpString() const {
    _StrPrinter printer;
    printer << RtcpHeader::dumpHeader();
    printer << "ssrc :" << ssrc << "\r\n";
    printer << "bt :" << (int)bt << "\r\n";
    printer << "block_length : " << block_length << "\r\n";
    printer << "ntp msw : " << ntpmsw << "\r\n";
    printer << "ntp lsw : " << ntplsw << "\r\n";
    return std::move(printer);
}

void RtcpXRRRTR::net2Host(size_t size) {
    static const size_t kMinSize = sizeof(RtcpHeader);
    CHECK_MIN_SIZE(size, kMinSize);
    if (size != sizeof(RtcpXRRRTR)) {
        throw std::invalid_argument(
            StrPrinter << "rtcp xr Receiver Reference Time Report Block must is " << sizeof(RtcpXRRRTR)
                       << " actual size " << size);
    }
    ssrc = ntohl(ssrc);
    block_length = ntohs(block_length);
    ntpmsw = ntohl(ntpmsw);
    ntplsw = ntohl(ntplsw);
}

string RtcpXRDLRRReportItem::dumpString() const {
    _StrPrinter printer;

    printer << "ssrc :" << ssrc << "\r\n";
    printer << "last RR (lrr) :" << lrr << "\r\n";
    printer << "delay since last RR (dlrr): " << dlrr << "\r\n";

    return std::move(printer);
}

void RtcpXRDLRRReportItem::net2Host() {
    ssrc = ntohl(ssrc);
    lrr = ntohl(lrr);
    dlrr = ntohl(dlrr);
}

std::vector<RtcpXRDLRRReportItem *> RtcpXRDLRR::getItemList() {
    auto count = block_length / 3;
    RtcpXRDLRRReportItem *ptr = &items;
    vector<RtcpXRDLRRReportItem *> ret;
    for (int i = 0; i < (int)count; ++i) {
        ret.emplace_back(ptr);
        ++ptr;
    }
    return ret;
}
string RtcpXRDLRR::dumpString() const {
    _StrPrinter printer;
    printer << RtcpHeader::dumpHeader();
    printer << "ssrc :" << ssrc << "\r\n";
    printer << "bt :" << (int)bt << "\r\n";
    printer << "block_length : " << block_length << "\r\n";
    auto items_list = ((RtcpXRDLRR *)this)->getItemList();
    auto i = 0;
    for (auto &item : items_list) {
        printer << "---- item:" << i++ << " ----\r\n";
        printer << item->dumpString();
    }
    return std::move(printer);
}

void RtcpXRDLRR::net2Host(size_t size) {
    static const size_t kMinSize = sizeof(RtcpHeader);
    CHECK_MIN_SIZE(size, kMinSize);

    ssrc = ntohl(ssrc);
    block_length = ntohs(block_length);

    auto count = block_length / 3;
    for (int i = 0; i < (int)count; ++i) {
        RtcpXRDLRRReportItem *ptr = &items;
        ptr->net2Host();
        ptr++;
    }
}

std::shared_ptr<RtcpXRDLRR> RtcpXRDLRR::create(size_t item_count) {
    auto real_size = sizeof(RtcpXRDLRR) - sizeof(RtcpXRDLRRReportItem) + item_count * sizeof(RtcpXRDLRRReportItem);
    auto bytes = alignSize(real_size);
    auto ptr = (RtcpXRDLRR *)new char[bytes];
    setupHeader(ptr, RtcpType::RTCP_XR, 0, bytes);
    setupPadding(ptr, bytes - real_size);
    return std::shared_ptr<RtcpXRDLRR>(ptr, [](RtcpXRDLRR *ptr) { delete[](char *) ptr; });
}

#if 0
#include "Util/onceToken.h"

static toolkit::onceToken token([](){
    auto bye = RtcpBye::create({1,2,3,4,5,6}, "this is a bye reason");
    auto buffer = RtcpHeader::toBuffer(bye);

    auto rtcps = RtcpHeader::loadFromBytes(buffer->data(), buffer->size());
    for(auto rtcp : rtcps){
        std::cout << rtcp->dumpString() << std::endl;
    }
});
#endif

} // namespace mediakit