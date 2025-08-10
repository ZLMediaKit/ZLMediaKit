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

#ifndef MS_RTC_ICE_SERVER_HPP
#define MS_RTC_ICE_SERVER_HPP

#include "StunPacket.hpp"
#include "Util/Byte.hpp"
#include "Network/Session.h"
#include "logger.h"
#include <list>
#include <string>
#include <functional>
#include <memory>
#include <unordered_map>
#include <map>
#include <algorithm>
#include "Poller/EventPoller.h"
#include "Network/Socket.h"
#include "Network/UdpClient.h"
#include "Poller/Timer.h"
#include "json/json.h"

namespace RTC {

uint64_t calCandidatePairPriority(uint32_t G, uint32_t D);

class CandidateAddr {
public:
    std::string _host;
    uint16_t    _port = 0;
};


class CandidateTuple {
public:
    using Ptr = std::shared_ptr<CandidateTuple>;
    CandidateTuple() = default;
    virtual ~CandidateTuple() = default;

    enum class AddressType {
        HOST = 1,
        SRFLX, //server reflexive
        PRFLX, //peer reflexive
        RELAY,
    };

    enum class SecureType {
        NOT_SECURE = 1,
        SECURE,
    };

    enum class TransportType {
        UDP = 1,
        TCP,
    };

    bool operator<(const CandidateTuple& rhs) const {
        return (_priority < rhs._priority);
    }

    bool operator==(const CandidateTuple& rhs) const {
        return ((_addr._host == rhs._addr._host) && (_addr._port == rhs._addr._port)
        && (_priority == rhs._priority)
        && (_transport == rhs._transport) && (_secure == rhs._secure));
    }

    struct ClassHash {
        std::size_t operator()(const CandidateTuple& t) const {
            std::string str = t._addr._host + std::to_string(t._addr._port) +
                std::to_string((uint32_t)t._transport) + std::to_string((uint32_t)t._secure);
            return std::hash<std::string>()(str);
        }
    };

    struct ClassEqual {
        bool operator()(const CandidateTuple& a, const CandidateTuple& b) const {
            return ((a._addr._host == b._addr._host)
                    && (a._addr._port == b._addr._port)
                    &&(a._transport == b._transport)
                    &&(a._secure == b._secure));
        }
    };

public:
    CandidateAddr _addr;
    uint32_t      _priority  = 0;
    TransportType _transport = TransportType::UDP;
    SecureType    _secure    = SecureType::NOT_SECURE;
    std::string   _ufrag;
    std::string   _pwd;
};

class CandidateInfo : public CandidateTuple {
public:
    using Ptr = std::shared_ptr<CandidateInfo>;
    CandidateInfo() = default;
    virtual ~CandidateInfo() = default;

    enum class AddressType {
        INVALID = 0,
        HOST = 1,
        SRFLX,  // server reflx
        PRFLX,  // peer reflx
        RELAY,
    };

    enum class State {
        Frozen = 1,         //尚未check,并还不需要check
        Waiting,            //尚未发送check,但也不是Frozen
        InProgress,         //已经发起check,但是仍在进行中
        Succeeded,          //check success
        Failed,             //check failed
    };

    bool operator==(const CandidateInfo& rhs) const {
        return ((_addr._host == rhs._addr._host) && (_addr._port == rhs._addr._port)
        && (_type == rhs._type) && (_priority == rhs._priority)
        && (_transport == rhs._transport) && (_secure == rhs._secure));
    }
    std::string getAddressTypeStr() {
        switch (_type) {
            case AddressType::HOST: return "host";
            case AddressType::SRFLX: return "srflx";
            case AddressType::PRFLX: return "reflx";
            case AddressType::RELAY: return "relay";
            default: break;
        }
        return "invalid";
    }

    static std::string getStateStr(State state) {
        switch (state) {
            case State::Frozen: return "frozen";
            case State::Waiting: return "waiting";
            case State::InProgress: return "in_progress";
            case State::Succeeded: return "succeeded";
            case State::Failed: return "failed";
            default: break;
        }
        return "unknown";
    }

public:
    AddressType _type = AddressType::HOST;
    CandidateAddr _base_addr;
};

class IceServerInfo : public CandidateTuple {
public:
    using Ptr = std::shared_ptr<IceServerInfo>;
    IceServerInfo() = default;
    virtual ~IceServerInfo() = default;
    IceServerInfo(const std::string &url) { parse(url); }
    void parse(const std::string &url);

