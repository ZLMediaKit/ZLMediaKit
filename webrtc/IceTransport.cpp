/**
ISC License

Copyright © 2015, Iñaki Baz Castillo <ibc@aliax.net>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */


#include <utility>
#include <random>
#include <algorithm>
#include "Util/onceToken.h"
#include "Common/Parser.h"
#include "Common/config.h"
#include "IceTransport.hpp"
#include "WebRtcTransport.h"
#include "Network/UdpClient.h"
#include "json/json.h"

using namespace toolkit;
using namespace mediakit;
using namespace std;

namespace RTC {

// 定义RequestInfo的静态常量成员
const uint32_t IceTransport::RequestInfo::INITIAL_RTO;
const uint32_t IceTransport::RequestInfo::MAX_RETRIES;

#define RTC_FIELD "rtc."
const string kPortRange = RTC_FIELD "port_range";
static onceToken token([]() {
    mINI::Instance()[kPortRange] = "49152-65535";
});

static uint32_t calIceCandidatePriority(CandidateInfo::AddressType type, uint32_t component_id = 1) {
    uint32_t type_preference;
    switch (type) {
        case CandidateInfo::AddressType::HOST: type_preference = 126;  break;
        case CandidateInfo::AddressType::PRFLX: type_preference = 110; break;
        case CandidateInfo::AddressType::SRFLX: type_preference = 100; break;
        case CandidateInfo::AddressType::RELAY: type_preference = 0; break;
        default: throw std::invalid_argument(StrPrinter << "not support type :" << (uint32_t)type);
    }

    uint32_t local_preference = 100;
    return (type_preference << 24) + (local_preference << 8) + (256 - component_id);
}

uint64_t calCandidatePairPriority(uint32_t G, uint32_t D) {
    uint32_t min_p = (G < D) ? G : D;
    uint32_t max_p = (G > D) ? G : D;
    return ((uint64_t)min_p << 32) | (2 * (uint64_t)max_p) | (G > D ? 1 : 0);
}

// 获取候选者地址类型字符串的静态函数
static std::string getAddressTypeStr(CandidateInfo::AddressType type) {
    switch (type) {
        case CandidateInfo::AddressType::HOST: return "host";
        case CandidateInfo::AddressType::SRFLX: return "srflx";
        case CandidateInfo::AddressType::PRFLX: return "reflx";
        case CandidateInfo::AddressType::RELAY: return "relay";
        default: return "invalid";
    }
}

// 检查ICE传输策略是否允许该候选者对
static bool checkIceTransportPolicy(const IceAgent::CandidatePair& pair_info, IceTransport::Pair::Ptr pair) {
    GET_CONFIG(int, ice_transport_policy, Rtc::kIceTransportPolicy);
    
    // 优先使用新的统一配置参数
    switch (ice_transport_policy) {
        case static_cast<int>(IceTransportPolicy::kRelayOnly):
            // 仅支持Relay转发：要求本地或远程是中继类型
            if (pair_info._local_candidate._type != CandidateInfo::AddressType::RELAY && 
                pair_info._remote_candidate._type != CandidateInfo::AddressType::RELAY) {
                DebugL << "relay only policy, skip pair: "
                    << "local(" << getAddressTypeStr(pair_info._local_candidate._type) << ") " 
                    << pair_info._local_candidate._addr._host << ":" << pair_info._local_candidate._addr._port
                    << " <-> "
                    << "remote(" << getAddressTypeStr(pair_info._remote_candidate._type) << ") " 
                    << pair_info._remote_candidate._addr._host << ":" << pair_info._remote_candidate._addr._port;
                return false;
            }
            break;
            
        case static_cast<int>(IceTransportPolicy::kP2POnly):
            // 仅支持P2P直连：要求本地和远程都不是中继类型
            if (pair_info._local_candidate._type == CandidateInfo::AddressType::RELAY ||
                pair_info._remote_candidate._type == CandidateInfo::AddressType::RELAY) {
                DebugL << "p2p only policy, skip pair: "
                    << "local(" << getAddressTypeStr(pair_info._local_candidate._type) << ") " 
                    << pair_info._local_candidate._addr._host << ":" << pair_info._local_candidate._addr._port
                    << " <-> "
                    << "remote(" << getAddressTypeStr(pair_info._remote_candidate._type) << ") " 
                    << pair_info._remote_candidate._addr._host << ":" << pair_info._remote_candidate._addr._port;
                return false;
            }
            break;
            
        case static_cast<int>(IceTransportPolicy::kAll):
        default:
            break;
    }
    
    return true;
}

////////////  IceServerInfo //////////////////////////
void IceServerInfo::parse(const std::string &url_in) {
    DebugL << url_in;

    _full_url = url_in;
    auto url = url_in;

    auto schema_pos = url.find(":");
    if (schema_pos == string::npos) {
        throw std::runtime_error(StrPrinter <<"fail to parse schema in url: " << url_in);
    }

    auto schema = url.substr(0, schema_pos);
    if (strcasecmp(schema.data(), "turns") == 0) {
        _schema = SchemaType::TURN;
        _secure = CandidateTuple::SecureType::SECURE;
    } else if (strcasecmp(schema.data(), "turn") == 0) {
        _schema = SchemaType::TURN;
        _secure = CandidateTuple::SecureType::NOT_SECURE;
    } else if (strcasecmp(schema.data(), "stuns") == 0) {
        _schema = SchemaType::STUN;
        _secure = CandidateTuple::SecureType::SECURE;
    } else if (strcasecmp(schema.data(), "stun") == 0) {
        _schema = SchemaType::STUN;
        _secure = CandidateTuple::SecureType::NOT_SECURE;
    } else {
        throw std::runtime_error(StrPrinter <<"not support schema: " << schema);
    }

    // 解析了用户名密码之后再解析?参数，防止密码中的?被判为参数分隔符
    auto pos = url.find("?");
    if (pos != string::npos) {
        _param_strs = url.substr(pos + 1);
        url.erase(pos);
    }

    auto host =  url.substr(schema_pos + 1, pos);
    mediakit::splitUrl(host, _addr._host, _addr._port);

    auto params = mediakit::Parser::parseArgs(_param_strs);
    if (params.find("transport") != params.end()) {
        auto transport = params["transport"];
        if (strcasecmp(transport.data(), "udp") == 0) {
            _transport = CandidateTuple::TransportType::UDP;
        } else if (strcasecmp(transport.data(), "tcp") == 0) {
            _transport = CandidateTuple::TransportType::TCP;
        } else {
            throw std::runtime_error(StrPrinter <<"not support transport: " << transport);
        }
    } else {
        _transport = CandidateTuple::TransportType::UDP;
    }
    return;
}

////////////  IceTransport //////////////////////////

// TODO 如果是需要copy的变量，那么改成值传递并std::move拷贝
IceTransport::IceTransport(Listener* listener, const std::string& ufrag, const std::string& password, const EventPoller::Ptr &poller)
: _poller(poller), _listener(listener), _ufrag(ufrag), _password(password) {
    TraceL;
    _identifier = makeRandStr(32);
    _request_handlers.emplace(std::make_pair(StunPacket::Class::REQUEST, StunPacket::Method::BINDING), 
                              std::bind(&IceTransport::handleBindingRequest, this, placeholders::_1, placeholders::_2));
}

void IceTransport::initialize() {
    weak_ptr<IceTransport> weak_self = static_pointer_cast<IceTransport>(shared_from_this());
    _retry_timer = std::make_shared<Timer>(
        0.1f, [weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return false;
            }
            strong_self->checkRequestTimeouts();
            return true;
        }, getPoller()
    );
}

// TODO pair是否改成引用传递更好？
void IceTransport::sendSocketData(toolkit::Buffer::Ptr buf, Pair::Ptr pair, bool flush) {
    return sendSocketData_l(buf, pair, flush);
}

// TODO pair是否改成引用传递更好？
void IceTransport::sendSocketData_l(toolkit::Buffer::Ptr buf, Pair::Ptr pair, bool flush) {
    // DebugL;
    if (pair == nullptr) {
        throw std::invalid_argument(StrPrinter << "pair should not be nullptr");
    }

    // 一次性发送一帧的rtp数据，提高网络io性能  [AUTO-TRANSLATED:fbab421e]
    // Send one frame of rtp data at a time to improve network io performance
    if (pair->_socket->getSock()->sockType() == SockNum::Sock_TCP) {
        // 增加tcp两字节头  [AUTO-TRANSLATED:62159f79]
        // Add two-byte header to tcp
        auto len = buf->size();
        char tcp_len[2] = { 0 };
        tcp_len[0] = (len >> 8) & 0xff;
        tcp_len[1] = len & 0xff;
        pair->_socket->SockSender::send(tcp_len, 2);
    }

    sockaddr_storage peer_addr;
    pair->get_peer_addr(peer_addr);

#if 0
    TraceL << "send data_len=" << buf->size()
           << ", " << pair->get_local_ip() << ":" << pair->get_local_port() 
           << " -> " << pair->get_peer_ip() << ":" << pair->get_peer_port() 
           << " peer_addr:  " << SockUtil::inet_ntoa((const struct sockaddr*)&peer_addr) << ":" 
        << SockUtil::inet_port((const struct sockaddr*)&peer_addr);

    TraceL << "data: " << hexdump(buf->data(), buf->size());
#endif
 

    auto addr_len = SockUtil::get_sock_len((const struct sockaddr*)&peer_addr);
    pair->_socket->sendto(std::move(buf), (struct sockaddr*)&peer_addr, addr_len);
    if (flush) {
        pair->_socket->flushAll();
    }
}

// TODO pair是否改成引用传递更好？
bool IceTransport::processSocketData(const uint8_t* data, size_t len, Pair::Ptr pair) {
#if 0
    TraceL << pair->get_local_ip() << ":" << pair->get_local_port() << " <- " << pair->get_peer_ip() << ":" << pair->get_peer_port() << " data len: " << len;
#endif
    sockaddr_storage relay_peer_addr;

#if 0
     if (pair->get_realyed_addr(relay_peer_addr)) {
         TraceL << "data relay from peer " << SockUtil::inet_ntoa((const struct sockaddr *)&relay_peer_addr) << ":"
                << SockUtil::inet_port((const struct sockaddr *)&relay_peer_addr);
     }
#endif
    auto packet = RTC::StunPacket::parse((const uint8_t *)data, len);
    if (!packet) {
        return processChannelData(data, len, pair);
    }
    processStunPacket(packet, pair);
    return true;
}

// TODO packet, pair是否改成引用传递更好？
void IceTransport::processStunPacket(const StunPacket::Ptr packet, Pair::Ptr pair) {
#if 0
    TraceL << "recv packet calss: " << packet->getClassStr() << ", method: " << packet->getMethodStr()
           << ", transaction_id: " << hexdump(packet->getTransactionId().data(), packet->getTransactionId().size());
#endif
    if ((packet->getClass() == StunPacket::Class::REQUEST) || (packet->getClass() == StunPacket::Class::INDICATION)) {
        processRequest(packet, pair);
    } else {
        processResponse(packet, pair);
    }
}

