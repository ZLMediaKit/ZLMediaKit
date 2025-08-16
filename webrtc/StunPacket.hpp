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

#ifndef MS_RTC_STUN_PACKET_HPP
#define MS_RTC_STUN_PACKET_HPP


#include "logger.h"
#include "Util/Byte.hpp"
#include "Network/Buffer.h"
#include "Network/sockutil.h"
#include <string>

namespace RTC {
// reference https://rcf-editor.org/rfc/rfc8489
// reference https://rcf-editor.org/rfc/rfc8656
// reference https://rcf-editor.org/rfc/rfc8445

////////////  Attribute //////////////////////////
// reference https://rcf-editor.org/rfc/rfc8489
/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|         Type                  |            Length             |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Value (variable)                ....
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        Figure 4: Format of STUN Attributes
        reference https://www.rfc-editor.org/rfc/rfc8489.html#section-14
*/
class StunAttribute {
public:
    // Attribute type.
    enum class Type : uint16_t {
        MAPPED_ADDRESS           = 0x0001,
        RESPONSE_ADDRESS         = 0x0002, // Reserved; was RESPONSE-ADDRESS prior to [RFC5389]
        CHANGE_REQUEST           = 0x0003, // Reserved; was CHANGE-REQUEST prior to [RFC5389]
        CHANGED_ADDRESS          = 0x0005, // Reserved; was CHANGED-ADDRESS prior to [RFC5389]
        USERNAME                 = 0x0006,
        PASSWORD                 = 0x0005, // Reserved; was PASSWORD prior to [RFC5389]
        MESSAGE_INTEGRITY        = 0x0008,
        ERROR_CODE               = 0x0009,
        UNKNOWN_ATTRIBUTES       = 0x000A,
        REFLECTED_FROM           = 0x000B, // Reserved; was REFLECTED-FROM prior to [RFC5389]
        CHANNEL_NUMBER           = 0x000C, // [RFC5766]
        LIFETIME                 = 0x000D, // [RFC5766]
        BANDWIDTH                = 0x0010, // Reserved; [RFC5766]
        XOR_PEER_ADDRESS         = 0x0012, // [RFC5766]
        DATA                     = 0x0013, // [RFC5766]
        REALM                    = 0x0014,
        NONCE                    = 0x0015,
        XOR_RELAYED_ADDRESS      = 0x0016, // [RFC5766]
        EVEN_PORT                = 0x0018, // [RFC5766]
        REQUESTED_TRANSPORT      = 0x0019, // [RFC5766]
        DONT_FRAGMENT            = 0x001A, // [RFC5766]
        MESSAGE_INTEGRITY_SHA256 = 0x001C,
        USERHASH                 = 0x001E,
        PASSWORD_ALGORITHM       = 0x001D,
        XOR_MAPPED_ADDRESS       = 0x0020,
        TIMER_VAL                = 0x0021, // Reserved; [RFC5766]
        RESERVATION_TOKEN        = 0x0022, // [RFC5766]
        PRIORITY                 = 0x0024,
        USE_CANDIDATE            = 0x0025,

        //Comprehension-optional range (0x8000-0xFFFF)
        PASSWORD_ALGORITHMS      = 0x8002,
        ALTERNATE_DOMAIN         = 0x8003,
        SOFTWARE                 = 0x8022,
        ALTERNATE_SERVER         = 0x8023,
        FINGERPRINT              = 0x8028,
        ICE_CONTROLLED           = 0x8029,
        ICE_CONTROLLING          = 0x802A,
        GOOG_NETWORK_INFO        = 0xC057,
    };

    static const size_t ATTR_HEADER_SIZE = 4;
    static bool isComprehensionRequired(const uint8_t* data, size_t len);

    using Ptr = std::shared_ptr<StunAttribute>;
    StunAttribute(StunAttribute::Type type) : _type(type) {}
    virtual ~StunAttribute() = default;

    char *data()  {
        return _data? _data->data() : nullptr;
    }

    size_t size() {
        return _data? _data->size() : 0;
    }

    Type type() const {
        return _type;
    }