    enum class SchemaType {
        TURN = 1,
        STUN,
    };

public:
    std::string   _full_url;
    std::string   _param_strs;
    SchemaType    _schema = SchemaType::TURN;
};


class IceTransport : public std::enable_shared_from_this<IceTransport> {
public:
    using Ptr = std::shared_ptr<IceTransport>;

    class Pair {
    public:
        using Ptr = std::shared_ptr<Pair>;

        Pair() = default;
        Pair(toolkit::SocketHelper::Ptr socket) : _socket(socket) {;}
        Pair(toolkit::SocketHelper::Ptr socket, std::string peer_host, uint16_t peer_port,
             std::shared_ptr<sockaddr_storage> realyed_addr = nullptr) : 
            _socket(socket), _peer_host(peer_host), _peer_port(peer_port), _realyed_addr(realyed_addr) {
        }

        Pair(Pair &that) {
            // DebugL;
            _socket = that._socket;
            _peer_host = that._peer_host;
            _peer_port = that._peer_port;
            _realyed_addr = nullptr;
            if (that._realyed_addr) {
                // DebugL;
                _realyed_addr = std::make_shared<sockaddr_storage>();
                memcpy(_realyed_addr.get(), that._realyed_addr.get(), sizeof(sockaddr_storage));
            }
        }
        virtual ~Pair() = default;

        void get_peer_addr(sockaddr_storage &peer_addr) {
            memset(&peer_addr, 0, sizeof(peer_addr));
            sockaddr_storage dummy_peer_addr;
            if (!_peer_host.empty()) {
                dummy_peer_addr = toolkit::SockUtil::make_sockaddr(_peer_host.data(), _peer_port);
            } else {
                dummy_peer_addr = toolkit::SockUtil::make_sockaddr(_socket->get_peer_ip().data(), _socket->get_peer_port());
            }
            memcpy(&peer_addr, &dummy_peer_addr, sizeof(peer_addr));
        }

        bool get_realyed_addr(sockaddr_storage &peerAddr) {
            if (!_realyed_addr) {
                return false;
            }

            memset(&peerAddr, 0, sizeof(peerAddr));
            memcpy(&peerAddr, _realyed_addr.get(), sizeof(peerAddr));
            return true;
        }

        std::string get_local_ip() {
            return _socket->get_local_ip();
        }

        uint16_t get_local_port() {
            return _socket->get_local_port();
        };

        std::string get_peer_ip() {
            return !_peer_host.empty()? _peer_host : _socket->get_peer_ip();
        }

        uint16_t get_peer_port() {
            return !_peer_host.empty()? _peer_port : _socket->get_peer_port();
        }

        std::string get_realyed_ip() {
            if (_realyed_addr) {
                return toolkit::SockUtil::inet_ntoa((const struct sockaddr*)_realyed_addr.get());
            }
            return "";
        }

        uint16_t get_realyed_port() {
            if (_realyed_addr) {
                return toolkit::SockUtil::inet_port((const struct sockaddr*)_realyed_addr.get());
            }
            return 0;
        };

        static bool is_same_realyed_addr(Pair* a, Pair* b) {
            if (a->_realyed_addr != nullptr && b->_realyed_addr != nullptr) {
                return toolkit::SockUtil::is_same_addr(reinterpret_cast<const struct sockaddr*>(a->_realyed_addr.get()), 
                reinterpret_cast<const struct sockaddr*>(b->_realyed_addr.get()));
            }
            return (a->_realyed_addr == b->_realyed_addr);
        }

        static bool is_same(Pair* a, Pair* b) {
            if ((a->_socket == b->_socket)
                && (a->get_peer_ip() == b->get_peer_ip())
                && (a->get_peer_port() == b->get_peer_port()) 
                && (is_same_realyed_addr(a, b))) {
                return true;
            }
            return false;
        }

        toolkit::SocketHelper::Ptr _socket;
        std::string _peer_host;
        uint16_t _peer_port;