// TODO packet, pair是否改成引用传递更好？
StunPacket::Authentication IceTransport::checkRequestAuthentication(const StunPacket::Ptr packet, Pair::Ptr pair) {
    if (packet->getClass() == RTC::StunPacket::Class::INDICATION) {
        return RTC::StunPacket::Authentication::OK;
    }
#if 0
    DebugL << "_ufrag: "  << _ufrag << ", _password: "  << _password;
#endif
    // Check authentication.
    auto ret = packet->checkAuthentication(_ufrag, _password);
    if (ret != RTC::StunPacket::Authentication::OK) {
        sendUnauthorizedResponse(packet, pair);
    }
    return ret;
}

// TODO packet, pair是否改成引用传递更好？
StunPacket::Authentication IceTransport::checkResponseAuthentication(const StunPacket::Ptr request, const StunPacket::Ptr packet, Pair::Ptr pair) {
    if (!packet->hasAttribute(StunAttribute::Type::FINGERPRINT)) {
        sendUnauthorizedResponse(packet, pair);
        return  RTC::StunPacket::Authentication::UNAUTHORIZED;
    }
#if 0
    DebugL << "peer_ufrag: "  << request->getPeerUfrag() << ", peer_password: "  << request->getPeerPassword();
#endif
    // Check authentication.
    auto ret = packet->checkAuthentication(request->getPeerUfrag(), request->getPeerPassword());
    if (ret != RTC::StunPacket::Authentication::OK) {
        sendUnauthorizedResponse(packet, pair);
    }
    return ret;
}

// TODO packet, pair是否改成引用传递更好？
void IceTransport::processResponse(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL;

    auto it = _response_handlers.find(packet->getTransactionId().data());
    if (it == _response_handlers.end()) {
        WarnL << " not support Stun transaction_id ignore: " << hexdump(packet->getTransactionId().data(), packet->getTransactionId().size());
        return;
    }

    auto request = it->second._request;
    auto handle = it->second._handler;
    // 收到响应后立即清理请求信息
    _response_handlers.erase(packet->getTransactionId().data());

    if (packet->getClass() == StunPacket::Class::ERROR_RESPONSE) {
        if (StunAttrErrorCode::Code::Unauthorized == packet->getErrorCode()) {
            return processUnauthorizedResponse(packet, request, pair, handle);
        }
        return;
    }

    if (RTC::StunPacket::Authentication::OK != checkResponseAuthentication(request, packet, pair)) {
        WarnL << " checkRequestAuthentication fail, Method: " << packet->getMethodStr() << ", Class: " << packet->getClassStr();
        return;
    }

    handle(packet, pair);
    return;
}

// TODO pair是否改成引用传递更好？
bool IceTransport::processChannelData(const uint8_t* data, size_t len, Pair::Ptr pair) {
    // DebugL;

    // 检查数据长度是否足够
    if (len < 4) {
        WarnL << "Received data too short to be a valid STUN or ChannelData message";
        return false;
    }

    // 检查是否是ChannelData消息
    // ChannelData消息的前两个字节是通道号，范围是0x4000-0x7FFF
    uint16_t first_two_bytes;
    memcpy(&first_two_bytes, data, 2);
    first_two_bytes = ntohs(first_two_bytes);

    if (first_two_bytes < 0x4000 || first_two_bytes > 0x7FFF) {
        return false;
    }

    // 这是一个ChannelData消息
    uint16_t channel_number = first_two_bytes;
    uint16_t data_length;
    memcpy(&data_length, data + 2, 2);
    data_length = ntohs(data_length);

    // 检查数据长度是否合法
    if (len < 4 + data_length) {
        WarnL << "ChannelData message truncated, len: " << len << ", data_length: " << data_length;
        return false;
    }

    handleChannelData(channel_number, (const char*)(data + 4), data_length, pair);
    return true;
}

// TODO response, request, pair, handler是否改成引用传递更好？
void IceTransport::processUnauthorizedResponse(const StunPacket::Ptr response, StunPacket::Ptr request, Pair::Ptr pair, MsgHandler handler) {
    // TraceL;
    auto attr_nonce = dynamic_pointer_cast<StunAttrNonce>(response->getAttribute(StunAttribute::Type::NONCE));
    auto attr_realm = dynamic_pointer_cast<StunAttrRealm>(response->getAttribute(StunAttribute::Type::REALM));
    if (!attr_nonce || !attr_realm) {
        return;
    }

    request->refreshTransactionId();
    request->addAttribute(attr_nonce);
    request->addAttribute(attr_realm);
    request->setNeedMessageIntegrity(true);
    sendRequest(request, pair, handler);
}

// TODO packet, pair, 是否改成引用传递更好？
void IceTransport::processRequest(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL;
    if (RTC::StunPacket::Authentication::OK != checkRequestAuthentication(packet, pair)) {
        WarnL << " checkRequestAuthentication fail, Method: " << packet->getMethodStr() << ", Class: " << packet->getClassStr();
        return;
    }

    auto it = _request_handlers.find(std::make_pair(packet->getClass(), packet->getMethod()));
    if (it == _request_handlers.end()) {
        WarnL << " not support Stun Class: "<< packet->getClassStr() << ", Stun Method: " << packet->getMethodStr() << ", ignore";
        return;
    }

    return (it->second)(packet, pair);
}

// TODO packet, pair, 是否改成引用传递更好？
void IceTransport::handleBindingRequest(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL;

    auto response = packet->createSuccessResponse();
    response->setUfrag(_ufrag);
    response->setPassword(_password);

    sockaddr_storage peer_addr;
    if (!pair->get_realyed_addr(peer_addr)) {
        pair->get_peer_addr(peer_addr);
    }

    // Add XOR-MAPPED-ADDRESS.
    auto attr_xor_mapped_address = std::make_shared<StunAttrXorMappedAddress>(response->getTransactionId());
    attr_xor_mapped_address->setAddr(peer_addr);
    response->addAttribute(std::move(attr_xor_mapped_address));

    // Send back.
    sendPacket(response, pair);
}

// TODO pair, 是否改成引用传递更好？
void IceTransport::sendChannelData(uint16_t channel_number, const Buffer::Ptr& buffer, Pair::Ptr pair) {
    // TraceL;
 
    // ChannelData不是STUN消息，需要单独实现
    // ChannelData格式：2字节Channel Number + 2字节数据长度 + 数据内容

    // 分配缓冲区：头部4字节 + 数据长度
    auto data_len = buffer->size();
    size_t total_len = 4 + data_len;
    auto channel_data = toolkit::BufferRaw::create(total_len);

    // 设置Channel Number (前两字节，网络字节序)
    uint16_t net_channel_number = htons(channel_number);
    memcpy(channel_data->data(), &net_channel_number, 2);

    // 设置数据长度 (中间两字节，网络字节序)
    uint16_t net_data_len = htons(data_len);
    memcpy(channel_data->data() + 2, &net_data_len, 2);

    // 拷贝数据
    memcpy(channel_data->data() + 4, buffer->data(), data_len);
    channel_data->setSize(total_len);

#if 0
    TraceL << "send channel data: channel_number=" << channel_number
           << ", data_len=" << data_len
           << ", " << pair->get_local_ip() << ":" << pair->get_local_port()
           << " -> " << pair->get_peer_ip() << ":" << pair->get_peer_port();
    TraceL << "data: " << hexdump(buffer->data(), buffer->size());
#endif

    sendSocketData(channel_data, pair);
}

// TODO packet, pair, 是否改成引用传递更好？
void IceTransport::sendUnauthorizedResponse(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL;

    auto response = packet->createErrorResponse(StunAttrErrorCode::Code::Unauthorized);
    sendPacket(response, pair);
}

// TODO packet, pair, 是否改成引用传递更好？
void IceTransport::sendErrorResponse(const StunPacket::Ptr packet, Pair::Ptr pair, StunAttrErrorCode::Code errorCode) {
    // TraceL;
    auto response = packet->createErrorResponse(errorCode);
    sendPacket(response, pair);
}

// TODO packet, pair, handler 是否改成引用传递更好？
void IceTransport::sendRequest(const StunPacket::Ptr packet, Pair::Ptr pair, MsgHandler handler) {
    // TraceL;
    // 使用新的RequestInfo结构存储请求信息
    _response_handlers.emplace(packet->getTransactionId().data(), RequestInfo(packet, handler, pair));
    sendPacket(packet, pair);
}

// TODO packet, pair 是否改成引用传递更好？
void IceTransport::sendPacket(const StunPacket::Ptr packet, Pair::Ptr pair) {
#if 0
    TraceL << "send packet calss: " << packet->getClassStr() << ", method: " << packet->getMethodStr()
           << ", transaction_id: " << hexdump(packet->getTransactionId().data(), packet->getTransactionId().size())
           << ", " << pair->get_local_ip() <<":" << pair->get_local_port() << " -> " << pair->get_peer_ip() << ":" << pair->get_peer_port();
#endif
    packet->serialize();
    toolkit::Buffer::Ptr buffer = std::static_pointer_cast<toolkit::Buffer>(packet);
    sendSocketData(buffer, pair);
    return;
}

bool IceTransport::hasPermission(const sockaddr_storage& addr) {
    auto it = _permissions.find(addr);
    if (it == _permissions.end()) {
        return false;
    }

    // 权限有效期为5分钟
    uint64_t now = toolkit::getCurrentMillisecond();
    if (now - it->second > 5 * 60 * 1000) {
        DebugL << "permissions over time, ip:" 
            << SockUtil::inet_ntoa((const struct sockaddr*)&addr) << ", port: " << SockUtil::inet_port((const struct sockaddr*)&addr);
        _permissions.erase(it);
        return false;
    }
 
    return true;
}

void IceTransport::addPermission(const sockaddr_storage& addr) {
    _permissions[addr] = toolkit::getCurrentMillisecond();
}

bool IceTransport::hasChannelBind(uint16_t channel_number) {
    return _channel_bindings.find(channel_number) != _channel_bindings.end();
}

bool IceTransport::hasChannelBind(const sockaddr_storage& addr, uint16_t& channel_number) {
    for (const auto& binding : _channel_bindings) {
        if (SockUtil::is_same_addr(reinterpret_cast<const struct sockaddr*>(&binding.second),
                                   reinterpret_cast<const struct sockaddr*>(&addr))) {
            channel_number = binding.first;
            return true;
        }
    }
    return false;
}

void IceTransport::addChannelBind(uint16_t channel_number, const sockaddr_storage& addr) {
    _channel_bindings[channel_number] = addr;
    _channel_binding_times[channel_number] = toolkit::getCurrentMillisecond();
}

