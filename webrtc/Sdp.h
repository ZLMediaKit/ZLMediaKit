//
// Created by xzl on 2021/3/27.
//

#ifndef ZLMEDIAKIT_SDP_H
#define ZLMEDIAKIT_SDP_H

#include <string>
#include <vector>
#include "Extension/Frame.h"
using namespace std;
using namespace mediakit;

//https://datatracker.ietf.org/doc/rfc4566/?include_text=1
//Session description
//         v=  (protocol version)
//         o=  (originator and session identifier)
//         s=  (session name)
//         i=* (session information)
//         u=* (URI of description)
//         e=* (email address)
//         p=* (phone number)
//         c=* (connection information -- not required if included in
//              all media)
//         b=* (zero or more bandwidth information lines)
//         One or more time descriptions ("t=" and "r=" lines; see below)
//         z=* (time zone adjustments)
//         k=* (encryption key)
//         a=* (zero or more session attribute lines)
//         Zero or more media descriptions
//
//      Time description
//         t=  (time the session is active)
//         r=* (zero or more repeat times)
//
//      Media description, if present
//         m=  (media name and transport address)
//         i=* (media title)
//         c=* (connection information -- optional if included at
//              session level)
//         b=* (zero or more bandwidth information lines)
//         k=* (encryption key)
//         a=* (zero or more media attribute lines)

enum class RtpDirection {
    invalid = -1,
    //只发送
    sendonly,
    //只接收
    recvonly,
    //同时发送接收
    sendrecv,
    //禁止发送数据
    inactive
};

enum class DtlsRole {
    invalid = -1,
    //客户端
    active,
    //服务端
    passive,
    //既可作做客户端也可以做服务端
    actpass,
};

enum class SdpType {
    invalid = -1,
    offer,
    answer
};

TrackType getTrackType(const string &str);
const char* getTrackString(TrackType type);
DtlsRole getDtlsRole(const string &str);
const char* getDtlsRoleString(DtlsRole role);
RtpDirection getRtpDirection(const string &str);
const char* getRtpDirectionString(RtpDirection val);

class SdpItem {
public:
    using Ptr = std::shared_ptr<SdpItem>;
    virtual ~SdpItem() = default;
    virtual void parse(const string &str) {
        value  = str;
    }
    virtual string toString() const {
        return value;
    }
    virtual const char* getKey() = 0;

protected:
    mutable string value;
};

template <char KEY>
class SdpString : public SdpItem{
public:
    // *=*
    const char* getKey() override { static string key(1, KEY); return key.data();}
};

class SdpCommon : public SdpItem {
public:
    string key;
    SdpCommon(string key) { this->key = std::move(key); }
    const char* getKey() override { return key.data();}
};

class SdpTime : public SdpItem{
public:
    //5.9.  Timing ("t=")
    // t=<start-time> <stop-time>
    uint64_t start {0};
    uint64_t stop {0};
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "t";}
};

class SdpOrigin : public SdpItem{
public:
    // 5.2.  Origin ("o=")
    // o=jdoe 2890844526 2890842807 IN IP4 10.47.16.5
    // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
    string username {"-"};
    string session_id;
    string session_version;
    string nettype {"IN"};
    string addrtype {"IP4"};
    string address {"0.0.0.0"};
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "o";}
};

class SdpConnection : public SdpItem {
public:
    // 5.7.  Connection Data ("c=")
    // c=IN IP4 224.2.17.12/127
    // c=<nettype> <addrtype> <connection-address>
    string nettype {"IN"};
    string addrtype {"IP4"};
    string address {"0.0.0.0"};
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "c";}
};

class SdpBandwidth : public SdpItem {
public:
    //5.8.  Bandwidth ("b=")
    //b=<bwtype>:<bandwidth>

    //AS、CT
    string bwtype {"AS"};
    uint32_t bandwidth {0};

    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "b";}
};

class SdpMedia : public SdpItem {
public:
    // 5.14.  Media Descriptions ("m=")
    // m=<media> <port> <proto> <fmt> ...
    TrackType type;
    uint16_t port;
    string proto;
    vector<uint32_t> fmts;

    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "m";}
};

class SdpAttr : public SdpItem{
public:
    using Ptr = std::shared_ptr<SdpAttr>;
    //5.13.  Attributes ("a=")
    //a=<attribute>
    //a=<attribute>:<value>
    SdpItem::Ptr detail;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "a";}
};

class SdpAttrGroup : public SdpItem{
public:
    //a=group:BUNDLE line with all the 'mid' identifiers part of the
    //  BUNDLE group is included at the session-level.
    //a=group:LS session level attribute MUST be included wth the 'mid'
    //  identifiers that are part of the same lip sync group.
    string type {"BUNDLE"};
    vector<string> mids;
    void parse(const string &str) override ;
    string toString() const override ;
    const char* getKey() override { return "group";}
};