        std::shared_ptr<sockaddr_storage> _realyed_addr = nullptr;
    };

    class Listener {
    public:
        virtual ~Listener() = default;

    public:
        virtual void onIceTransportRecvData(const toolkit::Buffer::Ptr& buffer, Pair::Ptr pair) = 0;
        virtual void onIceTransportGatheringCandidate(Pair::Ptr, CandidateInfo) = 0;
        virtual void onIceTransportDisconnected() = 0;
        virtual void onIceTransportCompleted() = 0;
    };

public:
    using MsgHandler = std::function<void(StunPacket::Ptr, Pair::Ptr)>;
    
    struct RequestInfo {
        StunPacket::Ptr _request;        // 原始请求包
        MsgHandler _handler;             // 响应处理函数
        Pair::Ptr _pair;                 // 发送对
        uint64_t _send_time;             // 首次发送时间(毫秒)
        uint64_t _next_timeout;          // 下次超时时间(毫秒)
        uint32_t _retry_count;           // 当前重传次数
        uint32_t _rto;                   // 当前RTO值(毫秒)

        static const uint32_t INITIAL_RTO = 500;    // 初始RTO 500ms
        static const uint32_t MAX_RETRIES = 7;      // 最大重传次数
        
        RequestInfo(StunPacket::Ptr req, MsgHandler h, Pair::Ptr p)
            : _request(req), _handler(h), _pair(p), _retry_count(0), _rto(INITIAL_RTO) {
            _send_time = toolkit::getCurrentMillisecond();
            _next_timeout = _send_time + _rto;
        }
    };

    IceTransport(Listener* listener, const std::string& ufrag, const std::string& password, const toolkit::EventPoller::Ptr &poller);
    virtual ~IceTransport() {}
    
    // 初始化方法，必须在构造完成后调用
    virtual void initialize();

    const toolkit::EventPoller::Ptr& getPoller() const { return _poller; }
    const std::string& getIdentifier() const { return _identifier; }

    const std::string& getUfrag() const { return _ufrag; }
    const std::string& getPassword() const { return _password; }
    void setUFrag(const std::string& ufrag) { _ufrag = ufrag; }
    void setPassword(const std::string& password) { _password = password; }

    virtual bool processSocketData(const uint8_t* data, size_t len, Pair::Ptr pair);
    virtual void sendSocketData(toolkit::Buffer::Ptr buf, Pair::Ptr pair, bool flush = true);
    void sendSocketData_l(toolkit::Buffer::Ptr buf, Pair::Ptr pair, bool flush = true);

protected:
    virtual void processStunPacket(const StunPacket::Ptr packet, Pair::Ptr pair);
    virtual void processRequest(const StunPacket::Ptr packet, Pair::Ptr pair);
    virtual void processResponse(const StunPacket::Ptr packet, Pair::Ptr pair);
    virtual bool processChannelData(const uint8_t* data, size_t len, Pair::Ptr pair);
    virtual StunPacket::Authentication checkRequestAuthentication(const StunPacket::Ptr packet, Pair::Ptr pair);
    StunPacket::Authentication checkResponseAuthentication(const StunPacket::Ptr request, const StunPacket::Ptr packet, Pair::Ptr pair);
    void processUnauthorizedResponse(const StunPacket::Ptr response, StunPacket::Ptr request, Pair::Ptr pair, MsgHandler handler);
    virtual void handleBindingRequest(const StunPacket::Ptr packet, Pair::Ptr pair);
    virtual void handleChannelData(uint16_t channel_number, const char* data, size_t len, Pair::Ptr pair) {};

    void sendChannelData(uint16_t channel_number, const toolkit::Buffer::Ptr &buffer, Pair::Ptr pair);
    virtual void sendUnauthorizedResponse(const StunPacket::Ptr packet, Pair::Ptr pair);
    void sendErrorResponse(const StunPacket::Ptr packet, Pair::Ptr pair, StunAttrErrorCode::Code errorCode);
    void sendRequest(const StunPacket::Ptr packet, Pair::Ptr pair, MsgHandler handler);
    void sendPacket(const StunPacket::Ptr packet, Pair::Ptr pair);