    virtual bool loadFromData(const uint8_t *buf, size_t len) = 0;
    virtual bool storeToData() = 0;
    // virtual std::string dump() = 0;

protected:
    void loadHeader(const uint8_t *buf);
    void storeHeader();

protected:
    Type _type;
    uint16_t _content_length;
    toolkit::BufferRaw::Ptr _data;
};

/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|0 0 0 0 0 0 0 0|    Family     |           Port                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                 Address (32 bits or 128 bits)                 |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        Figure 5: Format of MAPPED-ADDRESS Attribute
        reference https://www.rfc-editor.org/rfc/rfc8489.html#page-37
*/
class StunAttrMappedAddress : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrMappedAddress>;
    StunAttrMappedAddress() : StunAttribute(StunAttribute::Type::MAPPED_ADDRESS) {};
    virtual ~StunAttrMappedAddress() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;
};

class StunAttrUserName : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrUserName>;
    StunAttrUserName() : StunAttribute(StunAttribute::Type::USERNAME) {};
    virtual ~StunAttrUserName() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;

    void setUsername(const std::string username) {
        _username = username;
    }

    std::string getUsername() {
        return _username;
    }

private:
    std::string _username;
};

class StunAttrMessageIntegrity : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrMessageIntegrity>;
    StunAttrMessageIntegrity() : StunAttribute(StunAttribute::Type::MESSAGE_INTEGRITY) {};
    virtual ~StunAttrMessageIntegrity() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;
    //
    void setHmac(toolkit::BufferLikeString hmac) {
        _hmac = hmac;
    }

    toolkit::BufferLikeString getHmac() {
        return _hmac;
    }

    toolkit::BufferLikeString _hmac;
};

class StunAttrErrorCode : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrErrorCode>;
    StunAttrErrorCode() : StunAttribute(StunAttribute::Type::ERROR_CODE) {};
    virtual ~StunAttrErrorCode() = default;
    enum class Code : uint16_t {
        Invalid                     = 0,   //
        TryAlternate                = 300, //尝试备用服务器
        BadRequest                  = 400,
        Unauthorized                = 401,
        Forbidden                   = 403, //禁止
        RequestTimedOut             = 408, //请求超时(客户端认为此事务已经失败)
        UnknownAttribute            = 420,
        AllocationMismatch          = 438,
        StaleNonce                  = 438, //NONCE 不再有效,客户端应使用响应中的NONCE重试
        AddressFamilyNotSupported   = 440, //不支持的协议簇
        WrongCredentials            = 441, //凭据错误
        UnsupportedTransportAddress = 442, //不支持的传输地址
        AllocationQuotaReached      = 486, //alloction 达到上限,客户端应该至少等待一分钟后重新尝试创建
        RoleConflict                = 487, //角色冲突
        ServerError                 = 500, //服务器临时错误，客户端应重试
        InsuficientCapacity         = 508, //容量不足,没有更多可用的中继传输地址
    };

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;
    //
 
    void setErrorCode(Code error_code) {
        _error_code = error_code;
    }

    Code getErrorCode() {
        return _error_code;
    }

    Code _error_code;
};

class StunAttrChannelNumber : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrChannelNumber>;
    StunAttrChannelNumber() : StunAttribute(StunAttribute::Type::CHANNEL_NUMBER) {};
    virtual ~StunAttrChannelNumber() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;

    void setChannelNumber(uint16_t channel_number) {
        _channel_number = channel_number;
    }

    uint16_t getChannelNumber() {
        return _channel_number;
    }

private:
    uint16_t _channel_number;
};

class StunAttrLifeTime : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrLifeTime>;
    StunAttrLifeTime() : StunAttribute(StunAttribute::Type::LIFETIME) {};
    ~StunAttrLifeTime() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;

    void setLifetime(uint32_t lifetime) {
        _lifetime = lifetime;
    }

    uint32_t getLifetime() {
        return _lifetime;
    }

private:
    uint32_t _lifetime;
};

/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|0 0 0 0 0 0 0 0|    Family     |         X-Port                |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                X-Address (Variable)
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        Figure 6: Format of XOR-MAPPED-ADDRESS Attribute
        reference https://www.rfc-editor.org/rfc/rfc8489.html#page-38
*/
class StunAttrXorPeerAddress : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrXorPeerAddress>;
    StunAttrXorPeerAddress(toolkit::BufferLikeString transaction_id) : StunAttribute(StunAttribute::Type::XOR_PEER_ADDRESS), _transaction_id(transaction_id) {};
    virtual ~StunAttrXorPeerAddress() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;
 
    void setAddr(struct sockaddr_storage addr) {
        _addr = addr;
    }

    struct sockaddr_storage getAddr() {
        return _addr;
    }

    std::string getAddrString() {
        return toolkit::SockUtil::inet_ntoa((struct sockaddr *)&_addr);
    }

    uint16_t getPort() {
        return toolkit::SockUtil::inet_port((struct sockaddr *)&_addr);
    }

