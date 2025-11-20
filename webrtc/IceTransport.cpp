/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
*/

#include <utility>
#include <random>
#include <algorithm>
#include "json/json.h"
#include "Util/onceToken.h"
#include "Network/UdpClient.h"
#include "Common/Parser.h"
#include "Common/config.h"
#include "IceTransport.hpp"
#include "WebRtcTransport.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace RTC {

#define RTC_FIELD "rtc."
const string kPortRange = RTC_FIELD "port_range";
const string kMaxStunRetry = RTC_FIELD "max_stun_retry";
static onceToken token([]() {
    mINI::Instance()[kPortRange] = "49152-65535";
    mINI::Instance()[kMaxStunRetry] = 7;
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

std::string addrToStr(const sockaddr_storage& addr) {
    return StrPrinter << SockUtil::inet_ntoa((const struct sockaddr*)&addr) 
        << ":" << SockUtil::inet_port((const struct sockaddr*)&addr);
}

// 检查ICE传输策略是否允许该候选者对
static bool checkIceTransportPolicy(const IceAgent::CandidatePair& pair_info, const IceTransport::Pair::Ptr& pair) {
    GET_CONFIG(int, ice_transport_policy, Rtc::kIceTransportPolicy);
    
    // 优先使用新的统一配置参数
    switch (static_cast<IceTransportPolicy>(ice_transport_policy)) {
        case IceTransportPolicy::kRelayOnly:
            // 仅支持Relay转发：要求本地或远程是中继类型
            if (pair_info._local_candidate._type != CandidateInfo::AddressType::RELAY && 
                pair_info._remote_candidate._type != CandidateInfo::AddressType::RELAY) {
                DebugL << "relay only policy, skip pair: " << pair_info.dumpString();
                return false;
            }
            break;
            
        case IceTransportPolicy::kP2POnly:
            // 仅支持P2P直连：要求本地和远程都不是中继类型
            if (pair_info._local_candidate._type == CandidateInfo::AddressType::RELAY ||
                pair_info._remote_candidate._type == CandidateInfo::AddressType::RELAY) {
                DebugL << "p2p only policy, skip pair: " << pair_info.dumpString();
                return false;
            }
            break;
            
        case IceTransportPolicy::kAll:
        default: break;
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
        throw std::runtime_error(StrPrinter << "fail to parse schema in url: " << url_in);
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
        throw std::runtime_error(StrPrinter << "not support schema: " << schema);
    }

    // 解析了用户名密码之后再解析?参数，防止密码中的?被判为参数分隔符
    auto pos = url.find("?");
    if (pos != string::npos) {
        _param_strs = url.substr(pos + 1);
        url.erase(pos);
    }
    _addr._port = (_secure == CandidateTuple::SecureType::NOT_SECURE) ? 3478 : 5349;
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
}

////////////  IceTransport //////////////////////////

IceTransport::IceTransport(Listener* listener, std::string ufrag, std::string password, EventPoller::Ptr poller)
: _poller(std::move(poller)), _listener(listener), _ufrag(std::move(ufrag)), _password(std::move(password)) {
    TraceL;
    _identifier = makeRandStr(32);
    _request_handlers.emplace(std::make_pair(StunPacket::Class::REQUEST, StunPacket::Method::BINDING), 
                              std::bind(&IceTransport::handleBindingRequest, this, std::placeholders::_1, std::placeholders::_2));
}

void IceTransport::initialize() {
    weak_ptr<IceTransport> weak_self = static_pointer_cast<IceTransport>(shared_from_this());
    _retry_timer = std::make_shared<Timer>(0.1f, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->checkRequestTimeouts();
        return true;
    }, getPoller());
}

void IceTransport::sendSocketData(const Buffer::Ptr& buf, const Pair::Ptr& pair, bool flush) {
    return sendSocketData_l(buf, pair, flush);
}

void IceTransport::sendSocketData_l(const Buffer::Ptr& buf, const Pair::Ptr& pair, bool flush) {
    // DebugL;
    if (pair == nullptr) {
        throw std::invalid_argument("pair should not be nullptr");
    }

    // 一次性发送一帧的rtp数据，提高网络io性能  [AUTO-TRANSLATED:fbab421e]
    // Send one frame of rtp data at a time to improve network io performance
    if (pair->_socket->getSock()->sockType() == SockNum::Sock_TCP) {
        // 增加tcp两字节头
        uint16_t len = htons(buf->size());
        pair->_socket->SockSender::send((char *)&len, 2);
    }

#if 0
    TraceL << pair->dumpString(1) << " send " << buf->size();
    TraceL << "data: " << hexdump(buf->data(), buf->size());
#endif

    sockaddr_storage peer_addr;
    pair->get_peer_addr(peer_addr);
    auto addr_len = SockUtil::get_sock_len((const struct sockaddr*)&peer_addr);
    pair->_socket->sendto(buf, (struct sockaddr*)&peer_addr, addr_len);
    if (flush) {
        pair->_socket->flushAll();
    }
}

bool IceTransport::processSocketData(const uint8_t* data, size_t len, const Pair::Ptr& pair) {
#if 0
    TraceL << pair->dumpString(0) << " data len: " << len;
    sockaddr_storage relay_peer_addr;
    if (pair->get_relayed_addr(relay_peer_addr)) {
        TraceL << "data relay from peer " << addrToStr(relay_peer_addr);
    }
#endif

    auto packet = StunPacket::parse(data, len);
    if (!packet) {
        return processChannelData(data, len, pair);
    }
    processStunPacket(packet, pair);
    return true;
}

void IceTransport::processStunPacket(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
#if 0
    TraceL << "recv packet : " << packet->dumpString(true);
#endif
    if ((packet->getClass() == StunPacket::Class::REQUEST) || (packet->getClass() == StunPacket::Class::INDICATION)) {
        processRequest(packet, pair);
    } else {
        processResponse(packet, pair);
    }
}

StunPacket::Authentication IceTransport::checkRequestAuthentication(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    if (packet->getClass() == StunPacket::Class::INDICATION) {
        return StunPacket::Authentication::OK;
    }
#if 0
    DebugL << "_ufrag: "  << _ufrag << ", _password: "  << _password;
#endif
    // Check authentication.
    auto ret = packet->checkAuthentication(_ufrag, _password);
    if (ret != StunPacket::Authentication::OK) {
        sendUnauthorizedResponse(packet, pair);
    }
    return ret;
}

StunPacket::Authentication IceTransport::checkResponseAuthentication(const StunPacket::Ptr& request, const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    if (!packet->hasAttribute(StunAttribute::Type::FINGERPRINT)) {
        sendUnauthorizedResponse(packet, pair);
        return StunPacket::Authentication::UNAUTHORIZED;
    }
#if 0
    DebugL << "peer_ufrag: "  << request->getPeerUfrag() << ", peer_password: "  << request->getPeerPassword();
#endif
    auto ret = packet->checkAuthentication(request->getPeerUfrag(), request->getPeerPassword());
    if (ret != StunPacket::Authentication::OK) {
        sendUnauthorizedResponse(packet, pair);
    }
    return ret;
}

void IceTransport::processResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL;

    auto it = _response_handlers.find(packet->getTransactionId().data());
    if (it == _response_handlers.end()) {
        WarnL << "not support stun transaction_id ignore: " << packet->dumpString(true);
        return;
    }

    auto request = std::move(it->second._request);
    auto handle = std::move(it->second._handler);
    // 收到响应后立即清理请求信息
    _response_handlers.erase(it);

    if (packet->getClass() == StunPacket::Class::ERROR_RESPONSE) {
        if (StunAttrErrorCode::Code::Unauthorized == packet->getErrorCode()) {
            return processUnauthorizedResponse(packet, request, pair, std::move(handle));
        }
        return;
    }

    if (StunPacket::Authentication::OK != checkResponseAuthentication(request, packet, pair)) {
        WarnL << "checkRequestAuthentication fail: " << packet->dumpString();
        return;
    }

    handle(packet, pair);
}