    // For permissions
    bool hasPermission(const sockaddr_storage& addr);
    void addPermission(const sockaddr_storage& addr);
 
    // For Channel Bind
    bool hasChannelBind(uint16_t channel_number);
    bool hasChannelBind(const sockaddr_storage& addr, uint16_t& channel_number);
    void addChannelBind(uint16_t channel_number, const sockaddr_storage& addr);

    toolkit::SocketHelper::Ptr createSocket(CandidateTuple::TransportType type, const std::string &peer_host, uint16_t peer_port, const std::string &local_ip, uint16_t local_port = 0);
    toolkit::SocketHelper::Ptr createUdpSocket(const std::string &target_host, uint16_t peer_port, const std::string &local_ip, uint16_t local_port);
    
    void checkRequestTimeouts();
    void retransmitRequest(const std::string& transaction_id, RequestInfo& req_info);

protected:
    std::string _identifier;
    toolkit::EventPoller::Ptr _poller;
    Listener* _listener = nullptr;
    std::unordered_map<std::string /*transcation ID*/, RequestInfo> _response_handlers;
    std::unordered_map<std::pair<StunPacket::Class, StunPacket::Method>, MsgHandler, StunPacket::ClassMethodHash> _request_handlers;

    // for local
    std::string _ufrag;
    std::string _password;

    // For permissions
    std::unordered_map<sockaddr_storage /*peer ip:port*/, uint64_t /* create or fresh time*/, 
        toolkit::SockUtil::SockAddrHash, toolkit::SockUtil::SockAddrEqual> _permissions;

    // For Channel Bind
    std::unordered_map<uint16_t /*channel number*/, sockaddr_storage /*peer ip:port*/> _channel_bindings;
    std::unordered_map<uint16_t /*channel number*/, uint64_t /*bind or fresh time*/> _channel_binding_times;
    
    // For STUN request retry
    std::shared_ptr<toolkit::Timer> _retry_timer;
};

class IceServer : public IceTransport {
public:
    using Ptr = std::shared_ptr<IceServer>;
    using WeakPtr = std::weak_ptr<IceServer>;
    IceServer(Listener* listener, const std::string& ufrag, const std::string& password, const toolkit::EventPoller::Ptr &poller);
    virtual ~IceServer() {}
    
    void initialize() override;
    bool processSocketData(const uint8_t* data, size_t len, Pair::Ptr pair) override;
    void relayForwordingData(const toolkit::Buffer::Ptr& buffer, struct sockaddr_storage peer_addr);
    void relayBackingData(const toolkit::Buffer::Ptr& buffer, Pair::Ptr pair, struct sockaddr_storage peer_addr);

protected:
    void processRealyPacket(const toolkit::Buffer::Ptr &buffer, Pair::Ptr pair);
    void handleAllocateRequest(const StunPacket::Ptr packet, Pair::Ptr pair);
    void handleRefreshRequest(const StunPacket::Ptr packet, Pair::Ptr pair);
    void handleCreatePermissionRequest(const StunPacket::Ptr packet, Pair::Ptr pair);
    void handleChannelbindRequest(const StunPacket::Ptr packet, Pair::Ptr pair);
    void handleSendIndication(const StunPacket::Ptr packet, Pair::Ptr pair);
    void handleChannelData(uint16_t channel_number, const char* data, size_t len, Pair::Ptr pair) override;

    StunPacket::Authentication checkRequestAuthentication(const StunPacket::Ptr packet, Pair::Ptr pair) override;

    void sendDataIndication(const sockaddr_storage& peer_addr, const toolkit::Buffer::Ptr &buffer, Pair::Ptr pair);
    void sendUnauthorizedResponse(const StunPacket::Ptr packet, Pair::Ptr pair) override;

    toolkit::SocketHelper::Ptr allocateRealyed(Pair::Ptr pair);
    toolkit::SocketHelper::Ptr createRealyedUdpSocket(const std::string &peer_host, uint16_t peer_port, const std::string &local_ip, uint16_t local_port);

protected:
    std::vector<toolkit::BufferLikeString> _nonce_list;