protected:
    struct sockaddr_storage _addr;
    toolkit::BufferLikeString _transaction_id;
};

class StunAttrData : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrData>;
    StunAttrData() : StunAttribute(StunAttribute::Type::DATA) {};
    virtual ~StunAttrData() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;

    void setData(const toolkit::BufferLikeString &data) {
        _data_content = data;
    }

    toolkit::BufferLikeString getData() {
        return _data_content;
    }

private:
    toolkit::BufferLikeString _data_content;
};

class StunAttrRealm : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrRealm>;
    StunAttrRealm() : StunAttribute(StunAttribute::Type::REALM) {};
    virtual ~StunAttrRealm() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;

    void setRealm(toolkit::BufferLikeString realm) {
        _realm = realm;
    }

    toolkit::BufferLikeString getRealm() {
        return _realm;
    }

private:
    //必须要少于128字符
    toolkit::BufferLikeString _realm;
};

class StunAttrNonce : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrNonce>;
    StunAttrNonce() : StunAttribute(StunAttribute::Type::NONCE) {};
    virtual ~StunAttrNonce() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;

    void setNonce(toolkit::BufferLikeString nonce) {
        _nonce = nonce;
    }

    toolkit::BufferLikeString getNonce() {
        return _nonce;
    }

private:
    //必须要少于128字符
    toolkit::BufferLikeString _nonce;
};

class StunAttrXorRelayedAddress : public StunAttrXorPeerAddress {
public:
    using Ptr = std::shared_ptr<StunAttrXorRelayedAddress>;
    StunAttrXorRelayedAddress(toolkit::BufferLikeString transaction_id) : StunAttrXorPeerAddress(transaction_id) {
        _type = StunAttribute::Type::XOR_RELAYED_ADDRESS;
    }
    virtual ~StunAttrXorRelayedAddress() = default;
};

/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|    Protocol   |                    RFFU                       |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        reference https://www.rfc-editor.org/rfc/rfc5766.html#section-14.7
*/
class StunAttrRequestedTransport : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrRequestedTransport>;
    StunAttrRequestedTransport() : StunAttribute(StunAttribute::Type::REQUESTED_TRANSPORT) {};
    virtual ~StunAttrRequestedTransport() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;
 
    enum class Protocol : uint8_t {
        //This specification only allows the use of codepoint 17 (User Datagram Protocol).
        UDP = 0x11,
    };

    void setProtocol(Protocol protocol) {
        _protocol = protocol;
    }

    Protocol getProtocol() {
        return _protocol;
    }

private:
    Protocol _protocol = Protocol::UDP;
};

class StunAttrXorMappedAddress : public StunAttrXorPeerAddress {
public:
    using Ptr = std::shared_ptr<StunAttrXorPeerAddress>;
    StunAttrXorMappedAddress(toolkit::BufferLikeString transaction_id) : StunAttrXorPeerAddress(transaction_id) {
        _type = StunAttribute::Type::XOR_MAPPED_ADDRESS;
    }
    virtual ~StunAttrXorMappedAddress() = default;
};

class StunAttrPriority : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrPriority>;
    StunAttrPriority() : StunAttribute(StunAttribute::Type::PRIORITY) {};
    virtual ~StunAttrPriority() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;

    void setPriority(uint64_t priority) {
        _priority = priority;
    }

    uint64_t getPriority() {
        return _priority;
    }

private:
    uint32_t _priority;
};

class StunAttrUseCandidate : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrUseCandidate>;
    StunAttrUseCandidate() : StunAttribute(StunAttribute::Type::USE_CANDIDATE) {};
    virtual ~StunAttrUseCandidate() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;
};

class StunAttrFingerprint : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrFingerprint>;
    StunAttrFingerprint() : StunAttribute(StunAttribute::Type::FINGERPRINT) {};
    virtual ~StunAttrFingerprint() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;
 
    void setFingerprint(uint32_t fingerprint) {
        _fingerprint = fingerprint;
    }

    uint32_t getFingerprint() {
        return _fingerprint;
    }

private:
    uint32_t _fingerprint;
};

class StunAttrIceControlled : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrIceControlled>;
    StunAttrIceControlled() : StunAttribute(StunAttribute::Type::ICE_CONTROLLED) {};
    virtual ~StunAttrIceControlled() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;
 
    void setTiebreaker(uint64_t tiebreaker) {
        _tiebreaker = tiebreaker;
    }

    uint64_t getTiebreaker() {
        return _tiebreaker;
    }