#pragma pack(push, 1)
struct ChannelDataHeader {
    uint16_t channel_number;
    uint16_t data_length;
};
#pragma pack(pop)

bool IceTransport::processChannelData(const uint8_t* data, size_t len, const Pair::Ptr& pair) {
    // DebugL;

    // 检查数据长度是否足够
    if (len < 4) {
        WarnL << "Received data too short to be a valid STUN or ChannelData message";
        return false;
    }
    ChannelDataHeader header = *(reinterpret_cast<const ChannelDataHeader *>(data));
    header.channel_number = ntohs(header.channel_number);
    header.data_length = ntohs(header.data_length);

    // 检查是否是ChannelData消息
    // ChannelData消息的前两个字节是通道号，范围是0x4000-0x7FFF
    if (header.channel_number < 0x4000 || header.channel_number > 0x7FFF) {
        // WarnL << "Invalid channel number: " << header.channel_number;
        return false;
    }

    // 这是一个ChannelData消息; 检查数据长度是否合法
    if (len < 4 + header.data_length) {
        WarnL << "ChannelData message truncated, len: " << len << ", data_length: " << header.data_length;
        return false;
    }

    handleChannelData(header.channel_number, (const char *)(data + 4), header.data_length, pair);
    return true;
}

void IceTransport::processUnauthorizedResponse(const StunPacket::Ptr& response, const StunPacket::Ptr& request, const Pair::Ptr& pair, MsgHandler handler) {
    // TraceL;
    auto attr_nonce = response->getAttribute<StunAttrNonce>();
    auto attr_realm = response->getAttribute<StunAttrRealm>();
    if (!attr_nonce || !attr_realm) {
        return;
    }

    request->refreshTransactionId();
    request->addAttribute(std::move(attr_nonce));
    request->addAttribute(std::move(attr_realm));
    request->setNeedMessageIntegrity(true);
    sendRequest(request, pair, std::move(handler));
}

void IceTransport::processRequest(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL;
    if (StunPacket::Authentication::OK != checkRequestAuthentication(packet, pair)) {
        WarnL << "checkRequestAuthentication fail: " << packet->dumpString();
        return;
    }

    auto it = _request_handlers.find(std::make_pair(packet->getClass(), packet->getMethod()));
    if (it == _request_handlers.end()) {
        WarnL << "ignore unsupport stun "<< packet->dumpString();
        return;
    }

    return (it->second)(packet, pair);
}

void IceTransport::handleBindingRequest(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL;

    auto response = packet->createSuccessResponse();
    response->setUfrag(_ufrag);
    response->setPassword(_password);

    sockaddr_storage peer_addr;
    if (!pair->get_relayed_addr(peer_addr)) {
        pair->get_peer_addr(peer_addr);
    }

    // Add XOR-MAPPED-ADDRESS.
    auto attr_xor_mapped_address = std::make_shared<StunAttrXorMappedAddress>(response->getTransactionId());
    attr_xor_mapped_address->setAddr(peer_addr);
    response->addAttribute(std::move(attr_xor_mapped_address));

    sendPacket(response, pair);
}

void IceTransport::sendChannelData(uint16_t channel_number, const Buffer::Ptr& buffer, const Pair::Ptr& pair) {
    // TraceL;

    // ChannelData不是STUN消息，需要单独实现
    // ChannelData格式：2字节Channel Number + 2字节数据长度 + 数据内容
    auto data_len = buffer->size();
    size_t total_len = 4 + data_len;
    // 分配缓冲区：头部4字节 + 数据长度
    auto channel_data = toolkit::BufferRaw::create(total_len);
    auto header = reinterpret_cast<ChannelDataHeader *>(channel_data->data());
    // 设置Channel Number (前两字节，网络字节序)
    header->channel_number = htons(channel_number);
    // 设置数据长度 (中间两字节，网络字节序)
    header->data_length = htons(data_len);
    // 拷贝数据
    memcpy(channel_data->data() + 4, buffer->data(), data_len);
    channel_data->setSize(total_len);

#if 0
    TraceL << pair->dumpString(1) << " send channel " << channel_number << " data " << data_len;
    TraceL << "data: " << hexdump(buffer->data(), buffer->size());
#endif

    sendSocketData(channel_data, pair);
}

void IceTransport::sendUnauthorizedResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL;
    auto response = packet->createErrorResponse(StunAttrErrorCode::Code::Unauthorized);
    sendPacket(response, pair);
}