    std::unordered_map<sockaddr_storage /*peer ip:port*/, std::pair<std::shared_ptr<uint16_t> /* port */, Pair::Ptr /*realyed_pairs*/>,
        toolkit::SockUtil::SockAddrHash, toolkit::SockUtil::SockAddrEqual> _realyed_pairs;
    Pair::Ptr _session_pair = nullptr;
};

class IceAgent : public IceTransport {

public:
    using Ptr = std::shared_ptr<IceAgent>;
    
    // 候选者对信息结构
    struct CandidatePair {
        Pair::Ptr _local_pair;                // 本地候选者对
        CandidateInfo _remote_candidate;      // 远程候选者信息
        CandidateInfo _local_candidate;       // 本地候选者信息
        uint64_t _priority;                   // 候选者对优先级（64位，符合RFC 8445）
        CandidateInfo::State _state;          // 连通性检查状态
        bool _nominated = false;
        
        CandidatePair(Pair::Ptr local_pair, const CandidateInfo& remote, const CandidateInfo& local) 
            : _local_pair(local_pair), _remote_candidate(remote), _local_candidate(local), _state(CandidateInfo::State::Frozen) {
            _priority = calCandidatePairPriority(local._priority, remote._priority);
        }
        
        // 比较操作符，用于优先级排序（高优先级在前）
        bool operator<(const CandidatePair& other) const {
            return _priority > other._priority;
        }
    };

    enum class State {
        //checklist state and ice session state
        Running = 1,        //正在进行候选地址的连通性检测
        Completed,          //所有候选地址完成验证,且至少有一路连接检测成功
        Failed,             //所有候选地址检测失败,连接不可用
    };

    static const char* stateToString(State state) {
        switch (state) {
            case State::Running: return "Running";
            case State::Completed: return "Completed";
            case State::Failed: return "Failed";
            default: return "Unknown";
        }
    }

    enum class Role {
        Controlling = 1,
        Controlled,
    };

    enum class Implementation {
        Lite = 1,
        Full,
    };

    IceAgent(Listener* listener, Implementation implementation, Role role, 
             const std::string& ufrag, const std::string& password, 
             const toolkit::EventPoller::Ptr &poller);
    virtual ~IceAgent() {}
    
    void initialize() override;

    void setIceServer(IceServerInfo::Ptr ice_server) {
        _ice_server = ice_server;
    }

    void gatheringCandidate(CandidateTuple::Ptr candidate_tuple, bool gathering_rflx, bool gathering_realy);
    void connectivityCheck(CandidateInfo candidate);
    void Checks(CandidateInfo candidate);
    void nominated(Pair::Ptr pair, CandidateTuple candidate);

    void sendSocketData(toolkit::Buffer::Ptr buf, Pair::Ptr pair, bool flush = true) override;

    IceAgent::Implementation getImplementation() const {
        return _implementation;
    }

    void setgetImplementation(IceAgent::Implementation implementation) { 
        InfoL << (uint32_t)implementation;
        _implementation = implementation;
    }

    IceAgent::Role getRole() const {
        return _role;
    }

    void setRole(IceAgent::Role role) { 
        InfoL << (uint32_t)role;
        _role = role;
    }

    IceAgent::State getState() const {
        return _state;
    }

    void setState(IceAgent::State state) { 
        InfoL << stateToString(state);
        _state = state;
    }

    Pair::Ptr getSelectedPair(bool try_last = false) const {
        return try_last ?  _last_selected_pair.lock() : _selected_pair;
    }
    void setSelectedPair(Pair::Ptr pair);

    // 获取checklist信息，用于API查询
    Json::Value getChecklistInfo() const;

protected:
    void gatheringSrflxCandidate(Pair::Ptr pair);
    void gatheringRealyCandidate(Pair::Ptr pair);
    void localRealyedConnectivityCheck(CandidateInfo candidate);
    void connectivityCheck(Pair::Ptr pair, CandidateTuple candidate);
    void tryTriggerredCheck(Pair::Ptr pair);