SocketHelper::Ptr IceTransport::createSocket(CandidateTuple::TransportType type, const std::string &peer_host, uint16_t peer_port, const std::string &local_ip, uint16_t local_port) {
    if (type == CandidateTuple::TransportType::UDP) {
        return createUdpSocket(peer_host, peer_port, local_ip, local_port);
    } else {
        throw std::invalid_argument(StrPrinter << "not support transport type: TCP");
    }
}

SocketHelper::Ptr IceTransport::createUdpSocket(const std::string &peer_host, uint16_t peer_port, const std::string &local_ip, uint16_t local_port) {
    auto socket = std::make_shared<UdpClient>(getPoller());

    weak_ptr<IceTransport> weak_self = static_pointer_cast<IceTransport>(shared_from_this());
    socket->setOnRecvFrom([weak_self, socket](const Buffer::Ptr &buffer, struct sockaddr *addr, int addr_len){
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        auto peer_host = SockUtil::inet_ntoa(addr);
        auto peer_port = SockUtil::inet_port(addr);
        auto pair = std::make_shared<Pair>(socket, peer_host, peer_port);
        strong_self->_listener->onIceTransportRecvData(buffer, pair);
    });

    socket->setOnError([weak_self](const SockException &err) {
        WarnL << err;
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
    });

    socket->setNetAdapter(local_ip);
    socket->startConnect(peer_host, peer_port, local_port);

    return socket;
}


////////////  IceServer //////////////////////////

#ifndef UNUSED
#define UNUSED(x) ((void)(x))
#endif
template <int type>
class PortManager : public std::enable_shared_from_this<PortManager<type>> {
public:
    PortManager() = default;

    static PortManager &Instance() {
        static auto instance = std::make_shared<PortManager>();
        return *instance;
    }

    void addListenConfigRelaod(){
        weak_ptr<PortManager> weak_self = this->shared_from_this();

        static auto func = [weak_self](const string &str, int index) {
            uint16_t port[] = { 49152, 65535 };
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return port[index];
            }
            sscanf(str.data(), "%" SCNu16 "-%" SCNu16, port, port + 1);
            strong_self->setRange(port[0], port[1]);
            return port[index];
        };


        GET_CONFIG_FUNC(uint16_t, dummy_min_port, kPortRange, [](const string &str) { return func(str, 0); });
        GET_CONFIG_FUNC(uint16_t, dummy_max_port, kPortRange, [](const string &str) { return func(str, 1); });
        UNUSED(dummy_min_port);
        UNUSED(dummy_max_port);
    }

    std::shared_ptr<uint16_t> getSinglePort() {
        lock_guard<recursive_mutex> lck(_pool_mtx);
        if (_port_pool.empty()) {
            return nullptr;
        }

        auto pos = _port_pool.front();
        _port_pool.pop_front();
        InfoL << "got port from pool:" << pos;

        weak_ptr<PortManager> weak_self = this->shared_from_this();
        std::shared_ptr<uint16_t> ret(new uint16_t(pos), [weak_self, pos](uint16_t *ptr) {
            delete ptr;
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }

            lock_guard<recursive_mutex> lck(strong_self->_pool_mtx);
            if (pos >= strong_self->_min_port && pos < strong_self->_max_port) {
                InfoL << "return port:" << pos << " to pool";
                // 回收端口号
                strong_self->_port_pool.emplace_back(pos);
            } else {
                InfoL << "port not in port range[" << strong_self->_min_port << "-" << strong_self->_max_port 
                      << "] any more, not return port:" << pos << " to pool";
                // 端口范围修改过，该端口不在范围内了，不回收端口号
            }
        });
 
        return ret;
    }

private:
    void setRange(uint16_t min_port, uint16_t max_port) {
        assert(max_port >= min_port + 36 - 1);
        lock_guard<recursive_mutex> lck(_pool_mtx);
        //端口范围未改变，不用处理
        if (min_port == _min_port && max_port == _max_port) {
            return;
        }

        InfoL << "setPortRange from [" << _min_port << "-" << _max_port << "] to [" << min_port << "-" << max_port << "]";

        // 修改：直接使用端口值，不再除以2
        uint16_t start_pos = min_port;
        uint16_t end_pos = max_port;
        std::mt19937 rng(std::random_device {}());

        //新指定的端口范围和原端口范围不交集,直接清除并重新增加
        if (max_port <= _min_port || min_port >= _max_port) {
            _port_pool.clear();
        } else {
 
            //存在交集，先把交集范围内还未被分配的端口保留
            deque<uint16_t> port_pool;
            for (; !_port_pool.empty(); _port_pool.pop_front()) {
                auto pos = _port_pool.front();

                if (pos >= start_pos && pos < end_pos) {
                    port_pool.emplace_back(pos);
                }
            }
            _port_pool.swap(port_pool);

            if (_min_port <= min_port && _max_port < max_port) {
                // <_min_port|--------|********************|_max_port>
                //           <min_port|********************|++++++++|max_port|

                start_pos = _max_port + 1;
                end_pos = max_port;

            } else if (_max_port >= max_port && _min_port > min_port) {
                // <min_port|++++++++|********************|max_port>
                //         <_min_port|********************|--------|_max_port|

                start_pos = min_port;
                end_pos = _min_port;
            } else if (min_port < _min_port && max_port > _max_port) {
                // <min_port|++++++++|********************|++++++++|max_port|
                //         <_min_port|********************|_max_port|

                start_pos = min_port;
                end_pos = _min_port;

                auto it = _port_pool.begin();
                while (start_pos < end_pos) {
                    // 随机端口排序，防止重启后导致分配的端口重复
                    _port_pool.insert(it, start_pos++);
                    it = _port_pool.begin() + (rng() % (1 + _port_pool.size()));
                }

                start_pos = _max_port + 1;
                end_pos = max_port;

            } else {
                //           <min_port|********************|max_port|
                // <_min_port|--------|********************|--------|_max_port|
                end_pos = start_pos;
            }
        }

        auto it = _port_pool.begin();
        while (start_pos < end_pos) {
            // 随机端口排序，防止重启后导致分配的端口重复
            _port_pool.insert(it, start_pos++);
            it = _port_pool.begin() + (rng() % (1 + _port_pool.size()));
        }

        InfoL << "now idle port num: " << _port_pool.size();

        _min_port = min_port;
        _max_port = max_port;
    }

private:
    //左闭右开
    uint16_t _min_port = 0;
    uint16_t _max_port = 0;
    recursive_mutex _pool_mtx;
    deque<uint16_t> _port_pool; // 修改：从_port_pair_pool改为_port_pool
};

//注册端口管理监听配置重载
onceToken PortManager_token([](){
    PortManager<0>::Instance().addListenConfigRelaod();
    PortManager<1>::Instance().addListenConfigRelaod();
});

std::unordered_map<sockaddr_storage /*peer ip:port*/, IceServer::WeakPtr, toolkit::SockUtil::SockAddrHash, toolkit::SockUtil::SockAddrEqual> _realyed_session;

IceServer::IceServer(Listener* listener, const std::string& ufrag, const std::string& password, const toolkit::EventPoller::Ptr &poller) 
    : IceTransport(listener, ufrag, password, poller) {
    DebugL;

    GET_CONFIG(bool, enable_turn, Rtc::kEnableTurn);
    if (enable_turn) {
        _request_handlers.emplace(std::make_pair(StunPacket::Class::REQUEST, StunPacket::Method::ALLOCATE), 
                                  std::bind(&IceServer::handleAllocateRequest, this, placeholders::_1, placeholders::_2));
        _request_handlers.emplace(std::make_pair(StunPacket::Class::REQUEST, StunPacket::Method::REFRESH), 
                                  std::bind(&IceServer::handleRefreshRequest, this, placeholders::_1, placeholders::_2));
        _request_handlers.emplace(std::make_pair(StunPacket::Class::REQUEST, StunPacket::Method::CREATEPERMISSION), 
                                  std::bind(&IceServer::handleCreatePermissionRequest, this, placeholders::_1, placeholders::_2));
        _request_handlers.emplace(std::make_pair(StunPacket::Class::REQUEST, StunPacket::Method::CHANNELBIND), 
                                  std::bind(&IceServer::handleChannelbindRequest, this, placeholders::_1, placeholders::_2));
        _request_handlers.emplace(std::make_pair(StunPacket::Class::INDICATION, StunPacket::Method::SEND), 
                                  std::bind(&IceServer::handleSendIndication, this, placeholders::_1, placeholders::_2));
    }

}

void IceServer::initialize() {
    IceTransport::initialize();
}

// TODO pair使用引用？
bool IceServer::processSocketData(const uint8_t* data, size_t len, Pair::Ptr pair) {
    if (!_session_pair) {
        _session_pair = pair;
    }
    return IceTransport::processSocketData(data, len, pair);
}

// TODO pair使用引用？
void IceServer::processRealyPacket(const Buffer::Ptr &buffer, Pair::Ptr pair) {
    // TraceL << pair->get_local_ip() <<":" << pair->get_local_port() << " <- " << pair->get_peer_ip() << ":" << pair->get_peer_port();

    sockaddr_storage peer_addr;
    pair->get_peer_addr(peer_addr);

    if (!hasPermission(peer_addr)) {
        WarnL << "No permission exists for peer: " << pair->get_peer_ip() << ":" << pair->get_peer_port();
        return;
    }

    auto forward_pair = std::make_shared<Pair>(_session_pair->_socket, pair->_socket->get_peer_ip(), pair->_socket->get_peer_port());
    uint16_t channel_number;
    if (hasChannelBind(peer_addr, channel_number)) {
        sendChannelData(channel_number, buffer, forward_pair);
    } else {
        sendDataIndication(peer_addr, buffer, forward_pair);
    }
}

// TODO packet， pair使用引用？
void IceServer::handleAllocateRequest(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL;
    auto response = packet->createSuccessResponse();
    response->setUfrag(_ufrag);
    response->setPassword(_password);

    // Add XOR-MAPPED-ADDRESS.
    sockaddr_storage peer_addr;
    pair->get_peer_addr(peer_addr);

    auto attr_xor_mapped_address = std::make_shared<StunAttrXorMappedAddress>(response->getTransactionId());
    attr_xor_mapped_address->setAddr(peer_addr);
    response->addAttribute(std::move(attr_xor_mapped_address));

    // Add XOR-RELAYED-ADDRESS.
    auto socket = allocateRealyed(pair);
    sockaddr_storage relayed_addr = SockUtil::make_sockaddr(socket->get_local_ip().data(), socket->get_local_port());
    auto attr_xor_relayed_address = std::make_shared<StunAttrXorRelayedAddress>(response->getTransactionId());
    attr_xor_relayed_address->setAddr(relayed_addr);
    response->addAttribute(std::move(attr_xor_relayed_address));

    auto attr_lifetime = std::make_shared<StunAttrLifeTime>();
    attr_lifetime->setLifetime(600);
    response->addAttribute(std::move(attr_lifetime));
 
    sendPacket(response, pair);
}

