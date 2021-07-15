/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Rtcp/Rtcp.h"
#include "Util/logger.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

void printRtcp(const std::shared_ptr<Buffer> &buffer){
    auto rtcp_arr = RtcpHeader::loadFromBytes(buffer->data(), buffer->size());
    //转换为主机字节序方可打印
    InfoL << "\r\n" << rtcp_arr[0]->dumpString();
}

std::shared_ptr<Buffer> makeRtcpSR() {
    auto rtcp = RtcpSR::create(3);
    rtcp->ssrc = htonl(1);
    struct timeval tv;
    gettimeofday(&tv, NULL);
    rtcp->setNtpStamp(tv);
    rtcp->rtpts = htonl(2);
    rtcp->packet_count = htonl(3);
    rtcp->octet_count = htonl(4);
    auto i = 5;
    for (auto &ptr : rtcp->getItemList()) {
        ReportItem *item = (ReportItem *) ptr;
        item->ssrc = htonl(i++);
        item->fraction = i++;
        item->cumulative = htonl(i++) >> 8;
        item->seq_cycles = htons(i++);
        item->seq_max = htons(i++);
        item->jitter = htonl(i++);
        item->last_sr_stamp = htonl(i++);
        item->delay_since_last_sr = htonl(i++);
    }
    //返回网络字节序
    return RtcpHeader::toBuffer(rtcp);
}

std::shared_ptr<Buffer> makeRtcpRR() {
    auto rtcp = RtcpRR::create(3);
    rtcp->ssrc = htonl(1);
    auto i = 5;
    for (auto &ptr : rtcp->getItemList()) {
        ReportItem *item = (ReportItem *) ptr;
        item->ssrc = htonl(i++);
        item->fraction = i++;
        item->cumulative = htonl(i++) >> 8;
        item->seq_cycles = htons(i++);
        item->seq_max = htons(i++);
        item->jitter = htonl(i++);
        item->last_sr_stamp = htonl(i++);
        item->delay_since_last_sr = htonl(i++);
    }
    //返回网络字节序
    return RtcpHeader::toBuffer(rtcp);
}

std::shared_ptr<Buffer> makeRtcpSDES() {
    auto rtcp = RtcpSdes::create({"zlmediakit", "", "https://github.com/xia-chu/ZLMediaKit", "1213642868@qq.com", "123456789012345678"});
    auto i = 5;
    auto items = rtcp->getChunkList();
    items[0]->type = (uint8_t)SdesType::RTCP_SDES_CNAME;
    items[0]->ssrc = htonl(i++);

    items[1]->type = (uint8_t)SdesType::RTCP_SDES_NOTE;
    items[1]->ssrc = htonl(i++);

    items[2]->type = (uint8_t)SdesType::RTCP_SDES_LOC;
    items[2]->ssrc = htonl(i++);

    items[3]->type = (uint8_t)SdesType::RTCP_SDES_EMAIL;
    items[3]->ssrc = htonl(i++);

    items[4]->type = (uint8_t)SdesType::RTCP_SDES_PHONE;
    items[4]->ssrc = htonl(i++);

    //返回网络字节序
    return RtcpHeader::toBuffer(rtcp);
}


int main(int argc, char *argv[]){
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    {
        static char rtcp_data[] = "\x81\xca\x00\x05\x70\xd8\xac\x1b\x01\x0b\x7a\x73\x68\x50\x43\x40"
                                  "\x7a\x73\x68\x50\x43\x00\x00\x00"
                                  "\x81\xc9\x00\x07\x70\xd8\xac\x1b\x55\x66\x77\x88\x00\x00\x00\x00"
                                  "\x00\x00\x0d\x21\x00\x00\x00\x32\xdd\xf1\x00\x00\x00\x03\x4f\x67"
                                  "\x80\xc8\x00\x06\x55\x66\x77\x88\xe3\x70\xdd\xf1\x00\x00\xc2\xb8"
                                  "\x00\x21\xe4\x90\x00\x00\x0b\x81\x00\x2f\x6a\x60";
        auto rtcp_arr = RtcpHeader::loadFromBytes(rtcp_data, sizeof(rtcp_data) - 1);
        for (auto &rtcp : rtcp_arr) {
            DebugL << "\r\n" << rtcp->dumpString();
        }

    }

    {
        printRtcp(makeRtcpSR());
        printRtcp(makeRtcpRR());
        printRtcp(makeRtcpSDES());
    }

    {
        string str;
        auto sr = makeRtcpSR();
        auto rr = makeRtcpRR();
        auto sdes = makeRtcpSDES();
        str.append(sr->data(), sr->size());
        str.append(rr->data(), rr->size());
        str.append(sdes->data(), sdes->size());
        //测试内存越界
        char *data = new char[str.size()];
        memcpy(data, str.data(), str.size());
        auto rtcp_arr = RtcpHeader::loadFromBytes(data, str.size());
        for (auto &rtcp : rtcp_arr) {
            WarnL << "\r\n" << rtcp->dumpString();
        }
        delete [] data;
    }

}