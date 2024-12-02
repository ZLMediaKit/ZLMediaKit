#include "Common/config.h"
#include "Http/HttpSession.h"
#include "Network/TcpServer.h"
#include "Rtmp/RtmpSession.h"
#include "Rtp/RtpProcess.h"
#include "Rtsp/RtspSession.h"
#include "Util/logger.h"
#include "Util/util.h"
#include <iostream>
#include <map>
#include <pcap.h>

using namespace std;
using namespace toolkit;
using namespace mediakit;

/* 以太网帧头部 */
struct sniff_ethernet {
#define ETHER_ADDR_LEN 6
    u_char ether_dhost[ETHER_ADDR_LEN]; /* 目的主机的地址 */
    u_char ether_shost[ETHER_ADDR_LEN]; /* 源主机的地址 */
    u_short ether_unused;
    u_short ether_type; /* IP：0x0800;IPV6:0x86DD; ARP:0x0806;RARP:0x8035 */
};

#define ETHERTYPE_IPV4 (0x0800)
#define ETHERTYPE_IPV6 (0x86DD)
#define ETHERTYPE_ARP (0x0806)
#define ETHERTYPE_RARP (0x8035)

/* IP数据包的头部 */
struct sniff_ip {
#if BYTE_ORDER == LITTLE_ENDIAN
    u_int ip_hl : 4, /* 头部长度 */
        ip_v : 4;    /* 版本号 */
#if BYTE_ORDER == BIG_ENDIAN
    u_int ip_v : 4, /* 版本号 */
        ip_hl : 4;  /* 头部长度 */
#endif
#endif              /* not _IP_VHL */
    u_char ip_tos;  /* 服务的类型 */
    u_short ip_len; /* 总长度 */
    u_short ip_id;  /*包标志号 */
    u_char ip_flag;
    u_char ip_off;                 /* 碎片偏移 */
#define IP_RF 0x8000               /* 保留的碎片标志 */
#define IP_DF 0x4000               /* dont fragment flag */
#define IP_MF 0x2000               /* 多碎片标志*/
#define IP_OFFMASK 0x1fff          /*分段位 */
    u_char ip_ttl;                 /* 数据包的生存时间 */
    u_char ip_p;                   /* 所使用的协议:1 ICMP;2 IGMP;4 IP;6 TCP;17 UDP;89 OSPF */
    u_short ip_sum;                /* 校验和 */
    struct in_addr ip_src, ip_dst; /* 源地址、目的地址*/
};
#define IPTYPE_ICMP (1)
#define IPTYPE_IGMP (2)
#define IPTYPE_IP (4)
#define IPTYPE_TCP (6)
#define IPTYPE_UDP (17)
#define IPTYPE_OSPF (89)

typedef u_int tcp_seq;
/* TCP 数据包的头部 */
struct sniff_tcp {
    u_short th_sport; /* 源端口 */
    u_short th_dport; /* 目的端口 */
    tcp_seq th_seq;   /* 包序号 */
    tcp_seq th_ack;   /* 确认序号 */
#if BYTE_ORDER == LITTLE_ENDIAN
    u_int th_x2 : 4, /* 还没有用到 */
        th_off : 4;  /* 数据偏移 */
#endif
#if BYTE_ORDER == BIG_ENDIAN
    u_int th_off : 4, /* 数据偏移*/
        th_x2 : 4;    /*还没有用到 */
#endif
    u_char th_flags;
#define TH_FIN 0x01
#define TH_SYN 0x02
#define TH_RST 0x04
#define TH_PUSH 0x08
#define TH_ACK 0x10
#define TH_URG 0x20
#define TH_ECE 0x40
#define TH_CWR 0x80
#define TH_FLAGS (TH_FINTH_SYNTH_RSTTH_ACKTH_URGTH_ECETH_CWR)
    u_short th_win; /* TCP滑动窗口 */
    u_short th_sum; /* 头部校验和 */
    u_short th_urp; /* 紧急服务位 */
};

/* UDP header */
struct sniff_udp {
    uint16_t sport; /* source port */
    uint16_t dport; /* destination port */
    uint16_t udp_length;
    uint16_t udp_sum; /* checksum */
};

struct rtp_stream {
    uint64_t stamp = 0;
    uint64_t stamp_last = 0;
    std::shared_ptr<RtpProcess> rtp_process;
    Socket::Ptr sock;
    struct sockaddr_storage addr;
};
static semaphore sem;
unordered_map<uint32_t, rtp_stream> rtp_streams_map;

#if defined(ENABLE_RTPPROXY)
void processRtp(uint32_t stream_id, const char *rtp, int &size, bool is_udp, const EventPoller::Ptr &poller) {
    rtp_stream &stream = rtp_streams_map[stream_id];
    if (!stream.rtp_process) {
        auto process = RtpProcess::createProcess(MediaTuple{DEFAULT_VHOST, kRtpAppName, to_string(stream_id), ""});
        stream.rtp_process = process;
        struct sockaddr_storage addr;
        memset(&addr, 0, sizeof(addr));
        addr.ss_family = AF_INET;
        auto sock = Socket::createSocket(poller);
        stream.sock = sock;
        stream.addr = addr;
    }

    try {
        stream.rtp_process->inputRtp(is_udp, stream.sock, rtp, size, (struct sockaddr *)&stream.addr, &stream.stamp);
    } catch (std::exception &ex) {
        WarnL << "Input rtp failed: " << ex.what();
        return ;
    }

    auto diff = static_cast<int64_t>(stream.stamp - stream.stamp_last);
    if (diff > 0 && diff < 500) {
        usleep(diff * 1000);
    } else {
        usleep(1 * 1000);
    }
    stream.stamp_last = stream.stamp;

    rtp = nullptr;
    size = 0;
}
#endif // #if defined(ENABLE_RTPPROXY)