// TODO packet， pair使用引用？
void IceServer::handleRefreshRequest(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL
}

void IceServer::handleCreatePermissionRequest(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL

    // 检查XOR-PEER-ADDRESS属性是否存在
    auto peer_addr = static_pointer_cast<StunAttrXorPeerAddress>(packet->getAttribute(StunAttribute::Type::XOR_PEER_ADDRESS));
    if (!peer_addr) {
        WarnL << "CreatePermission request missing XOR-PEER-ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
        return;
    }

    addPermission(peer_addr->getAddr());

    auto response = packet->createSuccessResponse();
    response->setUfrag(_ufrag);
    response->setPassword(_password);
    sendPacket(response, pair);
}

// TODO packet， pair使用引用？
void IceServer::handleChannelbindRequest(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL

    // 检查必要的属性
    auto channel_number = static_pointer_cast<StunAttrChannelNumber>(packet->getAttribute(StunAttribute::Type::CHANNEL_NUMBER));
    auto peer_addr = static_pointer_cast<StunAttrXorPeerAddress>(packet->getAttribute(StunAttribute::Type::XOR_PEER_ADDRESS));
    if (!channel_number || !peer_addr) {
        WarnL << "ChannelBind request missing required attributes";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
        return;
    }

    // 验证通道号是否在有效范围内 (0x4000-0x7FFF)
    uint16_t number = channel_number->getChannelNumber();
    if (number < 0x4000 || number > 0x7FFF) {
        WarnL << "Invalid channel number: " << number;
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
        return;
    }

    // 检查是否有对应peer地址的权限
    auto addr = peer_addr->getAddr();
    if (!hasPermission(addr)) {
        WarnL << "No permission exists for peer address";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::Forbidden);
        return;
    }

    // 添加或更新通道绑定
    addChannelBind(number, addr);

    auto response = packet->createSuccessResponse();
    response->setUfrag(_ufrag);
    response->setPassword(_password);
    sendPacket(response, pair);
}

// TODO packet， pair使用引用？
void IceServer::handleSendIndication(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL

    // 检查必要的属性
    auto peer_addr = static_pointer_cast<StunAttrXorPeerAddress>(packet->getAttribute(StunAttribute::Type::XOR_PEER_ADDRESS));
    auto data = static_pointer_cast<StunAttrData>(packet->getAttribute(StunAttribute::Type::DATA));

    if (!peer_addr || !data) {
        WarnL << "Send indication missing required attributes";
        return;
    }

    // 检查是否有对应peer地址的权限
    auto addr = peer_addr->getAddr();
    if (!hasPermission(addr)) {
        WarnL << "No permission exists for peer address";
        return;
    }

    auto buffer = data->getData();
    auto send_buffer = BufferRaw::create(buffer.size());
    send_buffer->assign(buffer.data(), buffer.size());
    return relayBackingData(send_buffer, pair, addr);
}

// TODO pair使用引用？
void IceServer::handleChannelData(uint16_t channel_number, const char* data, size_t len, Pair::Ptr pair) {
    // TraceL << "Received ChannelData message, channel number: " << channel_number;

    // 查找该通道号对应的目标地址
    auto it = _channel_bindings.find(channel_number);
    if (it == _channel_bindings.end()) {
        WarnL << "No binding found for channel number: " << channel_number;
        return;
    }
 
    // 获取目标地址
    sockaddr_storage peer_addr = it->second;
 
    // 创建一个新的缓冲区用于转发
    auto buffer = BufferRaw::create(len);
    buffer->assign(data, len);

    // 转发数据到目标地址
    return relayBackingData(buffer, pair, peer_addr);
}

// TODO packet, pair使用引用？
void IceServer::sendUnauthorizedResponse(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL

    if (packet->getMethod() == StunPacket::Method::ALLOCATE) {
        auto response = packet->createErrorResponse(StunAttrErrorCode::Code::Unauthorized);
        auto attr_nonce = std::make_shared<StunAttrNonce>();
        auto nonce = makeRandStr(80);
        _nonce_list.push_back(nonce);
        attr_nonce->setNonce(nonce);
        response->addAttribute(std::move(attr_nonce));

        auto attr_realm = std::make_shared<StunAttrRealm>(); 
        attr_realm->setRealm("ZLM"); //TODO: use config.ini
        response->addAttribute(std::move(attr_realm));
        sendPacket(response, pair);
        return;
    }

    return IceTransport::sendUnauthorizedResponse(packet, pair);
}

// TODO packet, pair使用引用？
StunPacket::Authentication IceServer::checkRequestAuthentication(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL

    //ICE SERVER 不对BINDGING请求校验
    if (packet->getMethod() == RTC::StunPacket::Method::BINDING) {
        return  RTC::StunPacket::Authentication::OK;
    }

    return IceTransport::checkRequestAuthentication(packet, pair);
}

// TODO pair使用引用？
void IceServer::sendDataIndication(const sockaddr_storage& peer_addr, const Buffer::Ptr& buffer, Pair::Ptr pair) {
    // TraceL

    auto packet = std::make_shared<DataIndicationPacket>();

    auto attr_peer_address = std::make_shared<StunAttrXorPeerAddress>(packet->getTransactionId());
    attr_peer_address->setAddr(peer_addr);
    packet->addAttribute(std::move(attr_peer_address));

    auto attr_data = std::make_shared<StunAttrData>();
    BufferLikeString data;
    data.assign(buffer->data(), buffer->size());
    attr_data->setData(data);
    packet->addAttribute(std::move(attr_data));

    sendPacket(packet, pair);
#if 0
    TraceL << "Forward UDP data as DataIndication,"
           << "from: " << pair->get_local_ip() << ":" << pair->get_local_port() << " to: " << pair->get_peer_ip() << ":" << pair->get_peer_port()
           << ", size: " << buffer->size();
#endif
}

// TODO pair使用引用？
SocketHelper::Ptr IceServer::allocateRealyed(Pair::Ptr pair) {
    // DebugL;

    // only support udp
    auto port = PortManager<0>::Instance().getSinglePort();

    GET_CONFIG_FUNC(std::vector<std::string>, extern_ips, Rtc::kExternIP, [](string str) {
        std::vector<std::string> ret;
        if (str.length()) {
            ret = split(str, ",");
        }
        translateIPFromEnv(ret);
        return ret;
    });

    std::string extern_ip;
    if (extern_ips.empty()) {
        extern_ip = SockUtil::get_local_ip();
    } else {
        extern_ip = extern_ips.front();
    }

    auto socket = createRealyedUdpSocket(pair->get_peer_ip(), pair->get_peer_port(), extern_ip, *port);
    auto realyed_pair = std::make_shared<Pair>(socket);
    auto peer_addr = SockUtil::make_sockaddr(pair->get_peer_ip().data(), pair->get_peer_port());
    _realyed_pairs.emplace(peer_addr, std::make_pair(port, realyed_pair));
    weak_ptr<IceServer> weak_self = static_pointer_cast<IceServer>(shared_from_this());
    _realyed_pairs.emplace(peer_addr, std::make_pair(port, realyed_pair));
    _realyed_session.emplace(peer_addr, weak_self);

    InfoL << "Alloc realyed pair: " << realyed_pair->get_local_ip() << ":" <<  realyed_pair->get_local_port()
          << " for peer pair: " << pair->get_peer_ip() << ":" << pair->get_peer_port();
    return socket;
}

// TODO peer_addr使用引用？
void IceServer::relayForwordingData(const toolkit::Buffer::Ptr& buffer, struct sockaddr_storage peer_addr) {
    TraceL;
    getPoller()->async([=]() {
        auto it = _realyed_pairs.find(peer_addr);
        if (it == _realyed_pairs.end()) {
#if 0
            //不是当前对象的转发,交给其他对象转发
            auto forword_it = _realyed_session.find(peer_addr);
            if (forword_it == _realyed_session.end()) {
                WarnL << "not relayed addr for peer addr: " 
                    << SockUtil::inet_ntoa((const struct sockaddr *)&peer_addr) << ":"
                    << SockUtil::inet_port((const struct sockaddr *)&peer_addr);
            }

            auto forword_session = forword_it->second.lock();
            if (!forword_session) {
                WarnL << "forword session for peer addr "
                    << SockUtil::inet_ntoa((const struct sockaddr *)&peer_addr) << ":"
                    << SockUtil::inet_port((const struct sockaddr *)&peer_addr)
                    << " is release";
                return;
            }

            if (getIdentifier() == forword_session->getIdentifier()) {
                //找到的会话就是当前会话，忽略
                return;
            }
            forword_session->relayForwordingData(buffer, peer_addr);
            return;
#else 
            WarnL << "not relayed addr for peer addr: " 
                << SockUtil::inet_ntoa((const struct sockaddr *)&peer_addr) << ":"
                << SockUtil::inet_port((const struct sockaddr *)&peer_addr);
#endif
        }

        sendSocketData(buffer, it->second.second);
#if 0
        TraceL << "Forwarded ChannelData to peer: "
               << SockUtil::inet_ntoa((struct sockaddr *)&peer_addr) << ":"
               << SockUtil::inet_port((struct sockaddr *)&peer_addr);
#endif
    });
}

// TODO pair, peer_addr使用引用？
void IceServer::relayBackingData(const toolkit::Buffer::Ptr& buffer, Pair::Ptr pair, struct sockaddr_storage peer_addr) {
    // TraceL;

    sockaddr_storage addr;
    pair->get_peer_addr(addr);

    auto it = _realyed_pairs.find(addr);
    if (it == _realyed_pairs.end()) {
        WarnL << "not relayed addr for peer addr: " 
            << SockUtil::inet_ntoa((const struct sockaddr *)&addr) << ":"
            << SockUtil::inet_port((const struct sockaddr *)&addr);
        return;
    }

    auto forward_pair = std::make_shared<Pair>(it->second.second->_socket,
        SockUtil::inet_ntoa((const struct sockaddr *)&peer_addr).data(), SockUtil::inet_port((const struct sockaddr *)&peer_addr));

    sendSocketData(buffer, forward_pair);
#if 0
    DebugL << "relay backing"
           << " from: " << forward_pair->get_local_ip() << ":" << forward_pair->get_local_port()
           << " to peer: " << forward_pair->get_peer_ip() << ":" << forward_pair->get_peer_port();
#endif
}

SocketHelper::Ptr IceServer::createRealyedUdpSocket(const std::string &peer_host, uint16_t peer_port, const std::string &local_ip, uint16_t local_port) {
    auto socket = std::make_shared<UdpClient>(getPoller());

    weak_ptr<IceServer> weak_self = static_pointer_cast<IceServer>(shared_from_this());
    socket->setOnRecvFrom([weak_self, socket](const Buffer::Ptr &buffer, struct sockaddr *addr, int addr_len) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        auto peer_host = SockUtil::inet_ntoa(addr);
        auto peer_port = SockUtil::inet_port(addr);
        auto pair = std::make_shared<Pair>(socket, peer_host, peer_port);
        strong_self->processRealyPacket(buffer, pair);
    });

    socket->setOnError([weak_self](const SockException &err) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
    });

    socket->setNetAdapter(local_ip);
    socket->startConnect(peer_host, peer_port, local_port);

    return socket;
}