void IceTransport::sendErrorResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair, StunAttrErrorCode::Code errorCode) {
    // TraceL;
    auto response = packet->createErrorResponse(errorCode);
    sendPacket(response, pair);
}

void IceTransport::sendRequest(const StunPacket::Ptr& packet, const Pair::Ptr& pair, MsgHandler handler) {
    // TraceL;
    _response_handlers.emplace(packet->getTransactionId().data(), RequestInfo(packet, std::move(handler), pair));
    sendPacket(packet, pair);
}

void IceTransport::sendPacket(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
#if 0
     TraceL << pair->dumpString(1) << " send packet: " << packet->dumpString(true);
#endif
    packet->serialize();
    sendSocketData(std::static_pointer_cast<toolkit::Buffer>(packet), pair);
}

bool IceTransport::hasPermission(const sockaddr_storage& addr) {
    auto it = _permissions.find(addr);
    if (it == _permissions.end()) {
        return false;
    }

    // 权限有效期为5分钟
    uint64_t now = toolkit::getCurrentMillisecond();
    if (now - it->second > 5 * 60 * 1000) {
        DebugL << "permissions over time, ip:" << addrToStr(addr);
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
    if (type != CandidateTuple::TransportType::UDP) {
        throw std::invalid_argument("not support transport type: TCP");
    }
    return createUdpSocket(peer_host, peer_port, local_ip, local_port);
}

SocketHelper::Ptr IceTransport::createUdpSocket(const std::string &peer_host, uint16_t peer_port, const std::string &local_ip, uint16_t local_port) {
    auto socket = std::make_shared<UdpClient>(getPoller());

    weak_ptr<IceTransport> weak_self = static_pointer_cast<IceTransport>(shared_from_this());
    auto ptr = socket.get();
    socket->setOnRecvFrom([weak_self, ptr](const Buffer::Ptr &buffer, struct sockaddr *addr, int addr_len) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        auto peer_host = SockUtil::inet_ntoa(addr);
        auto peer_port = SockUtil::inet_port(addr);
        auto pair = std::make_shared<Pair>(ptr->shared_from_this(), std::move(peer_host), peer_port);
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

    void addListenConfigReload(){
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
                InfoL << "release port:" << pos << "[" << strong_self->_min_port << "-" << strong_self->_max_port << "]";
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
    deque<uint16_t> _port_pool;
};

//注册端口管理监听配置重载
onceToken PortManager_token([](){
    PortManager<0>::Instance().addListenConfigReload();
    PortManager<1>::Instance().addListenConfigReload();
});

std::unordered_map<sockaddr_storage /*peer ip:port*/, IceServer::WeakPtr, toolkit::SockUtil::SockAddrHash, toolkit::SockUtil::SockAddrEqual> _relayed_session;

IceServer::IceServer(Listener* listener, std::string ufrag, std::string password, toolkit::EventPoller::Ptr poller)
    : IceTransport(listener, std::move(ufrag), std::move(password), std::move(poller)) {
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
                                  std::bind(&IceServer::handleChannelBindRequest, this, placeholders::_1, placeholders::_2));
        _request_handlers.emplace(std::make_pair(StunPacket::Class::INDICATION, StunPacket::Method::SEND), 
                                  std::bind(&IceServer::handleSendIndication, this, placeholders::_1, placeholders::_2));
    }

}

bool IceServer::processSocketData(const uint8_t* data, size_t len, const Pair::Ptr& pair) {
    if (!_session_pair) {
        _session_pair = pair;
    }
    return IceTransport::processSocketData(data, len, pair);
}

void IceServer::processRelayPacket(const Buffer::Ptr &buffer, const Pair::Ptr& pair) {
    // TraceL << pair->dumpString(0);

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

void IceServer::handleAllocateRequest(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
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
    auto socket = allocateRelayed(pair);
    sockaddr_storage relayed_addr = SockUtil::make_sockaddr(socket->get_local_ip().data(), socket->get_local_port());
    auto attr_xor_relayed_address = std::make_shared<StunAttrXorRelayedAddress>(response->getTransactionId());
    attr_xor_relayed_address->setAddr(relayed_addr);
    response->addAttribute(std::move(attr_xor_relayed_address));

    auto attr_lifetime = std::make_shared<StunAttrLifeTime>();
    attr_lifetime->setLifetime(600);
    response->addAttribute(std::move(attr_lifetime));
 
    sendPacket(response, pair);
}

void IceServer::handleRefreshRequest(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL
}

void IceServer::handleCreatePermissionRequest(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL

    // 检查XOR-PEER-ADDRESS属性是否存在
    auto peer_addr = packet->getAttribute<StunAttrXorPeerAddress>();
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

void IceServer::handleChannelBindRequest(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL

    // 检查必要的属性
    auto channel_number = packet->getAttribute<StunAttrChannelNumber>();
    auto peer_addr = packet->getAttribute<StunAttrXorPeerAddress>();
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

void IceServer::handleSendIndication(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL

    // 检查必要的属性
    auto peer_addr = packet->getAttribute<StunAttrXorPeerAddress>();
    auto data = packet->getAttribute<StunAttrData>();

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

void IceServer::handleChannelData(uint16_t channel_number, const char* data, size_t len, const Pair::Ptr& pair) {
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
    relayBackingData(buffer, pair, peer_addr);
}

void IceServer::sendUnauthorizedResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
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

    IceTransport::sendUnauthorizedResponse(packet, pair);
}

StunPacket::Authentication IceServer::checkRequestAuthentication(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL

    //ICE SERVER 不对BINDGING请求校验
    if (packet->getMethod() == StunPacket::Method::BINDING) {
        return  StunPacket::Authentication::OK;
    }

    return IceTransport::checkRequestAuthentication(packet, pair);
}

void IceServer::sendDataIndication(const sockaddr_storage& peer_addr, const Buffer::Ptr& buffer, const Pair::Ptr& pair) {
    // TraceL

    auto packet = std::make_shared<DataIndicationPacket>();

    auto attr_peer_address = std::make_shared<StunAttrXorPeerAddress>(packet->getTransactionId());
    attr_peer_address->setAddr(peer_addr);
    packet->addAttribute(std::move(attr_peer_address));

    auto attr_data = std::make_shared<StunAttrData>();
    attr_data->setData((const char *)buffer->data(), buffer->size());
    packet->addAttribute(std::move(attr_data));

    sendPacket(packet, pair);
#if 0
    TraceL << pair->dumpString(1) << " Forward UDP data as DataIndication, size: " << buffer->size();
#endif
}

SocketHelper::Ptr IceServer::allocateRelayed(const Pair::Ptr& pair) {
    // DebugL;

    // only support udp
    auto port = PortManager<0>::Instance().getSinglePort();
    std::string local_ip;

    GET_CONFIG_FUNC(std::vector<std::string>, interfaces, Rtc::kInterfaces, [](string str) {
        std::vector<std::string> ret;
        if (str.length()) {
            ret = split(str, ",");
        }
        translateIPFromEnv(ret);
        return ret;
    });

    //如果指定了对外的网卡，使用第一个对外网卡的ip
    if (!interfaces.empty()) {
        auto machine_interfaces = SockUtil::getInterfaceList();
        for (auto& obj : machine_interfaces) {
            std::string& interface_ip = obj["ip"];
            if (toolkit::start_with(obj["name"], "lo") || interface_ip == "0.0.0.0" || interface_ip == "::") {
                DebugL << "skip interace: " << obj["name"] << " " << interface_ip;
                continue;
            }
            if (obj["name"] == interfaces.front()) {
                DebugL << "use interace: " << obj["name"] << " " << interface_ip;
                local_ip = interface_ip;
                break;
            }
        }
    }

    if (interfaces.empty() || local_ip.empty()){
        GET_CONFIG_FUNC(std::vector<std::string>, extern_ips, Rtc::kExternIP, [](string str) {
            std::vector<std::string> ret;
            if (str.length()) {
                ret = split(str, ",");
            }
            translateIPFromEnv(ret);
            return ret;
        });

        if (!extern_ips.empty()) {
            local_ip = extern_ips.front();
        } else {
            local_ip = SockUtil::get_local_ip();
        }
    }

    auto socket = createRelayedUdpSocket(pair->get_peer_ip(), pair->get_peer_port(), local_ip, *port);
    auto relayed_pair = std::make_shared<Pair>(socket);
    auto peer_addr = SockUtil::make_sockaddr(pair->get_peer_ip().data(), pair->get_peer_port());
    weak_ptr<IceServer> weak_self = static_pointer_cast<IceServer>(shared_from_this());
    _relayed_pairs.emplace(peer_addr, std::make_pair(port, relayed_pair));
    _relayed_session.emplace(peer_addr, weak_self);

    InfoL << "Alloc relayed pair: " << relayed_pair->get_local_ip() << ":" <<  relayed_pair->get_local_port()
          << " for peer pair: " << pair->get_peer_ip() << ":" << pair->get_peer_port();
    return socket;
}

void IceServer::relayForwordingData(const toolkit::Buffer::Ptr& buffer, const sockaddr_storage& peer_addr) {
    TraceL;
    getPoller()->async([=]() {
        auto it = _relayed_pairs.find(peer_addr);
        if (it == _relayed_pairs.end()) {
#if 0
            //不是当前对象的转发,交给其他对象转发
            auto forword_it = _relayed_session.find(peer_addr);
            if (forword_it == _relayed_session.end()) {
                WarnL << "not relayed addr for peer addr: " << addrToStr(peer_addr);
            }

            auto forword_session = forword_it->second.lock();
            if (!forword_session) {
                WarnL << "forword session for peer addr " << addrToStr(peer_addr) << " is release";
                return;
            }

            if (getIdentifier() == forword_session->getIdentifier()) {
                //找到的会话就是当前会话，忽略
                return;
            }
            forword_session->relayForwordingData(buffer, peer_addr);
            return;
#else 
            WarnL << "not relayed addr for peer addr: " << addrToStr(peer_addr);
#endif
        }

        sendSocketData(buffer, it->second.second);
#if 0
        TraceL << "Forwarded ChannelData to peer: " << addrToStr(peer_addr);
#endif
    });
}

void IceServer::relayBackingData(const toolkit::Buffer::Ptr& buffer, const Pair::Ptr& pair, const sockaddr_storage& peer_addr) {
    // TraceL;

    sockaddr_storage addr;
    pair->get_peer_addr(addr);

    auto it = _relayed_pairs.find(addr);
    if (it == _relayed_pairs.end()) {
        WarnL << "not relayed addr for peer addr: " << addrToStr(addr);
        return;
    }

    auto forward_pair = std::make_shared<Pair>(it->second.second->_socket,
        SockUtil::inet_ntoa((const struct sockaddr *)&peer_addr), SockUtil::inet_port((const struct sockaddr *)&peer_addr));

    sendSocketData(buffer, forward_pair);
#if 0
    DebugL << "relay backing " << forward_pair->dumpString(1);
#endif
}

SocketHelper::Ptr IceServer::createRelayedUdpSocket(const std::string &peer_host, uint16_t peer_port, const std::string &local_ip, uint16_t local_port) {
    auto socket = std::make_shared<UdpClient>(getPoller());

    weak_ptr<IceServer> weak_self = static_pointer_cast<IceServer>(shared_from_this());
    auto ptr = socket.get();
    socket->setOnRecvFrom([weak_self, ptr](const Buffer::Ptr &buffer, struct sockaddr *addr, int addr_len) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        auto peer_host = SockUtil::inet_ntoa(addr);
        auto peer_port = SockUtil::inet_port(addr);
        auto pair = std::make_shared<Pair>(ptr->shared_from_this(), std::move(peer_host), peer_port);
        strong_self->processRelayPacket(buffer, pair);
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

IceAgent::IceAgent(Listener* listener, Implementation implementation, Role role, std::string ufrag, std::string password, toolkit::EventPoller::Ptr poller)
: IceTransport(listener, std::move(ufrag), std::move(password), std::move(poller)), _implementation(implementation) ,_role(role) {
    DebugL;
    _tiebreaker = makeRandNum();
    // 创建定时器，每分钟检查一次权限和通道绑定是否需要刷新
    _refresh_timer = std::make_shared<Timer>(60.0f, [this]() {
        refreshPermissions();
        refreshChannelBindings();
        return true;
    }, getPoller());
}

void IceAgent::gatheringCandidate(const CandidateTuple::Ptr& candidate_tuple, bool gathering_rflx, bool gathering_relay) {
    // TraceL;

    auto interfaces = SockUtil::getInterfaceList();
    for (auto obj : interfaces) {
        std::string local_ip = obj["ip"];
        if (toolkit::start_with(obj["name"], "lo") || local_ip == "0.0.0.0" || local_ip == "::") {
            DebugL << "skip interace: " << obj["name"] << " " << local_ip;
            continue;
        }

        try {
            CandidateInfo candidate;
            candidate._type = CandidateInfo::AddressType::HOST;
            candidate._ufrag = getUfrag();
            candidate._pwd = getPassword();
            candidate._transport = candidate_tuple->_transport;

            auto socket = createSocket(candidate_tuple->_transport, candidate_tuple->_addr._host, candidate_tuple->_addr._port, local_ip);
            _socket_candidate_manager.addHostSocket(socket);
            candidate._addr._host = candidate._base_addr._host = local_ip;
            candidate._addr._port = candidate._base_addr._port = socket->get_local_port();

            TraceL << "gathering local candidate " << candidate.dumpString() << " from stun server " << candidate_tuple->_addr.dumpString();

            auto pair = std::make_shared<Pair>(std::move(socket));
            onGatheringCandidate(pair, candidate);
            if (gathering_rflx) {
                gatheringSrflxCandidate(pair);
            }

            if (gathering_relay) {
                //TODO: 代优化relay_socket 复用host socket当前SocketCandidateManager数据结构不支持
                auto relay_socket = createSocket(candidate_tuple->_transport, candidate_tuple->_addr._host, candidate_tuple->_addr._port, local_ip);
                _socket_candidate_manager.addRelaySocket(relay_socket);
                gatheringRelayCandidate(std::make_shared<Pair>(std::move(relay_socket)));
            }
        } catch (std::exception &ex) {
            WarnL << ex.what();
        }
    }
}

void IceAgent::connectivityCheck(CandidateInfo& candidate) {
    TraceL << candidate.dumpString();
    setState(IceAgent::State::Running);
    auto ret = _remote_candidates.emplace(candidate);
    if (ret.second) {
        bool udp = candidate._transport == CandidateTuple::TransportType::UDP;
        for (auto& socket : _socket_candidate_manager._host_sockets) {
            if (udp != (socket->getSock()->sockType() == SockNum::Sock_UDP)) {
                continue;
            }
            auto pair = std::make_shared<Pair>(socket, candidate._addr._host, candidate._addr._port);
            addToChecklist(pair, candidate);
        }

        if (_socket_candidate_manager._has_relayed_candidate) {
            localRelayedConnectivityCheck(candidate);
        }
    }

}

void IceAgent::localRelayedConnectivityCheck(CandidateInfo& candidate) {
    TraceL << candidate.dumpString();
    for (auto &socket: _socket_candidate_manager._relay_sockets) {
        auto local_relay_pair = std::make_shared<Pair>(socket, _ice_server->_addr._host, _ice_server->_addr._port);
        auto peer_addr = SockUtil::make_sockaddr(candidate._addr._host.data(), candidate._addr._port);
        sendCreatePermissionRequest(local_relay_pair, peer_addr);

        local_relay_pair->_relayed_addr = std::make_shared<sockaddr_storage>(peer_addr);
        addToChecklist(local_relay_pair, candidate);
    }
}

void IceAgent::nominated(const Pair::Ptr& pair, CandidateTuple& candidate) {
    // TraceL;
    auto handler = std::bind(&IceAgent::handleNominatedResponse, this, placeholders::_1, placeholders::_2, candidate);
    sendBindRequest(pair, candidate, true, std::move(handler));
}

void IceAgent::sendSendIndication(const sockaddr_storage& peer_addr, const Buffer::Ptr& buffer, const Pair::Ptr& pair) {
    // TraceL;
    auto packet = std::make_shared<SendIndicationPacket>();

    auto attr_peer_address = std::make_shared<StunAttrXorPeerAddress>(packet->getTransactionId());
    attr_peer_address->setAddr(peer_addr);
    packet->addAttribute(std::move(attr_peer_address));

    auto attr_data = std::make_shared<StunAttrData>();
    attr_data->setData(buffer->data(), buffer->size());
    packet->addAttribute(std::move(attr_data));

    sendPacket(packet, pair);
}

void IceAgent::gatheringSrflxCandidate(const Pair::Ptr& pair) {
    // TraceL;
    auto handle = std::bind(&IceAgent::handleGatheringCandidateResponse, this, placeholders::_1, placeholders::_2);
    sendBindRequest(pair, std::move(handle));
}

void IceAgent::gatheringRelayCandidate(const Pair::Ptr &pair) {
    // TraceL;
    sendAllocateRequest(pair);
}

void IceAgent::connectivityCheck(const Pair::Ptr &pair, CandidateTuple& candidate) {
    // TraceL;
    auto handler = std::bind(&IceAgent::handleConnectivityCheckResponse, this, placeholders::_1, placeholders::_2, candidate);
    sendBindRequest(pair, candidate, false, std::move(handler));
}

void IceAgent::tryTriggerredCheck(const Pair::Ptr& pair) {
    DebugL;
    //FIXME 暂不实现,因为当前实现基本收到candidate就会发起check
}

void IceAgent::sendBindRequest(const Pair::Ptr& pair, MsgHandler handler) {
    // TraceL;
    auto packet = std::make_shared<BindingPacket>();
    packet->setUfrag(_ufrag);
    packet->setPassword(_password);
    packet->setPeerUfrag(_ice_server->_ufrag);
    packet->setPeerPassword(_ice_server->_pwd);

    packet->setNeedFingerprint(false);
    packet->setNeedMessageIntegrity(false);
    sendRequest(packet, pair, std::move(handler));
}

void IceAgent::sendBindRequest(const Pair::Ptr& pair, CandidateTuple& candidate, bool use_candidate, MsgHandler handler) {
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

    sendRequest(packet, pair, std::move(handler));
}

void IceAgent::sendAllocateRequest(const Pair::Ptr& pair) {
    // TraceL;
    auto packet = std::make_shared<AllocatePacket>();
    packet->setNeedMessageIntegrity(false);
    packet->setUfrag(_ufrag);
    packet->setPassword(_password);
    packet->setPeerUfrag(_ice_server->_ufrag);
    packet->setPeerPassword(_ice_server->_pwd);

    auto attr_username = std::make_shared<StunAttrUserName>();
    attr_username->setUsername(_ice_server->_ufrag);
    packet->addAttribute(std::move(attr_username));

    auto attr_requested_transport = std::make_shared<StunAttrRequestedTransport>();
    attr_requested_transport->setProtocol(StunAttrRequestedTransport::Protocol::UDP);
    packet->addAttribute(std::move(attr_requested_transport));

    auto handler = std::bind(&IceAgent::handleAllocateResponse, this, placeholders::_1,  placeholders::_2);
    sendRequest(packet, pair, std::move(handler));
}

void IceAgent::sendCreatePermissionRequest(const Pair::Ptr& pair, const sockaddr_storage& peer_addr) {
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
    sendRequest(packet, pair, std::move(handler));
}

void IceAgent::sendChannelBindRequest(const Pair::Ptr& pair, uint16_t channel_number, const sockaddr_storage& peer_addr) {
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
    sendRequest(packet, pair, std::move(handler));
}

void IceAgent::processRequest(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    static toolkit::onceToken token([this]() {
        _request_handlers.emplace(std::make_pair(StunPacket::Class::INDICATION, StunPacket::Method::DATA), std::bind(&IceAgent::handleDataIndication, this, placeholders::_1, placeholders::_2));
    });
    return IceTransport::processRequest(packet, pair);
}

void IceAgent::handleBindingRequest(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL;
    auto controlling = packet->getAttribute<StunAttrIceControlling>();
    auto controlled = packet->getAttribute<StunAttrIceControlled>();
    auto priority = packet->getAttribute<StunAttrPriority>();

    //角色冲突
    if (controlling && getRole() == Role::Controlling) {
        if (controlling->getTiebreaker() > _tiebreaker) {
            setRole(Role::Controlled);
            InfoL << "role conflict, election fail, change role to controlled";
        } else {
            InfoL << "role conflict, election success in controlling, send error";
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
    if (!pair->get_relayed_addr(peer_addr)) {
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

void IceAgent::handleGatheringCandidateResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL; 

    if (StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "fail, get response: " << packet->dumpString();
        return;
    }

    auto srflx = packet->getAttribute<StunAttrXorMappedAddress>();
    if (!srflx) {
        WarnL << "Binding request missing XOR_MAPPED_ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
    }

    CandidateInfo candidate;
    candidate._type = CandidateInfo::AddressType::SRFLX;
    candidate._addr._host = srflx->getIp();
    candidate._addr._port = srflx->getPort();
    candidate._base_addr._host = pair->get_local_ip();
    candidate._base_addr._port = pair->get_local_port();
    candidate._ufrag = getUfrag();
    candidate._pwd = getPassword();
    onGatheringCandidate(pair, candidate);
}

void IceAgent::handleConnectivityCheckResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair, CandidateTuple& candidate) {
    // TraceL; 

    if (StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "fail, get response: " << packet->dumpString();
        if (packet->getErrorCode() == StunAttrErrorCode::Code::RoleConflict) {
            InfoL << "process Role Conflict";

            auto controlling = packet->getAttribute<StunAttrIceControlling>();
            auto controlled = packet->getAttribute<StunAttrIceControlled>();
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

    auto srflx = packet->getAttribute<StunAttrXorMappedAddress>();
    if (!srflx) {
        WarnL << "Binding request missing XOR_MAPPED_ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
    }

    if (!pair->_relayed_addr) {
        //relay的消息不添加PRFLX candidaite
        CandidateInfo preflx_candidate;
        preflx_candidate._type = CandidateInfo::AddressType::PRFLX;
        preflx_candidate._addr._host = srflx->getIp();
        preflx_candidate._addr._port = srflx->getPort();
        preflx_candidate._base_addr._host = pair->get_local_ip();
        preflx_candidate._base_addr._port = pair->get_local_port();
        preflx_candidate._ufrag = getUfrag();
        preflx_candidate._pwd = getPassword();
        onGatheringCandidate(pair, preflx_candidate);
    }

    DebugL << "get candidate type preflx: " << srflx->getIp() << ":" << srflx->getPort();
    onConnected(pair);
}

void IceAgent::handleNominatedResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair, CandidateTuple& candidate) {
    // TraceL; 

    if (StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "fail, get response: " << packet->dumpString();
        if (packet->getErrorCode() == StunAttrErrorCode::Code::RoleConflict) {
            //角色冲突
            InfoL << "process Role Conflict";
            auto controlling = packet->getAttribute<StunAttrIceControlling>();
            auto controlled = packet->getAttribute<StunAttrIceControlled>();
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

    auto srflx = packet->getAttribute<StunAttrXorMappedAddress>();
    if (!srflx) {
        WarnL << "Binding request missing XOR_MAPPED_ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
    }

    onCompleted(pair);
}

void IceAgent::handleAllocateResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL; 

    if (StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "fail, get response: " << packet->dumpString() << ", errorCode: " << (uint16_t)packet->getErrorCode();
        if (packet->getErrorCode() == StunAttrErrorCode::Code::AllocationQuotaReached) {
            InfoL << "use stun retry";
        }
        return;
    }

    auto srflx = packet->getAttribute<StunAttrXorMappedAddress>();
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

    auto relay = packet->getAttribute<StunAttrXorRelayedAddress>();
    if (!relay) {
        WarnL << "Binding request missing XOR_RELAYED_ADDRESS attribute";
        sendErrorResponse(packet, pair, StunAttrErrorCode::Code::BadRequest);
    }

    CandidateInfo candidate;
    candidate._type = CandidateInfo::AddressType::RELAY;
    candidate._addr._host = relay->getIp();
    candidate._addr._port = relay->getPort();
    candidate._base_addr._host = relay->getIp();
    candidate._base_addr._port = relay->getPort();
    candidate._ufrag = getUfrag();
    candidate._pwd = getPassword();

    TraceL << "get local candidate type "  << candidate.dumpString()
           << ", by srflx addr " << srflx->getIp() << " : " << srflx->getPort()
           << ", by host addr " << pair->get_local_ip() << " : " << pair->get_local_port();
    onGatheringCandidate(pair, candidate);
}

void IceAgent::handleCreatePermissionResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair, const sockaddr_storage& peer_addr) {
    // TraceL; 

    if (StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "CreatePermission failed, response: " << packet->dumpString();
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

void IceAgent::handleChannelBindResponse(const StunPacket::Ptr& packet, const Pair::Ptr& pair, uint16_t channel_number, const sockaddr_storage& peer_addr) {
    // TraceL;

    if (StunPacket::Class::SUCCESS_RESPONSE != packet->getClass()) {
        WarnL << "ChannelBind failed, response: " << packet->dumpString();
        return;
    }

    InfoL << "ChannelBind success, channel_number=" << channel_number 
          << ", peer_addr=" << addrToStr(peer_addr)
          << ", pair: " << pair->dumpString(2);

    addChannelBind(channel_number, peer_addr);
}

void IceAgent::handleDataIndication(const StunPacket::Ptr& packet, const Pair::Ptr& pair) {
    // TraceL;

    // 检查必要的属性
    auto peer_addr = packet->getAttribute<StunAttrXorPeerAddress>();
    auto data = packet->getAttribute<StunAttrData>();

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

    DebugL << "Received Data indication from peer: " << addrToStr(addr) << ", size: " << buffer.size();

    // 通知上层收到数据
    pair->_relayed_addr = std::make_shared<sockaddr_storage>();
    memcpy(pair->_relayed_addr.get(), &addr, sizeof(addr));
    _listener->onIceTransportRecvData(recv_buffer, pair);
}

void IceAgent::handleChannelData(uint16_t channel_number, const char* data, size_t len, const Pair::Ptr& pair) {
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

    auto channel_pair = std::make_shared<Pair>(*pair);
    channel_pair->_relayed_addr = std::make_shared<sockaddr_storage>();
    memcpy(channel_pair->_relayed_addr.get(), &addr, sizeof(addr));
    _listener->onIceTransportRecvData(buffer, channel_pair);
}

void IceAgent::onGatheringCandidate(const Pair::Ptr& pair, CandidateInfo& candidate) {
    candidate._priority = calIceCandidatePriority(candidate._type);
    InfoL << "got candidate "  << candidate.dumpString();

    // 使用_socket_candidate_manager替代_local_candidates进行5元组重复检查
    if (!_socket_candidate_manager.addMapping(pair->_socket, candidate)) {
        InfoL << "has same 5 tuple, skip";
        return;
    }

    _listener->onIceTransportGatheringCandidate(pair, candidate);

    //如果是REALY,当前的所有PEER Candidate进行CreatePermission
    if (candidate._type == CandidateInfo::AddressType::RELAY) {
        _socket_candidate_manager._has_relayed_candidate = true;
        for (auto remote_candidate : _remote_candidates) {
            localRelayedConnectivityCheck(remote_candidate);
        }
    }
}

void IceAgent::onConnected(const IceTransport::Pair::Ptr& pair) {
    DebugL << "get connectivity check pair: " << pair->dumpString(2);

    if (getState() != State::Running) {
        InfoL << "ice state: "<< stateToString(getState()) << " is not running, skip";
        return;
    }

    TraceL << "checklist size: " << _check_list.size();
    //判断ConnectivityCheck的pair是否在checklist中,存在的话加入到validlist
    for (auto &candidate_pair : _check_list) {
        auto &pair_it = candidate_pair->_local_pair;
        auto &remote_candidate = candidate_pair->_remote_candidate;
        auto &state = candidate_pair->_state;

        TraceL << "check pair " << candidate_pair->dumpString() << ", pair info: " << pair_it->dumpString(2);

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

        InfoL << "push " << candidate_pair->dumpString() << " to valid_list";

        // 直接将候选者对添加到valid_list
        _valid_list.push_back(candidate_pair);

        if (getRole() == Role::Controlling) {
            if (getState() != IceAgent::State::Nominated && getState() != IceAgent::State::Completed) {
                //TODO:need process priority
                setState(IceAgent::State::Nominated);
                nominated(pair, remote_candidate);
            }
        }
    }

    if (getRole() == Role::Controlled && _nominated_pair) {
        onCompleted(_nominated_pair);
    }
}

void IceAgent::onCompleted(const IceTransport::Pair::Ptr& pair) {
    // TraceL;
    bool found_in_valid_list = false;
    if (getImplementation() == Implementation::Full) {
        for (auto &candidate_pair : _valid_list) {
            auto &pair_it = candidate_pair->_local_pair;
            if (Pair::is_same(pair_it.get(), pair.get())) {
                candidate_pair->_nominated = true;
                _select_candidate_pair = candidate_pair;
                InfoL << "select pair: " << candidate_pair->dumpString();
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

    if (found_in_valid_list) { 

        if (setSelectedPair(pair)) {

            if (getState() != IceAgent::State::Completed) {
                setState(IceAgent::State::Completed);
            }

            _listener->onIceTransportCompleted();
            _nominated_pair = nullptr;
        }

        if (_nominated_response) {
            sendPacket(_nominated_response, pair);
            _nominated_response = nullptr;
        }
    }
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

bool IceAgent::setSelectedPair(const Pair::Ptr& pair) {
    if (_selected_pair && Pair::is_same(pair.get(), _selected_pair.get())){
        return false;
    }
    
    if (_selected_pair) {
        InfoL << "Previous selected_pair: " << _selected_pair->dumpString(2);
        InfoL << "New selected_pair: " << pair->dumpString(2);
    } else {
        InfoL << "Initial selected_pair: " << pair->dumpString(2);
    }
    
    _last_selected_pair = std::static_pointer_cast<Pair>(_selected_pair);
    _selected_pair = pair;
    return true;
}

void IceAgent::removePair(const toolkit::SocketHelper *socket) {
    // TODO
}

std::vector<IceAgent::Pair::Ptr> IceAgent::getPairs() const {
    // TODO
    if (_selected_pair) {
        return { _selected_pair };
    }
    return {};
}

void IceAgent::sendSocketData(const Buffer::Ptr& buf, const Pair::Ptr& pair, bool flush) {
    auto use_pair = pair? pair : getSelectedPair();

    if (use_pair == nullptr) {
        WarnL << "pair should not be nullptr";
        return;
    }

    if (use_pair->_relayed_addr) {
        return sendRelayPacket(buf, use_pair, flush);
    }
    return sendSocketData_l(buf, use_pair, flush);
}

void IceAgent::sendRelayPacket(const Buffer::Ptr &buffer, const Pair::Ptr &pair, bool flush) {
    // TraceL;
    auto forward_pair = std::make_shared<Pair>(*pair);
    auto peer_addr = std::move(forward_pair->_relayed_addr);
    forward_pair->_relayed_addr = nullptr;

    if (!hasPermission(*peer_addr)) {
        WarnL << "No permission exists for peer: " << addrToStr(*peer_addr);
        return;
    }

    uint16_t channel_number;
    if (hasChannelBind(*peer_addr, channel_number)) {
        sendChannelData(channel_number, buffer, forward_pair);
    } else {
        sendSendIndication(*peer_addr, buffer, forward_pair);
    }
}

CandidateInfo IceAgent::getLocalCandidateInfo(const Pair::Ptr& pair) {
    // 从socket_candidate_manager中查找对应的本地候选者信息
    for (const auto& socket_candidates : _socket_candidate_manager.socket_to_candidates) {
        if (socket_candidates.first == pair->_socket) {
            // 找到对应socket的候选者列表，选择第一个(host类型)作为默认
            if (!socket_candidates.second.empty()) {
                return socket_candidates.second[0];
            }
        }
    }

    throw std::invalid_argument("No candidate found for the specified socket pair");
}

void IceAgent::addToChecklist(const Pair::Ptr& pair, CandidateInfo& remote_candidate) {
    try {
        //TODO: 优化checklist
        CandidateInfo local_candidate = getLocalCandidateInfo(pair);
        auto candidate_pair = std::make_shared<CandidatePair>(std::make_shared<Pair>(*pair), remote_candidate, local_candidate);
        candidate_pair->_state = CandidateInfo::State::InProgress;
        _check_list.push_back(candidate_pair);

        std::sort(_check_list.begin(), _check_list.end(), [] (
            const std::shared_ptr<CandidatePair>& a, const std::shared_ptr<CandidatePair>& b) {
            return *a < *b;
        });

        InfoL << "connectivity check candidate pair " << candidate_pair->dumpString() << ", pair info: " << pair->dumpString(2);

        connectivityCheck(std::make_shared<Pair>(*pair), remote_candidate);
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }
}

void IceTransport::checkRequestTimeouts() {
    uint64_t now = toolkit::getCurrentMillisecond();
    GET_CONFIG(uint32_t, max_retry, kMaxStunRetry);
    for (auto it = _response_handlers.begin(); it != _response_handlers.end();) {
        auto& transaction_id = it->first;
        auto& req_info = it->second;
        
        // 检查是否超时
        if (now >= req_info._next_timeout) {
            if (req_info._retry_count >= max_retry) {
                // 超过最大重传次数，放弃请求并清理
                WarnL << "STUN request timeout after " << max_retry 
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
        candidate_info["type"] = CandidateInfo::getAddressTypeStr(local_candidate._type);
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
        candidate_info["type"] = CandidateInfo::getAddressTypeStr(remote_candidate._type);
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
    for (const auto& candidate_pair : _check_list) {
        Json::Value entry;
        entry["candidate_pair"] = candidate_pair->_local_candidate.dumpString() + " <-> " + candidate_pair->_remote_candidate.dumpString();
        entry["state"] = CandidateInfo::getStateStr(candidate_pair->_state);
        entry["priority"] = (Json::UInt64)candidate_pair->_priority;
        entry["nominated"] = candidate_pair->_nominated;
        checklist_array.append(entry);
    }
 
    result["checklists"] = checklist_array;
    result["checklists_count"] = (int)_check_list.size();
    result["ice_state"] = stateToString(_state);
    
    if (_selected_pair) {
        Json::Value selected_pair;
        selected_pair["local_addr"] = _selected_pair->get_local_ip() + ":" + std::to_string(_selected_pair->get_local_port());
        selected_pair["remote_addr"] = _selected_pair->get_peer_ip() + ":" + std::to_string(_selected_pair->get_peer_port());
        if (!_selected_pair->get_relayed_ip().empty()) {
            selected_pair["relayed_addr"] = _selected_pair->get_relayed_ip() + ":" + std::to_string(_selected_pair->get_relayed_port());
        }

        if (_select_candidate_pair) {
            selected_pair["candidate_pair"] = _select_candidate_pair->_local_candidate.dumpString() + " <-> " + _select_candidate_pair->_remote_candidate.dumpString();
        }
        
        result["selected_pair"] = selected_pair;
    } else {
        result["selected_pair"] = Json::nullValue;
    }
    return result;
}

size_t IceAgent::getRecvSpeed() {
    size_t ret = 0;
    for (auto s : _socket_candidate_manager.getAllSockets()) {
        if (s && s->getSock()) {
            ret += s->getSock()->getRecvSpeed();
        }
    }
    return ret;
}

size_t IceAgent::getRecvTotalBytes() {
    size_t ret = 0;
    for (auto s : _socket_candidate_manager.getAllSockets()) {
        if (s && s->getSock()) {
            ret += s->getSock()->getRecvTotalBytes();
        }
    }
    return ret;
}

size_t IceAgent::getSendSpeed() {
    size_t ret = 0;
    for (auto s : _socket_candidate_manager.getAllSockets()) {
        if (s && s->getSock()) {
            ret += s->getSock()->getSendSpeed();
        }
    }
    return ret;
}

size_t IceAgent::getSendTotalBytes() {
    size_t ret = 0;
    for (auto s : _socket_candidate_manager.getAllSockets()) {
        if (s && s->getSock()) {
            ret += s->getSock()->getSendTotalBytes();
        }
    }
    return ret;
}
} // namespace RTC