    void sendBindRequest(Pair::Ptr pair, MsgHandler handler);
    void sendBindRequest(Pair::Ptr pair, CandidateTuple candidate, bool use_candidate, MsgHandler handler);
    void sendAllocateRequest(Pair::Ptr pair);
    void sendCreatePermissionRequest(Pair::Ptr pair, const sockaddr_storage& peer_addr);
    void sendChannelBindRequest(Pair::Ptr pair, uint16_t channel_number, const sockaddr_storage& peer_addr);

    void processRequest(const StunPacket::Ptr packet, Pair::Ptr pair) override;

    void handleBindingRequest(const StunPacket::Ptr packet, Pair::Ptr pair) override;
    void handleGatheringCandidateResponse(const StunPacket::Ptr packet, Pair::Ptr pair);
    void handleConnectivityCheckResponse(const StunPacket::Ptr packet, Pair::Ptr pair, CandidateTuple candidate);
    void handleNominatedResponse(const StunPacket::Ptr packet, Pair::Ptr pair, CandidateTuple candidate);
    void handleAllocateResponse(const StunPacket::Ptr packet, Pair::Ptr pair);
    void handleCreatePermissionResponse(const StunPacket::Ptr packet, Pair::Ptr pair, const sockaddr_storage peer_addr);
    void handleChannelBindResponse(const StunPacket::Ptr packet, Pair::Ptr pair, uint16_t channel_number, const sockaddr_storage peer_addr);
    void handleDataIndication(const StunPacket::Ptr packet, Pair::Ptr pair);
    void handleChannelData(uint16_t channel_number, const char* data, size_t len, Pair::Ptr pair) override;

    void onGatheringCandidate(Pair::Ptr pair, CandidateInfo candidate);
    void onConnected(Pair::Ptr pair);
    void onCompleted(Pair::Ptr pair);

    void refreshPermissions();
    void refreshChannelBindings();

    void sendSendIndication(const sockaddr_storage& peer_addr, toolkit::Buffer::Ptr buffer, Pair::Ptr pair);
    void sendRealyPacket(toolkit::Buffer::Ptr buffer, Pair::Ptr pair, bool flush);

private:

    CandidateInfo getLocalCandidateInfo(Pair::Ptr local_pair);
    void addToChecklist(Pair::Ptr local_pair, CandidateInfo& remote_candidate);

protected:
    IceServerInfo::Ptr _ice_server;

    std::shared_ptr<toolkit::Timer> _refresh_timer;

    // for candidate

    Implementation _implementation = Implementation::Full;
    Role  _role  = Role::Controlling;            //ice role
    uint64_t _tiebreaker = 0;                    // 8 bytes unsigned integer.
    State _state = IceAgent::State::Running;     //ice session state
 
    Pair::Ptr _selected_pair = nullptr;
    Pair::Ptr _nominated_pair = nullptr;
    StunPacket::Ptr _nominated_response = nullptr;
    std::weak_ptr<Pair>  _last_selected_pair;

    // 双向索引的候选地址管理结构
    struct SocketCandidateManager {
        // socket -> candidates 的一对多映射
        std::unordered_map<toolkit::SocketHelper::Ptr, std::vector<CandidateInfo>> socket_to_candidates;
        
        // candidate -> socket 的映射（用于快速查找）
        std::unordered_map<CandidateInfo, toolkit::SocketHelper::Ptr, CandidateTuple::ClassHash, CandidateTuple::ClassEqual> candidate_to_socket;
        
        // 按类型分组的socket列表，方便遍历
        std::vector<toolkit::SocketHelper::Ptr> _host_sockets;    // HOST类型socket
        std::vector<toolkit::SocketHelper::Ptr> _relay_sockets;   // RELAY类型socket

        bool _has_realyed_cnadidate = false;
        
        // 添加映射关系，带5元组重复检查
        bool addMapping(toolkit::SocketHelper::Ptr socket, const CandidateInfo& candidate) {
            // 检查5元组是否已存在
            if (candidate_to_socket.find(candidate) != candidate_to_socket.end()) {
                return false; // 已存在相同的5元组
            }
            
            socket_to_candidates[socket].push_back(candidate);
            candidate_to_socket[candidate] = socket;
            
            // 按类型分组
            if (candidate._type != CandidateInfo::AddressType::RELAY) {
                if (std::find(_host_sockets.begin(), _host_sockets.end(), socket) == _host_sockets.end()) {
                    _host_sockets.push_back(socket);
                }
            } else if (candidate._type == CandidateInfo::AddressType::RELAY) {
                if (std::find(_relay_sockets.begin(), _relay_sockets.end(), socket) == _relay_sockets.end()) {
                    _relay_sockets.push_back(socket);
                }
            }
            
            return true;
        }
        