////////////  IceAgent //////////////////////////

IceAgent::IceAgent(Listener* listener, Implementation implementation, Role role, const std::string& ufrag, const std::string& password, const toolkit::EventPoller::Ptr &poller) 
: IceTransport(listener, ufrag, password, poller), _implementation(implementation) ,_role(role) {
    DebugL;
    _tiebreaker = makeRandNum();
    // 创建定时器，每分钟检查一次权限和通道绑定是否需要刷新
    _refresh_timer = std::make_shared<Timer>(
        60.0f, [this]() {
            refreshPermissions();
            refreshChannelBindings();
            return true;
        }, getPoller()
    );
}

void IceAgent::initialize() {
    IceTransport::initialize();
}

void IceAgent::gatheringCandidate(CandidateTuple::Ptr candidate_tuple, bool gathering_rflx, bool gathering_realy) {
    // TraceL;

    auto interfaces = SockUtil::getInterfaceList();
    for (auto obj : interfaces) {
        if (obj["name"]  == "lo") {
            DebugL << "skip interace: " << obj["name"];
            continue;
        }

        try {
            CandidateInfo candidate;
            candidate._type = CandidateInfo::AddressType::HOST;
            candidate._addr._host = obj["ip"];
            candidate._base_addr._host = candidate._addr._host;
            candidate._ufrag = getUfrag();
            candidate._pwd = getPassword();

            auto socket = createSocket(candidate_tuple->_transport, candidate_tuple->_addr._host, candidate_tuple->_addr._port, obj["ip"]);
            _socket_candidate_manager.addHostSocket(socket);
            candidate._addr._port = socket->get_local_port();
            candidate._base_addr._port = candidate._addr._port;

            // TraceL << "gathering local candidate " << candidate._addr._host << ":" << candidate._addr._port 
            // << " from stun server " << candidate_tuple->_addr._host << ":" << candidate_tuple->_addr._port;

            auto pair = std::make_shared<Pair>(socket);
            onGatheringCandidate(pair, candidate);
            if (gathering_rflx) {
                gatheringSrflxCandidate(pair);
            }

            if (gathering_realy) {
                auto realy_socket = createSocket(candidate_tuple->_transport, candidate_tuple->_addr._host, candidate_tuple->_addr._port, obj["ip"]);
                _socket_candidate_manager.addRelaySocket(realy_socket);
                gatheringRealyCandidate(std::make_shared<Pair>(realy_socket));
            }
        } catch (std::exception &ex) {
            WarnL << ex.what();
        }
    }
    return;
}

// TODO candidate使用引用？
void IceAgent::connectivityCheck(CandidateInfo candidate) {
    TraceL;

    setState(IceAgent::State::Running);
    auto ret = _remote_candidates.emplace(candidate);
    if (ret.second) {
        for (auto socket: _socket_candidate_manager._host_sockets) {
            auto pair = std::make_shared<Pair>(socket, candidate._addr._host, candidate._addr._port);
            addToChecklist(pair, candidate);
        }

        if (_socket_candidate_manager._has_realyed_cnadidate) {
            localRealyedConnectivityCheck(candidate);
        }
    }

}

// TODO candidate使用引用？
void IceAgent::localRealyedConnectivityCheck(CandidateInfo candidate) {
    TraceL;
    for (auto socket: _socket_candidate_manager._relay_sockets) {
        auto local_realy_pair = std::make_shared<Pair>(socket, _ice_server->_addr._host, _ice_server->_addr._port);
        auto peer_addr = SockUtil::make_sockaddr(candidate._addr._host.data(), candidate._addr._port);
        sendCreatePermissionRequest(local_realy_pair, peer_addr);

        local_realy_pair->_realyed_addr = std::make_shared<sockaddr_storage>();
        memcpy(local_realy_pair->_realyed_addr.get(), &peer_addr, sizeof(peer_addr));
        addToChecklist(local_realy_pair, candidate);
    }
}

// TODO pair, candidate使用引用？
void IceAgent::nominated(Pair::Ptr pair, CandidateTuple candidate) {
    // TraceL;
    auto handler = std::bind(&IceAgent::handleNominatedResponse, this, placeholders::_1, placeholders::_2, candidate);
    sendBindRequest(pair, candidate, true, handler);
}

// TODO buffer, pair使用引用？
void IceAgent::sendSendIndication(const sockaddr_storage& peer_addr, toolkit::Buffer::Ptr buffer, Pair::Ptr pair) {
    // TraceL;
    auto packet = std::make_shared<SendIndicationPacket>();

    auto attr_peer_address = std::make_shared<StunAttrXorPeerAddress>(packet->getTransactionId());
    attr_peer_address->setAddr(peer_addr);
    packet->addAttribute(std::move(attr_peer_address));

    auto attr_data = std::make_shared<StunAttrData>();
    BufferLikeString data;
    data.assign(buffer->data(), buffer->size());
    attr_data->setData(data);
    packet->addAttribute(std::move(attr_data));

    sendPacket(packet, pair);
}

// TODO pair使用引用？
void IceAgent::gatheringSrflxCandidate(Pair::Ptr pair) {
    // TraceL;
    auto handle = std::bind(&IceAgent::handleGatheringCandidateResponse, this, placeholders::_1, placeholders::_2);
    sendBindRequest(pair, handle);
}

// TODO pair使用引用？
void IceAgent::gatheringRealyCandidate(Pair::Ptr pair) {
    // TraceL;
    sendAllocateRequest(pair);
}

// TODO pair, candidate使用引用？
void IceAgent::connectivityCheck(Pair::Ptr pair, CandidateTuple candidate) {
    // TraceL;
    auto handler = std::bind(&IceAgent::handleConnectivityCheckResponse, this, placeholders::_1, placeholders::_2, candidate);
    sendBindRequest(pair, candidate, false, handler);
}

// TODO pair 使用引用？
void IceAgent::tryTriggerredCheck(Pair::Ptr pair) {
    DebugL;
    //FIXME 暂不实现,因为当前实现基本收到candidate就会发起check
}

// TODO pair, handler 使用引用？
void IceAgent::sendBindRequest(Pair::Ptr pair, MsgHandler handler) {
    // TraceL;
    auto packet = std::make_shared<BindingPacket>();
    packet->setUfrag(_ufrag);
    packet->setPassword(_password);
    packet->setPeerUfrag(_ice_server->_ufrag);
    packet->setPeerPassword(_ice_server->_pwd);

    packet->setNeedFingerprint(false);
    packet->setNeedMessageIntegrity(false);
    sendRequest(packet, pair, handler);
    return;
}

// TODO pair, candidate, handler 使用引用？
void IceAgent::sendBindRequest(Pair::Ptr pair, CandidateTuple candidate, bool use_candidate, MsgHandler handler) {
    // TraceL;
    auto packet = std::make_shared<BindingPacket>();
    packet->setUfrag(_ufrag);
    packet->setPassword(_password);
    packet->setPeerUfrag(candidate._ufrag);
    packet->setPeerPassword(candidate._pwd);

    auto attr_username = std::make_shared<StunAttrUserName>();
    attr_username->setUsername(candidate._ufrag + ":" + _ufrag);
    packet->addAttribute(std::move(attr_username));

    if (getRole() == Role::Controlling) {
        auto attr_icecontrolling = std::make_shared<StunAttrIceControlling>();
        attr_icecontrolling->setTiebreaker(_tiebreaker);
        packet->addAttribute(std::move(attr_icecontrolling));
    } else {
        auto attr_icecontrolled = std::make_shared<StunAttrIceControlled>();
        attr_icecontrolled->setTiebreaker(_tiebreaker);
        packet->addAttribute(std::move(attr_icecontrolled));
    }

    if (use_candidate) {
        auto attr_use_candidate = std::make_shared<StunAttrUseCandidate>();
        packet->addAttribute(std::move(attr_use_candidate));
    }

    if (candidate._priority != 0) {
        auto attr_priority = std::make_shared<StunAttrPriority>();
        attr_priority->setPriority(candidate._priority);
        packet->addAttribute(std::move(attr_priority));
    }

    sendRequest(packet, pair, handler);
}

// TODO pair使用引用？
void IceAgent::sendAllocateRequest(Pair::Ptr pair) {
    // TraceL;
    auto packet = std::make_shared<AllocatePacket>();
    packet->setNeedMessageIntegrity(false);
    packet->setUfrag(_ufrag);
    packet->setPassword(_password);
    packet->setPeerUfrag(_ice_server->_ufrag);
    packet->setPeerPassword(_ice_server->_pwd);
    auto attr_requested_transport = std::make_shared<StunAttrRequestedTransport>();
    attr_requested_transport->setProtocol(StunAttrRequestedTransport::Protocol::UDP);
    packet->addAttribute(std::move(attr_requested_transport));

    auto handler = std::bind(&IceAgent::handleAllocateResponse, this, placeholders::_1,  placeholders::_2);
    sendRequest(packet, pair, handler);
}

// TODO pair使用引用？
void IceAgent::sendCreatePermissionRequest(Pair::Ptr pair, const sockaddr_storage& peer_addr) {
    // TraceL;

    addPermission(peer_addr);

    auto packet = std::make_shared<CreatePermissionPacket>();
    packet->setUfrag(_ufrag);
    packet->setPassword(_password);
    packet->setPeerUfrag(_ice_server->_ufrag);
    packet->setPeerPassword(_ice_server->_pwd);

    auto attr_username = std::make_shared<StunAttrUserName>();
    attr_username->setUsername(_ice_server->_ufrag);
    packet->addAttribute(std::move(attr_username));
    auto attr_peer_address = std::make_shared<StunAttrXorPeerAddress>(packet->getTransactionId());
    attr_peer_address->setAddr(peer_addr);
    packet->addAttribute(std::move(attr_peer_address));

    auto handler = std::bind(&IceAgent::handleCreatePermissionResponse, this, placeholders::_1, placeholders::_2, peer_addr);
    sendRequest(packet, pair, handler);
}