class SdpAttrMsidSemantic : public SdpItem {
public:
    //https://tools.ietf.org/html/draft-alvestrand-rtcweb-msid-02#section-3
    //3.  The Msid-Semantic Attribute
    //
    //   In order to fully reproduce the semantics of the SDP and SSRC
    //   grouping frameworks, a session-level attribute is defined for
    //   signalling the semantics associated with an msid grouping.
    //
    //   This OPTIONAL attribute gives the message ID and its group semantic.
    //     a=msid-semantic: examplefoo LS
    //
    //
    //   The ABNF of msid-semantic is:
    //
    //     msid-semantic-attr = "msid-semantic:" " " msid token
    //     token = <as defined in RFC 4566>
    //
    //   The semantic field may hold values from the IANA registries
    //   "Semantics for the "ssrc-group" SDP Attribute" and "Semantics for the
    //   "group" SDP Attribute".
    //a=msid-semantic: WMS 616cfbb1-33a3-4d8c-8275-a199d6005549
    string msid{"WMS"};
    string token;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "msid-semantic";}
};

class SdpAttrRtcp : public SdpItem {
public:
    // a=rtcp:9 IN IP4 0.0.0.0
    uint16_t port;
    string nettype {"IN"};
    string addrtype {"IP4"};
    string address {"0.0.0.0"};
    void parse(const string &str) override;;
    string toString() const override;
    const char* getKey() override { return "rtcp";}
};

class SdpAttrIceUfrag : public SdpItem {
public:
    //a=ice-ufrag:sXJ3
    const char* getKey() override { return "ice-ufrag";}
};

class SdpAttrIcePwd : public SdpItem {
public:
    //a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV
    const char* getKey() override { return "ice-pwd";}
};

class SdpAttrFingerprint : public SdpItem {
public:
    //a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79
    string algorithm;
    string hash;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "fingerprint";}
};

class SdpAttrSetup : public SdpItem {
public:
    //a=setup:actpass
    DtlsRole role{DtlsRole::actpass};
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "setup";}
};

class SdpAttrMid : public SdpItem {
public:
    //a=mid:audio
    const char* getKey() override { return "mid";}
};

class SdpAttrExtmap : public SdpItem {
public:
    //a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
    int index;
    string ext;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "extmap";}
};

class SdpAttrRtpMap : public SdpItem {
public:
    //a=rtpmap:111 opus/48000/2
    uint8_t pt;
    string codec;
    int sample_rate;
    int channel {0};
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "rtpmap";}
};

class SdpAttrRtcpFb : public SdpItem {
public:
   //a=rtcp-fb:98 nack pli
    uint8_t pt;
    vector<string> arr;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "rtcp-fb";}
};

class SdpAttrFmtp : public SdpItem {
public:
    //fmtp:96 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
    uint8_t pt;
    vector<std::pair<string, string> > arr;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "fmtp";}
};

class SdpAttrSSRC : public SdpItem {
public:
    //a=ssrc:120276603 cname:iSkJ2vn5cYYubTve
    //a=ssrc:<ssrc-id> <attribute>
    //a=ssrc:<ssrc-id> <attribute>:<value>
    uint32_t ssrc;
    string attribute;
    string attribute_value;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "ssrc";}
};

class SdpAttrSctpMap : public SdpItem {
public:
    //https://tools.ietf.org/html/draft-ietf-mmusic-sctp-sdp-05
    //a=sctpmap:5000 webrtc-datachannel 1024
    //a=sctpmap: sctpmap-number media-subtypes [streams]
    uint16_t port;
    string subtypes;
    int streams;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "sctpmap";}
};

class SdpAttrCandidate : public SdpItem {
public:
    //https://tools.ietf.org/html/rfc5245
    //15.1.  "candidate" Attribute
    //a=candidate:4 1 udp 2 192.168.1.7 58107 typ host
    //a=candidate:<foundation> <component-id> <transport> <priority> <address> <port> typ <cand-type>
    uint32_t foundation;
    uint32_t component;
    string transport {"udp"};
    uint32_t priority;
    string address;
    uint16_t port;
    string type;
    vector<std::pair<string, string> > arr;

    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "candidate";}
};

class RtcMedia {
public:
    vector<SdpItem::Ptr> items;
    string toString() const;
    bool haveAttr(const char *attr) const;
    string getAttrValue(const char *attr) const;
    RtpDirection getDirection() const;
};

class RtcSdp {
public:
    vector<SdpItem::Ptr> items;
    vector<RtcMedia> medias;

    void parse(const string &str);
    string toString() const;
};




#endif //ZLMEDIAKIT_SDP_H