static bool loadFile(const char *path, const EventPoller::Ptr &poller) {
    char errbuf[PCAP_ERRBUF_SIZE] = {'\0'};
    std::shared_ptr<pcap_t> handle(pcap_open_offline(path, errbuf), [](pcap_t *handle) {
        sem.post();
        if (handle) {
            pcap_close(handle);
        }
    });
    if (!handle) {
        WarnL << "open file failed:" << path << "error: " << errbuf;
        return false;
    }
    auto total_size = std::make_shared<size_t>(0);
    struct pcap_pkthdr header = {0};
    while (true) {
        const u_char *pkt_buff = pcap_next(handle.get(), &header);
        if (!pkt_buff) {
            PrintE("pcapng read over.");
            break;
        }

        struct sniff_ethernet *ethernet = (struct sniff_ethernet *)pkt_buff;
        int eth_len = sizeof(struct sniff_ethernet);  // 以太网头的长度
        int ip_len = sizeof(struct sniff_ip);         // ip头的长度
        int tcp_len = sizeof(struct sniff_tcp);       // tcp头的长度
        int udp_headr_len = sizeof(struct sniff_udp); // udp头的长度

        /*解析网络层  IP头*/
        if (ntohs(ethernet->ether_type) == ETHERTYPE_IPV4) { // IPV4
            struct sniff_ip *ip = (struct sniff_ip *)(pkt_buff + eth_len);
            ip_len = (ip->ip_hl & 0x0f) * 4;                            // ip头的长度
            unsigned char *saddr = (unsigned char *)&ip->ip_src.s_addr; // 网络字节序转换成主机字节序
            unsigned char *daddr = (unsigned char *)&ip->ip_dst.s_addr;
            /*解析传输层  TCP、UDP、ICMP*/
            if (ip->ip_p == IPTYPE_TCP) { // TCP
                PrintI("ip->proto:TCP "); // 传输层用的哪一个协议
                struct sniff_tcp *tcp = (struct sniff_tcp *)(pkt_buff + eth_len + ip_len);
                PrintI("tcp_sport = %u ", tcp->th_sport);
                PrintI("tcp_dport = %u ", tcp->th_dport);
                for (int i = 0; *(pkt_buff + eth_len + ip_len + tcp_len + i) != '\0'; i++) {
                    PrintI("%02x ", *(pkt_buff + eth_len + ip_len + tcp_len + i));
                }
            } else if (ip->ip_p == IPTYPE_UDP) { // UDP
                // PrintI("ip->proto:UDP ");        // 传输层用的哪一个协议
                struct sniff_udp *udp = (struct sniff_udp *)(pkt_buff + eth_len + ip_len);
                auto udp_pack_len = ntohs(udp->udp_length);

                uint32_t src_ip = ntohl(ip->ip_src.s_addr);
                uint32_t dst_ip = ntohl(ip->ip_dst.s_addr);
                uint16_t src_port = ntohs(udp->sport);
                uint16_t dst_port = ntohs(udp->dport);
                uint32_t stream_id = (src_ip << 16) + src_port + (dst_ip << 4) + dst_port;

                const char *rtp = reinterpret_cast<const char *>(pkt_buff + eth_len + ip_len + udp_headr_len);
                auto rtp_len = udp_pack_len - udp_headr_len;
#if defined(ENABLE_RTPPROXY)
                processRtp(stream_id, rtp, rtp_len, true, poller);
#endif                                            // #if defined(ENABLE_RTPPROXY)
            } else if (ip->ip_p == IPTYPE_ICMP) { // ICMP
                PrintI("ip->proto:CCMP ");        // 传输层用的哪一个协议
            } else {
                PrintI("未识别的传输层协议");
            }

        } else if (ntohs(ethernet->ether_type) == ETHERTYPE_IPV6) { // IPV6
            PrintI("It's IPv6! ");
        } else {
            PrintI("既不是IPV4也不是IPV6 ");
        }
    }

    return true;
}

int main(int argc, char *argv[]) {
    // 设置日志
    Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel"));

    // 启动异步日志线程
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
    loadIniConfig((exeDir() + "config.ini").data());

    TcpServer::Ptr rtspSrv(new TcpServer());
    TcpServer::Ptr rtmpSrv(new TcpServer());
    TcpServer::Ptr httpSrv(new TcpServer());
    rtspSrv->start<RtspSession>(554);  // 默认554
    rtmpSrv->start<RtmpSession>(1935); // 默认1935
    httpSrv->start<HttpSession>(81);   // 默认80

    if (argc == 2) {
        auto poller = EventPollerPool::Instance().getPoller();
        poller->async_first([poller, argv]() {
            loadFile(argv[1], poller);
            sem.post();
        });
        sem.wait();
        sleep(1);
    } else {
        ErrorL << "parameter error.";
    }

    return 0;
}