// TODO pair使用引用？
void IceAgent::sendChannelBindRequest(Pair::Ptr pair, uint16_t channel_number, const sockaddr_storage& peer_addr) {
    // TraceL;
    auto packet = std::make_shared<ChannelBindPacket>();
    packet->setUfrag(_ufrag);
    packet->setPassword(_password);
    packet->setPeerUfrag(_ice_server->_ufrag);
    packet->setPeerPassword(_ice_server->_pwd);

    auto attr_username = std::make_shared<StunAttrUserName>();
    attr_username->setUsername(_ice_server->_ufrag);
    packet->addAttribute(std::move(attr_username));

    auto attr_channel_number = std::make_shared<StunAttrChannelNumber>();
    attr_channel_number->setChannelNumber(channel_number);
    packet->addAttribute(std::move(attr_channel_number));

    auto attr_peer_address = std::make_shared<StunAttrXorPeerAddress>(packet->getTransactionId());
    attr_peer_address->setAddr(peer_addr);
    packet->addAttribute(std::move(attr_peer_address));

    auto handler = std::bind(&IceAgent::handleChannelBindResponse, this, placeholders::_1, placeholders::_2, channel_number, peer_addr);
    sendRequest(packet, pair, handler);
}

// TODO packet, pair使用引用？
void IceAgent::processRequest(const StunPacket::Ptr packet, Pair::Ptr pair) {
    static toolkit::onceToken token([this]() {
        _request_handlers.emplace(std::make_pair(StunPacket::Class::INDICATION, StunPacket::Method::DATA), std::bind(&IceAgent::handleDataIndication, this, placeholders::_1, placeholders::_2));
    });
    return IceTransport::processRequest(packet, pair);
}

// TODO packet, pair使用引用？
void IceAgent::handleBindingRequest(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL;
    auto controlling = static_pointer_cast<StunAttrIceControlling>(packet->getAttribute(StunAttribute::Type::ICE_CONTROLLING));
    auto controlled = static_pointer_cast<StunAttrIceControlled>(packet->getAttribute(StunAttribute::Type::ICE_CONTROLLED));
    auto priority = static_pointer_cast<StunAttrPriority>(packet->getAttribute(StunAttribute::Type::PRIORITY));

    //角色冲突
    if (controlling && getRole() == Role::Controlling) {
        if (controlling->getTiebreaker() > _tiebreaker) {
            setRole(Role::Controlled);
            InfoL << "rule conflict, election fail, change role to controlled";
        } else {
            InfoL << "rule conflict, election success in controlling, send error";
            auto response = packet->createErrorResponse(StunAttrErrorCode::Code::RoleConflict);
            response->setUfrag(_ufrag);
            response->setPassword(_password);
            auto attr_icecontrolling = std::make_shared<StunAttrIceControlling>();
            attr_icecontrolling->setTiebreaker(_tiebreaker);
            response->addAttribute(std::move(attr_icecontrolling));
            return sendPacket(response, pair);
        }
    } else if (controlled && getRole() == Role::Controlled) {
        if (controlled->getTiebreaker() > _tiebreaker) {
            setRole(Role::Controlling);
            InfoL << "rule conflict, election fail, change role to controlling";
        } else {
            InfoL << "rule conflict, election success in controlled, send error";
            auto response = packet->createErrorResponse(StunAttrErrorCode::Code::RoleConflict);
            response->setUfrag(_ufrag);
            response->setPassword(_password);
            auto attr_icecontrolled = std::make_shared<StunAttrIceControlled>();
            attr_icecontrolled->setTiebreaker(_tiebreaker);
            response->addAttribute(std::move(attr_icecontrolled));
            return sendPacket(response, pair);
        }
    }

    auto response = packet->createSuccessResponse();
    response->setUfrag(_ufrag);
    response->setPassword(_password);

    sockaddr_storage peer_addr;
    if (!pair->get_realyed_addr(peer_addr)) {
        pair->get_peer_addr(peer_addr);
    }

    // Add XOR-MAPPED-ADDRESS.
    auto attr_xor_mapped_address = std::make_shared<StunAttrXorMappedAddress>(response->getTransactionId());
    attr_xor_mapped_address->setAddr(peer_addr);
    response->addAttribute(std::move(attr_xor_mapped_address));

    if (packet->hasAttribute(StunAttribute::Type::USE_CANDIDATE)) {
        if (getRole() == Role::Controlled) {
            _nominated_response = response;
            onCompleted(pair);
        }
    } else {
        sendPacket(response, pair);
        tryTriggerredCheck(pair);
    }

}

// TODO packet, pair使用引用？
void IceAgent::handleGatheringCandidateResponse(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL; 

    if (RTC::StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "fail, get response class: " << packet->getClassStr();
        return;
    }

    auto srflx = static_pointer_cast<StunAttrXorMappedAddress>(packet->getAttribute(StunAttribute::Type::XOR_MAPPED_ADDRESS));
    if (!srflx) {
        WarnL << "Binding request missing XOR_MAPPED_ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
    }

    CandidateInfo candidate;
    candidate._type = CandidateInfo::AddressType::SRFLX;
    candidate._addr._host = srflx->getAddrString();
    candidate._addr._port = srflx->getPort();
    candidate._base_addr._host = pair->get_local_ip();
    candidate._base_addr._port = pair->get_local_port();
    candidate._ufrag = getUfrag();
    candidate._pwd = getPassword();
    onGatheringCandidate(pair, candidate);
}

// TODO packet, pair, candidate使用引用？
void IceAgent::handleConnectivityCheckResponse(const StunPacket::Ptr packet, Pair::Ptr pair, CandidateTuple candidate) {
    // TraceL; 

    if (RTC::StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "fail, get response class: " << packet->getClassStr();
        if (packet->getErrorCode() == StunAttrErrorCode::Code::RoleConflict) {
            InfoL << "process Role Conflict";

            auto controlling = static_pointer_cast<StunAttrIceControlling>(packet->getAttribute(StunAttribute::Type::ICE_CONTROLLING));
            auto controlled = static_pointer_cast<StunAttrIceControlled>(packet->getAttribute(StunAttribute::Type::ICE_CONTROLLED));
            //角色冲突
            if (controlling && getRole() == Role::Controlling) {
                if (controlling->getTiebreaker() > _tiebreaker) {
                    InfoL << "rule conflict, election fail, change role to controlled";
                    setRole(Role::Controlled);
                } else {
                    InfoL << "rule conflict, election success in controlling, skip";
                    return;
                }
            } else if (controlled && getRole() == Role::Controlled) {
                if (controlled->getTiebreaker() > _tiebreaker) {
                    InfoL << "rule conflict, election fail, change role to controlling";
                    setRole(Role::Controlling);
                } else {
                    InfoL << "rule conflict, election success in controlled, skip";
                    return;
                }
            }
            connectivityCheck(pair, candidate);
        }
        return;
    }

    auto srflx = static_pointer_cast<StunAttrXorMappedAddress>(packet->getAttribute(StunAttribute::Type::XOR_MAPPED_ADDRESS));
    if (!srflx) {
        WarnL << "Binding request missing XOR_MAPPED_ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
    }

    CandidateInfo preflx_candidate;
    preflx_candidate._type = CandidateInfo::AddressType::PRFLX;
    preflx_candidate._addr._host = srflx->getAddrString();
    preflx_candidate._addr._port = srflx->getPort();
    preflx_candidate._base_addr._host = pair->get_local_ip();
    preflx_candidate._base_addr._port = pair->get_local_port();
    preflx_candidate._ufrag = getUfrag();
    preflx_candidate._pwd = getPassword();
    onGatheringCandidate(pair, preflx_candidate);

    DebugL << "get candidate type preflx: " << srflx->getAddrString() << ":" << srflx->getPort();
    onConnected(pair);
}

// TODO packet, pair, candidate使用引用？
void IceAgent::handleNominatedResponse(const StunPacket::Ptr packet, Pair::Ptr pair, CandidateTuple candidate) {
    // TraceL; 

    if (RTC::StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "fail, get response class: " << packet->getClassStr();
        if (packet->getErrorCode() == StunAttrErrorCode::Code::RoleConflict) {
            //角色冲突
            InfoL << "process Role Conflict";
            auto controlling = static_pointer_cast<StunAttrIceControlling>(packet->getAttribute(StunAttribute::Type::ICE_CONTROLLING));
            auto controlled = static_pointer_cast<StunAttrIceControlled>(packet->getAttribute(StunAttribute::Type::ICE_CONTROLLED));
            if (controlling && getRole() == Role::Controlling) {
                if (controlling->getTiebreaker() > _tiebreaker) {
                    InfoL << "rule conflict, election fail, change role to controlled";
                    setRole(Role::Controlled);
                    return;
                } else {
                    InfoL << "rule conflict, election success in controlling, skip";
                    return;
                }
            } else if (controlled && getRole() == Role::Controlled) {
                if (controlled->getTiebreaker() > _tiebreaker) {
                    InfoL << "rule conflict, election fail, change role to controlling";
                    setRole(Role::Controlling);
                } else {
                    InfoL << "rule conflict, election success in controlled, skip";
                    return;
                }
            }
            nominated(pair, candidate);
            return;
        }
    }

    auto srflx = static_pointer_cast<StunAttrXorMappedAddress>(packet->getAttribute(StunAttribute::Type::XOR_MAPPED_ADDRESS));
    if (!srflx) {
        WarnL << "Binding request missing XOR_MAPPED_ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
    }

    onCompleted(pair);
}

// TODO packet, pair使用引用？
void IceAgent::handleAllocateResponse(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL; 

    if (RTC::StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "fail, get response class: " << packet->getClassStr() << "method: " << packet->getMethodStr() 
            << ", errorCode: " << (uint16_t)packet->getErrorCode();
        if (packet->getErrorCode() == StunAttrErrorCode::Code::AllocationQuotaReached) {
            InfoL << "use stun retry";
        }
        return;
    }

    auto srflx = static_pointer_cast<StunAttrXorMappedAddress>(packet->getAttribute(StunAttribute::Type::XOR_MAPPED_ADDRESS));
    if (!srflx) {
        WarnL << "Binding request missing XOR_MAPPED_ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
    }

#if 0
    CandidateInfo candidate;
    candidate._type = CandidateInfo::AddressType::SRFLX;
    candidate._addr._host = srflx->getAddrString();
    candidate._addr._port = srflx->getPort();
    candidate._base_addr._host = pair->get_local_ip();
    candidate._base_addr._port = pair->get_local_port();
    candidate._ufrag = getUfrag();
    candidate._pwd = getPassword();
    onGatheringCandidate(pair, candidate);
#endif

    auto relay = static_pointer_cast<StunAttrXorRelayedAddress>(packet->getAttribute(StunAttribute::Type::XOR_RELAYED_ADDRESS));
    if (!relay) {
        WarnL << "Binding request missing XOR_RELAYED_ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
    }

    CandidateInfo candidate;
    candidate._type = CandidateInfo::AddressType::RELAY;
    candidate._addr._host = relay->getAddrString();
    candidate._addr._port = relay->getPort();
    candidate._base_addr._host = candidate._addr._host;
    candidate._base_addr._port = candidate._addr._port;
    candidate._ufrag = getUfrag();
    candidate._pwd = getPassword();

    TraceL << "get local candidate type "  << candidate.getAddressTypeStr() << " : " << candidate._addr._host << ":" << candidate._addr._port
        << ", by srflx addr " << srflx->getAddrString() << " : " << srflx->getPort()
        << ", by host addr " << pair->get_local_ip() << " : " << pair->get_local_port();
    onGatheringCandidate(pair, candidate);
}

