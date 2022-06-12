#ifndef ZLMEDIAKIT_SRT_TRANSPORT_H
#define ZLMEDIAKIT_SRT_TRANSPORT_H

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>

#include "Network/Session.h"
#include "Poller/EventPoller.h"
#include "Poller/Timer.h"

#include "Common.hpp"
#include "NackContext.hpp"
#include "Packet.hpp"
#include "PacketQueue.hpp"
#include "PacketSendQueue.hpp"
#include "Statistic.hpp"
namespace SRT {

using namespace toolkit;

extern const std::string kPort;
extern const std::string kTimeOutSec;
extern const std::string kLatencyMul;
extern const std::string kPktBufSize;

class SrtTransport : public std::enable_shared_from_this<SrtTransport> {
public:
    friend class SrtSession;
    using Ptr = std::shared_ptr<SrtTransport>;

    SrtTransport(const EventPoller::Ptr &poller);
    virtual ~SrtTransport();
    const EventPoller::Ptr &getPoller() const;
    void setSession(Session::Ptr session);
    const Session::Ptr &getSession() const;

    /**
     * socket收到udp数据
     * @param buf 数据指针
     * @param len 数据长度
     * @param addr 数据来源地址
     */
    virtual void inputSockData(uint8_t *buf, int len, struct sockaddr_storage *addr);
    virtual void onSendTSData(const Buffer::Ptr &buffer, bool flush);

    std::string getIdentifier();
    void unregisterSelf();
    void unregisterSelfHandshake();

protected:
    virtual bool isPusher() { return true; };
    virtual void onSRTData(DataPacket::Ptr pkt) {};
    virtual void onShutdown(const SockException &ex);
    virtual void onHandShakeFinished(std::string &streamid, struct sockaddr_storage *addr) {};
    virtual void sendPacket(Buffer::Ptr pkt, bool flush = true);
    virtual int getLatencyMul() { return 4; };
    virtual int getPktBufSize() { return 8192; };

private:
    void registerSelf();
    void registerSelfHandshake();

    void switchToOtherTransport(uint8_t *buf, int len, uint32_t socketid, struct sockaddr_storage *addr);

    void handleHandshake(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handleHandshakeInduction(HandshakePacket &pkt, struct sockaddr_storage *addr);
    void handleHandshakeConclusion(HandshakePacket &pkt, struct sockaddr_storage *addr);

    void handleKeeplive(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handleACK(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handleACKACK(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handleNAK(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handleCongestionWarning(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handleShutDown(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handleDropReq(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handleUserDefinedType(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handlePeerError(uint8_t *buf, int len, struct sockaddr_storage *addr);
    void handleDataPacket(uint8_t *buf, int len, struct sockaddr_storage *addr);

    void sendNAKPacket(std::list<PacketQueue::LostPair> &lost_list);
    void sendACKPacket();
    void sendLightACKPacket();
    void sendKeepLivePacket();
    void sendShutDown();
    void sendMsgDropReq(uint32_t first, uint32_t last);

    size_t getPayloadSize();

protected:
    void sendDataPacket(DataPacket::Ptr pkt, char *buf, int len, bool flush = false);
    void sendControlPacket(ControlPacket::Ptr pkt, bool flush = true);

private:
    // 当前选中的udp链接
    Session::Ptr _selected_session;
    // 链接迁移前后使用过的udp链接
    std::unordered_map<Session *, std::weak_ptr<Session>> _history_sessions;

    EventPoller::Ptr _poller;

    uint32_t _peer_socket_id;
    uint32_t _socket_id = 0;

    TimePoint _now;
    TimePoint _start_timestamp;

    // for calculate rtt for delay
    TimePoint _induction_ts;

    uint32_t _mtu = 1500;
    uint32_t _max_window_size = 8192;
    uint32_t _init_seq_number = 0;

    std::string _stream_id;
    uint32_t _sync_cookie = 0;
    uint32_t _send_packet_seq_number = 0;
    uint32_t _send_msg_number = 1;

    PacketSendQueue::Ptr _send_buf;
    uint32_t _buf_delay = 120;
    PacketQueueInterface::Ptr _recv_buf;
    // NackContext _recv_nack;
    uint32_t _rtt = 100 * 1000;
    uint32_t _rtt_variance = 50 * 1000;
    uint32_t _light_ack_pkt_count = 0;
    uint32_t _ack_number_count = 0;
    uint32_t _last_ack_pkt_seq_num = 0;

    uint32_t _last_pkt_seq = 0;
    UTicker _ack_ticker;
    std::map<uint32_t, TimePoint> _ack_send_timestamp;

    std::shared_ptr<PacketRecvRateContext> _pkt_recv_rate_context;
    std::shared_ptr<EstimatedLinkCapacityContext> _estimated_link_capacity_context;
    std::shared_ptr<RecvRateContext> _recv_rate_context;

    UTicker _nak_ticker;

    // 保持发送的握手消息，防止丢失重发
    HandshakePacket::Ptr _handleshake_res;

    ResourcePool<BufferRaw> _packet_pool;
};

class SrtTransportManager {
public:
    static SrtTransportManager &Instance();
    SrtTransport::Ptr getItem(const std::string &key);
    void addItem(const std::string &key, const SrtTransport::Ptr &ptr);
    void removeItem(const std::string &key);

    void addHandshakeItem(const std::string &key, const SrtTransport::Ptr &ptr);
    void removeHandshakeItem(const std::string &key);
    SrtTransport::Ptr getHandshakeItem(const std::string &key);

private:
    SrtTransportManager() = default;

private:
    std::mutex _mtx;
    std::unordered_map<std::string, std::weak_ptr<SrtTransport>> _map;

    std::mutex _handshake_mtx;
    std::unordered_map<std::string, std::weak_ptr<SrtTransport>> _handshake_map;
};

} // namespace SRT

#endif // ZLMEDIAKIT_SRT_TRANSPORT_H