private:
    uint64_t _tiebreaker = 0; // 8 bytes unsigned integer.
};

class StunAttrIceControlling : public StunAttribute {
public:
    using Ptr = std::shared_ptr<StunAttrIceControlling>;
    StunAttrIceControlling() : StunAttribute(StunAttribute::Type::ICE_CONTROLLING) {};
    virtual ~StunAttrIceControlling() = default;

    bool loadFromData(const uint8_t *buf, size_t len) override;
    bool storeToData() override;
    // std::string dump() override;

    void setTiebreaker(uint64_t tiebreaker) {
        _tiebreaker = tiebreaker;
    }

    uint64_t getTiebreaker() {
        return _tiebreaker;
    }

private:
    uint64_t _tiebreaker = 0; // 8 bytes unsigned integer.
};

////////////  STUN //////////////////////////
/*
0                   1                   2                   3
0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|0 0|     STUN Message Type     |         Message Length        |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                         Magic Cookie                          |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
|                                                               |
|                     Transaction ID (96 bits)                  |
|                                                               |
+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
        Figure 2: Format of STUN Message Header
        reference https://www.rfc-editor.org/rfc/rfc8489.html#section-5 */ 
class StunPacket : public toolkit::Buffer {
public:
    using Ptr = std::shared_ptr<StunPacket>;

    // STUN message class.
    enum class Class : uint8_t {
        REQUEST          = 0,
        INDICATION       = 1,
        SUCCESS_RESPONSE = 2,
        ERROR_RESPONSE   = 3
    };

    // STUN message method.
    enum class Method : uint16_t {
        BINDING = 0x001,

        //TURN Extended
        //https://www.rfc-editor.org/rfc/rfc5766.html#section-13
        ALLOCATE         = 0x003, //  (only request/response semantics defined)
        REFRESH          = 0x004, //  (only request/response semantics defined)
        SEND             = 0x006, //  (only indication semantics defined)
        DATA             = 0x007, //  (only indication semantics defined)
        CREATEPERMISSION = 0x008, //  (only request/response semantics defined
        CHANNELBIND      = 0x009, //  (only request/response semantics defined)
    };

    // Authentication result.
    enum class Authentication {
        OK           = 0,
        UNAUTHORIZED = 1,
        BAD_REQUEST  = 2
    };

    struct EnumClassHash {
        template<typename T>
        std::size_t operator()(T t) const {
            return static_cast<std::size_t>(t);
        }
    };
    struct ClassMethodHash {
        bool operator()(std::pair<StunPacket::Class, StunPacket::Method> key) const {
            std::size_t h = 0;
            h ^= std::hash<uint8_t>()((uint8_t)key.first) << 1;
            h ^= std::hash<uint8_t>()((uint8_t)key.second) << 2;
            return h;
        }
    };

    static const size_t HEADER_SIZE = 20;
    static const uint8_t _magicCookie[];

    static bool isStun(const uint8_t* data, size_t len);
    static Class getClass(const uint8_t* data, size_t len);
    static Method getMethod(const uint8_t* data, size_t len);
    static StunPacket::Ptr parse(const uint8_t* data, size_t len);
    static std::string mappingClassEnum2Str(Class klass);
    static std::string mappingMethodEnum2Str(Method method);


    StunPacket(Class klass, Method method);
    virtual ~StunPacket();

    Class getClass() const {
        return _klass;
    }

    Method getMethod() const {
        return _method;
    }

    std::string getClassStr() {
        return StrPrinter << mappingClassEnum2Str(_klass) << "(" << (uint32_t)_klass << ")";
    }

    std::string getMethodStr() {
        return StrPrinter << mappingMethodEnum2Str(_method) << "(" << (uint32_t)_method << ")";
    }

    const toolkit::BufferLikeString getTransactionId() {
        return _transaction_id;
    }

    void setUfrag(const std::string& ufrag) {
        _ufrag = ufrag;
    }

    const std::string getUfrag() {
        return _ufrag;
    }

    void setPassword(const std::string& password) {
        _password = password;
    }

    const std::string getPassword() {
        return _password;
    }

    void setPeerUfrag(const std::string& peer_ufrag) {
        _peer_ufrag = peer_ufrag;
    }

    const std::string getPeerUfrag() {
        return _peer_ufrag;
    }