// TODO packet, pair, peer_addr使用引用？
void IceAgent::handleCreatePermissionResponse(const StunPacket::Ptr packet, Pair::Ptr pair, const sockaddr_storage peer_addr) {
    // TraceL; 

    if (RTC::StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "CreatePermission failed, response class: " << packet->getClassStr();
        return;
    }

    // TraceL << "CreatePermission successfully";

    static uint16_t next_channel = 0x4000; // 有效范围是 0x4000-0x7FFF
    uint16_t channel_number = next_channel++;
    if (next_channel > 0x7FFF) {
        next_channel = 0x4000; // 循环使用通道号
    }

    sendChannelBindRequest(pair, channel_number, peer_addr);
}

// TODO packet, peer_addr使用引用？
void IceAgent::handleChannelBindResponse(const StunPacket::Ptr packet, Pair::Ptr pair, uint16_t channel_number, const sockaddr_storage peer_addr) {
    // TraceL;

    if (RTC::StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "ChannelBind failed, response class: " << packet->getClassStr();
        return;
    }

    InfoL << "ChannelBind success, channel_number=" << channel_number 
          << ", peer_addr=" << SockUtil::inet_ntoa((const struct sockaddr*)&peer_addr) << ":" << SockUtil::inet_port((const struct sockaddr*)&peer_addr)
          << ", pair: " << pair->get_local_ip() << ":" << pair->get_local_port() 
          << " <-> " << pair->get_peer_ip() << ":" << pair->get_peer_port()
          << (!pair->get_realyed_ip().empty() ? (" realyed_addr: " + pair->get_realyed_ip() + ":" + to_string(pair->get_realyed_port())) : "");

    addChannelBind(channel_number, peer_addr);
}

// TODO packet, pair使用引用？
void IceAgent::handleDataIndication(const StunPacket::Ptr packet, Pair::Ptr pair) {
    // TraceL;

    // 检查必要的属性
    auto peer_addr = static_pointer_cast<StunAttrXorPeerAddress>(packet->getAttribute(StunAttribute::Type::XOR_PEER_ADDRESS));
    auto data = static_pointer_cast<StunAttrData>(packet->getAttribute(StunAttribute::Type::DATA));

    if (!peer_addr || !data) {
        WarnL << "Data indication missing required attributes";
        return;
    }

    // 获取对端地址
    auto addr = peer_addr->getAddr();

    // 检查是否有对应peer地址的权限
    if (!hasPermission(addr)) {
        WarnL << "No permission exists for peer address";
        return;
    }

    // 获取数据
    auto buffer = data->getData();

    // 创建一个新的缓冲区
    auto recv_buffer = BufferRaw::create(buffer.size());
    recv_buffer->assign(buffer.data(), buffer.size());

    DebugL << "Received Data indication from peer: " 
        << SockUtil::inet_ntoa((struct sockaddr *)&addr) 
        << ":" << SockUtil::inet_port((struct sockaddr *)&addr)
        << ", data size: " << buffer.size();

    // 通知上层收到数据
    pair->_realyed_addr = std::make_shared<sockaddr_storage>();
    memcpy(pair->_realyed_addr.get(), &addr, sizeof(addr));
    _listener->onIceTransportRecvData(recv_buffer, pair);
}

// TODO pair使用引用？
void IceAgent::handleChannelData(uint16_t channel_number, const char* data, size_t len, Pair::Ptr pair) {
    // TraceL << "Received ChannelData message, channel number: " << channel_number;

    // 查找该通道号对应的目标地址
    auto it = _channel_bindings.find(channel_number);
    if (it == _channel_bindings.end()) {
        WarnL << "No binding found for channel number: " << channel_number;
        return;
    }
 
    // 获取目标地址
    sockaddr_storage addr = it->second;

    // 创建一个新的缓冲区用于转发
    auto buffer = BufferRaw::create(len);
    buffer->assign(data, len);

    // 通知上层收到数据
    pair->_realyed_addr = std::make_shared<sockaddr_storage>();
    memcpy(pair->_realyed_addr.get(), &addr, sizeof(addr));
    _listener->onIceTransportRecvData(buffer, pair);
}

// TODO pair, candidate使用引用？
void IceAgent::onGatheringCandidate(Pair::Ptr pair, CandidateInfo candidate) {
    candidate._priority = calIceCandidatePriority(candidate._type);
    InfoL << "get local candidate type "  << candidate.getAddressTypeStr() << " : " << candidate._addr._host << ":" << candidate._addr._port;

    // 使用_socket_candidate_manager替代_local_candidates进行5元组重复检查
    if (!_socket_candidate_manager.addMapping(pair->_socket, candidate)) {
        InfoL << "has same 5 tuple, skip";
        return;
    }

    _listener->onIceTransportGatheringCandidate(pair, candidate);

    //如果是REALY,当前的所有PEER Candidate进行CreatePermission
    if (candidate._type == CandidateInfo::AddressType::RELAY) {
        _socket_candidate_manager._has_realyed_cnadidate = true;
        for (auto remote_candidate : _remote_candidates) {
            localRealyedConnectivityCheck(remote_candidate);
        }
    }
}

// TODO pair使用引用？
void IceAgent::onConnected(IceTransport::Pair::Ptr pair) {
    DebugL << "get connectivity check pair: " 
        << pair->get_local_ip() << ":" << pair->get_local_port()
        << " <-> " << pair->get_peer_ip() << ":" << pair->get_peer_port() 
        << (!pair->get_realyed_ip().empty() ?  (" realyed addr: " + pair->get_realyed_ip() + ":" 
        + to_string(pair->get_realyed_port())) : " ");

    if (getState() != State::Running) {
        InfoL << "ice state: "<< stateToString(getState()) << " is not running, skip";
        return;
    }

    TraceL << "checklist size: " << _checklist.size();
    //判断ConnectivityCheck的pair是否在checklist中,存在的话加入到validlist
    for (auto &candidate_pair : _checklist) {
        auto &pair_it = candidate_pair->_local_pair;
        auto &remote_candidate = candidate_pair->_remote_candidate;
        auto &state = candidate_pair->_state;

        TraceL << "check pair local(" << candidate_pair->_local_candidate.getAddressTypeStr() << ") " 
            << candidate_pair->_local_candidate._addr._host << ":" << candidate_pair->_local_candidate._addr._port
            << " <-> " << "remote(" << candidate_pair->_remote_candidate.getAddressTypeStr() << ") " 
            << candidate_pair->_remote_candidate._addr._host << ":" << candidate_pair->_remote_candidate._addr._port
            << ", pair info: " << pair_it->get_local_ip() << ":" << pair_it->get_local_port()
            << " <-> " << pair_it->get_peer_ip() << ":" << pair_it->get_peer_port() 
            << (!pair_it->get_realyed_ip().empty() ?  (" realyed addr: " + pair_it->get_realyed_ip() + ":" 
            + to_string(pair_it->get_realyed_port())) : " ");

        //即使是新的Peer 反射地址,也已经添加到_checklist中了
        //所以肯定会在_checklist中找到匹配项
        if (!Pair::is_same(pair_it.get(), pair.get())) {
            continue;
        }

        if (state == CandidateInfo::State::Frozen || state == CandidateInfo::State::Waiting) {
            continue;
        }

        state = CandidateInfo::State::Succeeded;

        // 检查ICE传输策略
        if (!checkIceTransportPolicy(*candidate_pair, pair)) {
            return;
        }

        InfoL << "push local(" << candidate_pair->_local_candidate.getAddressTypeStr() << ") " 
            << candidate_pair->_local_candidate._addr._host << ":" << candidate_pair->_local_candidate._addr._port
            << " <-> remote(" << candidate_pair->_remote_candidate.getAddressTypeStr() << ") " 
            << candidate_pair->_remote_candidate._addr._host << ":" << candidate_pair->_remote_candidate._addr._port
            << " to valid_list";

        // 直接将候选者对添加到valid_list
        _valid_list.push_back(candidate_pair);

        if (getRole() == Role::Controlling) {
            if (getState() != IceAgent::State::Completed) {
                nominated(pair, remote_candidate);
            }
        }
    }

    if (getRole() == Role::Controlled && _nominated_pair) {
        onCompleted(_nominated_pair);
    }
}

// TODO pair使用引用？
void IceAgent::onCompleted(IceTransport::Pair::Ptr pair) {
    // TraceL;
    bool found_in_valid_list = false;
    CandidateInfo local_candidate_info;
    CandidateInfo remote_candidate_info;
    if (getImplementation() == Implementation::Full) {
        for (auto &candidate_pair : _valid_list) {
            auto &pair_it = candidate_pair->_local_pair;
            auto &remote_candidate = candidate_pair->_remote_candidate;

            if (Pair::is_same(pair_it.get(), pair.get())) {
                local_candidate_info = candidate_pair->_local_candidate;
                remote_candidate_info = candidate_pair->_remote_candidate;
                candidate_pair->_nominated = true;
                found_in_valid_list = true;
                break;
            }
        }

        if (!found_in_valid_list) {
            InfoL << "not found peer pair: ip: " << pair->get_peer_ip() << ", port: " << pair->get_peer_port() << "in valid_list, record first";
            //提名的candidate 未在_valid_list 中找到.先记录
            _nominated_pair = pair;
        }

    } else {
        //Lite 模式，不做candidate校验逻辑
        found_in_valid_list = true;
    }

    if (found_in_valid_list && getState() != IceAgent::State::Completed) {
        InfoL << "select pair: local(" << local_candidate_info.getAddressTypeStr() << ") " 
            << local_candidate_info._addr._host << ":" << local_candidate_info._addr._port
            << " <-> remote(" << remote_candidate_info.getAddressTypeStr() << ") " 
            << remote_candidate_info._addr._host << ":" << remote_candidate_info._addr._port
            << ", pair info: " << pair->get_local_ip() << ":" << pair->get_local_port()
            << " <-> " << pair->get_peer_ip() << ":" << pair->get_peer_port() 
            << (!pair->get_realyed_ip().empty() ?  (" realyed addr: " + pair->get_realyed_ip() + ":" 
            + to_string(pair->get_realyed_port())) : " ");

        setSelectedPair(pair);
        setState(IceAgent::State::Completed);
        _listener->onIceTransportCompleted();
        if (_nominated_response) {
            sendPacket(_nominated_response, pair);
        }
        _nominated_response = nullptr;
        _nominated_pair = nullptr;
    }
    return;
}