        // 获取socket对应的所有candidates
        std::vector<CandidateInfo> getCandidates(toolkit::SocketHelper::Ptr socket) const {
            auto it = socket_to_candidates.find(socket);
            return (it != socket_to_candidates.end()) ? it->second : std::vector<CandidateInfo>();
        }
        
        // 获取candidate对应的socket
        toolkit::SocketHelper::Ptr getSocket(const CandidateInfo& candidate) const {
            auto it = candidate_to_socket.find(candidate);
            return (it != candidate_to_socket.end()) ? it->second : nullptr;
        }
        
        // 获取所有socket（便于遍历）
        std::vector<toolkit::SocketHelper::Ptr> getAllSockets() const {
            std::vector<toolkit::SocketHelper::Ptr> result;
            result.reserve(_host_sockets.size() + _relay_sockets.size());
            result.insert(result.end(), _host_sockets.begin(), _host_sockets.end());
            result.insert(result.end(), _relay_sockets.begin(), _relay_sockets.end());
            return result;
        }
        
        // 获取所有candidates（便于遍历）
        std::vector<CandidateInfo> getAllCandidates() const {
            std::vector<CandidateInfo> result;
            for (const auto& pair : candidate_to_socket) {
                result.push_back(pair.first);
            }
            return result;
        }
        
        // 直接添加host socket
        void addHostSocket(toolkit::SocketHelper::Ptr socket) {
            if (std::find(_host_sockets.begin(), _host_sockets.end(), socket) == _host_sockets.end()) {
                _host_sockets.push_back(socket);
            }
        }
        
        // 直接添加relay socket
        void addRelaySocket(toolkit::SocketHelper::Ptr socket) {
            if (std::find(_relay_sockets.begin(), _relay_sockets.end(), socket) == _relay_sockets.end()) {
                _relay_sockets.push_back(socket);
            }
        }
        
        // 获取host sockets
        const std::vector<toolkit::SocketHelper::Ptr>& getHostSockets() const {
            return _host_sockets;
        }
        
        // 获取relay sockets
        const std::vector<toolkit::SocketHelper::Ptr>& getRelaySockets() const {
            return _relay_sockets;
        }
        
        // 移除host socket
        void removeHostSocket(toolkit::SocketHelper::Ptr socket) {
            auto it = std::find(_host_sockets.begin(), _host_sockets.end(), socket);
            if (it != _host_sockets.end()) {
                _host_sockets.erase(it);
            }
        }
        
        // 移除relay socket
        void removeRelaySocket(toolkit::SocketHelper::Ptr socket) {
            auto it = std::find(_relay_sockets.begin(), _relay_sockets.end(), socket);
            if (it != _relay_sockets.end()) {
                _relay_sockets.erase(it);
            }
        }
        
        // 清空host sockets
        void clearHostSockets() {
            _host_sockets.clear();
        }
        
        // 清空relay sockets
        void clearRelaySockets() {
            _relay_sockets.clear();
        }
        
        // 获取host socket数量
        size_t getHostSocketCount() const {
            return _host_sockets.size();
        }
        
        // 获取relay socket数量
        size_t getRelaySocketCount() const {
            return _relay_sockets.size();
        }
    };

    //for GATHERING_CANDIDATE
    SocketCandidateManager _socket_candidate_manager; //local candidates

    //for CONNECTIVITY_CHECK
    using CandidateSet = std::unordered_set<CandidateInfo, CandidateTuple::ClassHash, CandidateTuple::ClassEqual>;
    CandidateSet _remote_candidates;

    //TODO:当前仅支持多数据流复用一个checklist
    std::vector<std::shared_ptr<CandidatePair>> _checklist;
    std::vector<std::shared_ptr<CandidatePair>> _valid_list;

};

} // namespace RTC
#endif //MS_RTC_ICE_SERVER_HPP