    void setPeerPassword(const std::string& peer_password) {
        _peer_password = peer_password;
    }

    const std::string getPeerPassword() {
        return _peer_password;
    }

    void setNeedMessageIntegrity(bool flag) {
        _need_message_integrity = flag;
    }

    bool getNeedMessageIntegrity() {
        return _need_message_integrity;
    }

    void setNeedFingerprint(bool flag) {
        _need_fingerprint = flag;
    }

    bool getNeedFingerprint() {
        return _need_fingerprint;
    }

    void refreshTransactionId() {
        _transaction_id = toolkit::makeRandStr(12, false);
        return;
    }

    void addAttribute(StunAttribute::Ptr attr);
    void removeAttribute(StunAttribute::Type type);
    bool hasAttribute(StunAttribute::Type type);
    StunAttribute::Ptr getAttribute(StunAttribute::Type type);

    const std::string getUsername();
    uint64_t getPriority();
    StunAttrErrorCode::Code getErrorCode() const;

    Authentication checkAuthentication(const std::string& ufrag, const std::string& password);
    void serialize();

    StunPacket::Ptr createSuccessResponse();
    StunPacket::Ptr createErrorResponse(StunAttrErrorCode::Code errorCode);

    ///////Buffer override///////
    char *data() const override;
    size_t size() const override;

private:

    bool loadFromData(const uint8_t *buf, size_t len);

    //attribute
    bool loadAttrMessage(const uint8_t *buf, size_t len);
    bool storeAttrMessage();
    size_t getAttrSize();

protected:

    Class                         _klass;
    Method                        _method;
    toolkit::BufferLikeString     _transaction_id; // 12 bytes/96bits.
    std::map<StunAttribute::Type, StunAttribute::Ptr> _attribute_map;
    toolkit::BufferRaw::Ptr       _data;
    std::string                   _ufrag;
    std::string                   _password;
    std::string                   _peer_ufrag;
    std::string                   _peer_password;
    size_t                        _message_integrity_data_len = 0; //MESSAGE_INTEGRITY属性之前的字段
 
    bool _need_message_integrity = true;
    bool _need_fingerprint = true;
};

class BindingPacket : public StunPacket {
public:
    BindingPacket() : StunPacket(Class::REQUEST, Method::BINDING) {};
    virtual ~BindingPacket() {};
};

class SuccessResponsePacket : public StunPacket {
public:
    SuccessResponsePacket(Method method, toolkit::BufferLikeString transaction_id);
    virtual ~SuccessResponsePacket() {};

};

class ErrorResponsePacket : public StunPacket {
public:
    ErrorResponsePacket(Method method, toolkit::BufferLikeString transaction_id, StunAttrErrorCode::Code error_code);
    virtual ~ErrorResponsePacket() {};
};

////////////  TURN //////////////////////////

class TurnPacket : public StunPacket {
public:
    TurnPacket(Class klass, Method method) : StunPacket(klass, method) {};
    virtual ~TurnPacket() {};
};

class AllocatePacket : public TurnPacket {
public:
    AllocatePacket() : TurnPacket(Class::REQUEST, Method::ALLOCATE) {};
    virtual ~AllocatePacket() {};
};

class RefreshPacket : public TurnPacket {
public:
    RefreshPacket() : TurnPacket(Class::REQUEST, Method::REFRESH) {};
    virtual ~RefreshPacket() {};
};

class CreatePermissionPacket : public TurnPacket {
public:
    CreatePermissionPacket() : TurnPacket(Class::REQUEST, Method::CREATEPERMISSION) {};
    virtual ~CreatePermissionPacket() {};
};

class ChannelBindPacket : public TurnPacket {
public:
    ChannelBindPacket() : TurnPacket(Class::REQUEST, Method::CHANNELBIND) {};
    virtual ~ChannelBindPacket() {};
};

class SendIndicationPacket: public TurnPacket {
public:
    SendIndicationPacket() : TurnPacket(Class::INDICATION, Method::SEND) {};
    virtual ~SendIndicationPacket() {};
};

class DataIndicationPacket: public TurnPacket {
public:
    DataIndicationPacket() : TurnPacket(Class::INDICATION, Method::DATA) {};
    virtual ~DataIndicationPacket() {};
};

class DataPacket : public TurnPacket {
public:
    DataPacket() : TurnPacket(Class::INDICATION, Method::DATA) {};
    virtual ~DataPacket() {};
};


} // namespace RTC

#endif