void IceAgent::refreshPermissions() {
    if (!_ice_server || _ice_server->_schema != IceServerInfo::SchemaType::TURN) {
        return;
    }

    uint64_t now = toolkit::getCurrentMillisecond();

    // 遍历所有权限，删除过期的权限
    for (auto it = _permissions.begin(); it != _permissions.end();) {
        if (now - it->second > 5 * 60 * 1000) {
            it = _permissions.erase(it);
        } else {
            ++it;
        }
    }

    // 对于
    for (auto& permission : _permissions) {
        if (now - permission.second > 4 * 60 * 1000) {
            // 创建一个新的权限请求
            sockaddr_storage addr = permission.first;
            for (auto& socket : _socket_candidate_manager._relay_sockets) {
                auto pair = std::make_shared<Pair>(socket);
                sendCreatePermissionRequest(pair, addr);
                break; // 只需要使用一个本地候选项发送请求
            }
        }
    }
}

void IceAgent::refreshChannelBindings() {
    if (!_ice_server || _ice_server->_schema != IceServerInfo::SchemaType::TURN) {
        return;
    }
    uint64_t now = toolkit::getCurrentMillisecond();
 
    // 遍历所有通道绑定，删除过期的绑定
    for (auto it = _channel_binding_times.begin(); it != _channel_binding_times.end();) {
        if (now - it->second > 10 * 60 * 1000) { // 通道绑定有效期为10分钟
            _channel_bindings.erase(it->first);
            it = _channel_binding_times.erase(it);
        } else {
            ++it;
        }
    }
 
    // 对于即将过期的通道绑定（例如还有2分钟过期），刷新它们
    for (auto& binding_time : _channel_binding_times) {
        if (now - binding_time.second > 8 * 60 * 1000) {
            uint16_t channel_number = binding_time.first;
            auto it = _channel_bindings.find(channel_number);
            if (it != _channel_bindings.end()) {
                sockaddr_storage addr = it->second;
                for (auto& socket : _socket_candidate_manager._relay_sockets) {
                    auto pair = std::make_shared<Pair>(socket);
                    sendChannelBindRequest(pair, channel_number, addr);
                    break; // 只需要使用一个本地候选项发送请求
                }
            }
        }
    }
}

// TODO pair使用引用？
void IceAgent::setSelectedPair(Pair::Ptr pair) {
    DebugL;
    if (_selected_pair && Pair::is_same(pair.get(), _selected_pair.get())){
        return;
    }
    _last_selected_pair = std::static_pointer_cast<Pair>(_selected_pair);
    _selected_pair = pair;
}

// TODO buf, pair使用引用？
void IceAgent::sendSocketData(toolkit::Buffer::Ptr buf, Pair::Ptr pair, bool flush) {
    if (pair == nullptr) {
        pair = getSelectedPair();
    }

    if (pair == nullptr) {
        throw std::invalid_argument(StrPrinter << "pair should not be nullptr");
    }

    auto use_pair = std::make_shared<Pair>(*pair);
    if (use_pair->_realyed_addr) {
        return sendRealyPacket(buf, use_pair, flush);
    }
    return sendSocketData_l(buf, use_pair, flush);
}

// TODO buffer, pair使用引用？
void IceAgent::sendRealyPacket(toolkit::Buffer::Ptr buffer, Pair::Ptr pair, bool flush) {
    // TraceL;
    auto peer_addr = std::move(pair->_realyed_addr);
    pair->_realyed_addr = nullptr;

    if (!hasPermission(*peer_addr)) {
        WarnL << "No permission exists for peer: " << SockUtil::inet_ntoa((const struct sockaddr*)peer_addr.get()) 
            << ":" << SockUtil::inet_port((const struct sockaddr*)peer_addr.get());
        return;
    }

    uint16_t channel_number;
    if (hasChannelBind(*peer_addr, channel_number)) {
        sendChannelData(channel_number, buffer, pair);
    } else {
        sendSendIndication(*peer_addr, buffer, pair);
    }
}

// TODO pair使用引用？
CandidateInfo IceAgent::getLocalCandidateInfo(Pair::Ptr pair) {
    // 从socket_candidate_manager中查找对应的本地候选者信息
    for (const auto& socket_candidates : _socket_candidate_manager.socket_to_candidates) {
        if (socket_candidates.first == pair->_socket) {
            // 找到对应socket的候选者列表，选择第一个作为默认
            if (!socket_candidates.second.empty()) {
                return socket_candidates.second[0];
            }
        }
    }

    throw std::invalid_argument("No candidate found for the specified socket pair");
}

// TODO pair使用引用？
void IceAgent::addToChecklist(Pair::Ptr pair, CandidateInfo& remote_candidate) {
    try {
        CandidateInfo local_candidate = getLocalCandidateInfo(pair);
        auto candidate_pair = std::make_shared<CandidatePair>(std::make_shared<Pair>(*pair), remote_candidate, local_candidate);
        candidate_pair->_state = CandidateInfo::State::InProgress;
        _checklist.push_back(candidate_pair);

        std::sort(_checklist.begin(), _checklist.end(), [] (
            const std::shared_ptr<CandidatePair>& a, const std::shared_ptr<CandidatePair>& b) {
            return *a < *b;
        });

        InfoL << "connectivity check cnadidate pair " 
            << "local(" << local_candidate.getAddressTypeStr() << ") " 
            << local_candidate._addr._host << ":" << local_candidate._addr._port 
            << " <-> remote(" << remote_candidate.getAddressTypeStr() << ") " 
            << remote_candidate._addr._host << ":" << remote_candidate._addr._port
            << ", pair info: " << pair->get_local_ip() << ":" << pair->get_local_port()
            << " <-> " << pair->get_peer_ip() << ":" << pair->get_peer_port() 
            << (!pair->get_realyed_ip().empty() ?  (" realyed addr: " + pair->get_realyed_ip() + ":" 
            + to_string(pair->get_realyed_port())) : " ");

        connectivityCheck(std::make_shared<Pair>(*pair), remote_candidate);
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }
}

void IceTransport::checkRequestTimeouts() {
    uint64_t now = toolkit::getCurrentMillisecond();
    
    for (auto it = _response_handlers.begin(); it != _response_handlers.end();) {
        auto& transaction_id = it->first;
        auto& req_info = it->second;
        
        // 检查是否超时
        if (now >= req_info._next_timeout) {
            if (req_info._retry_count >= RequestInfo::MAX_RETRIES) {
                // 超过最大重传次数，放弃请求并清理
                WarnL << "STUN request timeout after " << RequestInfo::MAX_RETRIES 
                      << " retries, transaction_id: " << hexdump(transaction_id.data(), transaction_id.size());
                it = _response_handlers.erase(it);
                continue;
            } else {
                // 执行重传
                retransmitRequest(transaction_id, req_info);
            }
        }
        ++it;
    }
}

void IceTransport::retransmitRequest(const std::string& transaction_id, RequestInfo& req_info) {
    // 增加重传次数
    req_info._retry_count++;
    
    // RTO翻倍（指数退避）
    req_info._rto *= 2;
    
    // 计算下次超时时间
    uint64_t now = toolkit::getCurrentMillisecond();
    req_info._next_timeout = now + req_info._rto;

#if 0
    TraceL << "Retransmitting STUN request (attempt " << req_info._retry_count
          << "/" << RequestInfo::MAX_RETRIES << "), RTO: " << req_info._rto
          << "ms, transaction_id: " << hexdump(transaction_id.data(), transaction_id.size());
#endif
    
    // 重新发送请求包
    sendPacket(req_info._request, req_info._pair);
}

Json::Value IceAgent::getChecklistInfo() const {
    Json::Value result;

    Json::Value local_candidates_array(Json::arrayValue);
    auto all_local_candidates = _socket_candidate_manager.getAllCandidates();
    for (const auto& local_candidate : all_local_candidates) {
        Json::Value candidate_info;
        candidate_info["type"] = getAddressTypeStr(local_candidate._type);
        candidate_info["host"] = local_candidate._addr._host;
        candidate_info["port"] = local_candidate._addr._port;
        candidate_info["priority"] = local_candidate._priority;
        if (!local_candidate._base_addr._host.empty()) {
            candidate_info["base_host"] = local_candidate._base_addr._host;
            candidate_info["base_port"] = local_candidate._base_addr._port;
        }
        local_candidates_array.append(candidate_info);
    }
    result["local_candidates"] = local_candidates_array;
    result["local_candidates_count"] = static_cast<Json::UInt64>(all_local_candidates.size());
    
    Json::Value remote_candidates_array(Json::arrayValue);
    for (const auto& remote_candidate : _remote_candidates) {
        Json::Value candidate_info;
        candidate_info["type"] = getAddressTypeStr(remote_candidate._type);
        candidate_info["host"] = remote_candidate._addr._host;
        candidate_info["port"] = remote_candidate._addr._port;
        candidate_info["priority"] = remote_candidate._priority;
        if (!remote_candidate._base_addr._host.empty()) {
            candidate_info["base_host"] = remote_candidate._base_addr._host;
            candidate_info["base_port"] = remote_candidate._base_addr._port;
        }
        remote_candidates_array.append(candidate_info);
    }
    result["remote_candidates"] = remote_candidates_array;
    result["remote_candidates_count"] = static_cast<Json::UInt64>(_remote_candidates.size());

    Json::Value checklist_array(Json::arrayValue);
    auto build_candidate = [](const CandidateInfo& candidate) -> std::string {
        return "("+getAddressTypeStr(candidate._type) + ") " + 
            candidate._addr._host + ":" + to_string(candidate._addr._port);
    };
    
    for (const auto& candidate_pair : _checklist) {
        Json::Value entry;
        entry["candidate_pair"] = build_candidate(candidate_pair->_local_candidate) 
            + " <-> " + build_candidate(candidate_pair->_remote_candidate);
        entry["state"] = CandidateInfo::getStateStr(candidate_pair->_state);
        entry["priority"] = (Json::UInt64)candidate_pair->_priority;
        entry["nominated"] = candidate_pair->_nominated;
        checklist_array.append(entry);
    }
 
    result["checklists"] = checklist_array;
    result["checklists_count"] = (int)_checklist.size();
    result["ice_state"] = stateToString(_state);
    
    if (_selected_pair) {
        Json::Value active_pair;
        active_pair["local"] = _selected_pair->get_local_ip() + ":" + std::to_string(_selected_pair->get_local_port());
        
        // 优先使用realyed地址，如果没有则使用peer地址
        const auto remote_ip = _selected_pair->get_realyed_ip().empty() 
            ? _selected_pair->get_peer_ip() : _selected_pair->get_realyed_ip();
        const auto remote_port = _selected_pair->get_realyed_ip().empty() 
            ? _selected_pair->get_peer_port() : _selected_pair->get_realyed_port();
        active_pair["remote"] = remote_ip + ":" + std::to_string(remote_port);
        
        result["active_pair"] = active_pair;
    } else {
        result["active_pair"] = Json::nullValue;
    }
    
    return result;
}

} // namespace RTC
