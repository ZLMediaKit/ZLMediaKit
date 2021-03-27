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

//v=0
//o=- 2584450093346841581 2 IN IP4 127.0.0.1
//s=-
//t=0 0
//a=group:BUNDLE audio video data
//a=msid-semantic: WMS 616cfbb1-33a3-4d8c-8275-a199d6005549
//m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126
//c=IN IP4 0.0.0.0
//a=rtcp:9 IN IP4 0.0.0.0
//a=ice-ufrag:sXJ3
//a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV
//a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79
//a=setup:actpass
//a=mid:audio
//a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level
//a=sendrecv
//a=rtcp-mux
//a=rtpmap:111 opus/48000/2
//a=rtcp-fb:111 transport-cc
//a=fmtp:111 minptime=10;useinbandfec=1
//a=rtpmap:103 ISAC/16000
//a=rtpmap:104 ISAC/32000
//a=rtpmap:9 G722/8000
//a=rtpmap:0 PCMU/8000
//a=rtpmap:8 PCMA/8000
//a=rtpmap:106 CN/32000
//a=rtpmap:105 CN/16000
//a=rtpmap:13 CN/8000
//a=rtpmap:110 telephone-event/48000
//a=rtpmap:112 telephone-event/32000
//a=rtpmap:113 telephone-event/16000
//a=rtpmap:126 telephone-event/8000
//a=ssrc:120276603 cname:iSkJ2vn5cYYubTve
//a=ssrc:120276603 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 1da3d329-7399-4fe9-b20f-69606bebd363
//a=ssrc:120276603 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549
//a=ssrc:120276603 label:1da3d329-7399-4fe9-b20f-69606bebd363
//m=video 9 UDP/TLS/RTP/SAVPF 96 98 100 102 127 97 99 101 125
//c=IN IP4 0.0.0.0
//a=rtcp:9 IN IP4 0.0.0.0
//a=ice-ufrag:sXJ3
//a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV
//a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79
//a=setup:actpass
//a=mid:video
//a=extmap:2 urn:ietf:params:rtp-hdrext:toffset
//a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time
//a=extmap:4 urn:3gpp:video-orientation
//a=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01
//a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay
//a=sendrecv
//a=rtcp-mux
//a=rtcp-rsize
//a=rtpmap:96 VP8/90000
//a=rtcp-fb:96 ccm fir
//a=rtcp-fb:96 nack
//a=rtcp-fb:96 nack pli
//a=rtcp-fb:96 goog-remb
//a=rtcp-fb:96 transport-cc
//a=rtpmap:98 VP9/90000
//a=rtcp-fb:98 ccm fir
//a=rtcp-fb:98 nack
//a=rtcp-fb:98 nack pli
//a=rtcp-fb:98 goog-remb
//a=rtcp-fb:98 transport-cc
//a=rtpmap:100 H264/90000
//a=rtcp-fb:100 ccm fir
//a=rtcp-fb:100 nack
//a=rtcp-fb:100 nack pli
//a=rtcp-fb:100 goog-remb
//a=rtcp-fb:100 transport-cc
//a=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f
//a=rtpmap:102 red/90000
//a=rtpmap:127 ulpfec/90000
//a=rtpmap:97 rtx/90000
//a=fmtp:97 apt=96
//a=rtpmap:99 rtx/90000
//a=fmtp:99 apt=98
//a=rtpmap:101 rtx/90000
//a=fmtp:101 apt=100
//a=rtpmap:125 rtx/90000
//a=fmtp:125 apt=102
//a=ssrc-group:FID 2580761338 611523443
//a=ssrc:2580761338 cname:iSkJ2vn5cYYubTve
//a=ssrc:2580761338 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 bf270496-a23e-47b5-b901-ef23096cd961
//a=ssrc:2580761338 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549
//a=ssrc:2580761338 label:bf270496-a23e-47b5-b901-ef23096cd961
//a=ssrc:611523443 cname:iSkJ2vn5cYYubTve
//a=ssrc:611523443 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 bf270496-a23e-47b5-b901-ef23096cd961
//a=ssrc:611523443 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549
//a=ssrc:611523443 label:bf270496-a23e-47b5-b901-ef23096cd961
//m=application 9 DTLS/SCTP 5000
//c=IN IP4 0.0.0.0
//a=ice-ufrag:sXJ3
//a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV
//a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79
//a=setup:actpass
//a=mid:data
//a=sctpmap:5000 webrtc-datachannel 1024

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
    revonly,
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

class SdpItem {
public:
    SdpItem() = default;
    virtual ~SdpItem() = default;
    virtual void parse(const string &str) = 0;
    virtual string toString() const = 0;
    virtual const char* getKey() = 0;
};

class SdpTime : public SdpItem{
public:
    //5.9.  Timing ("t=")
    // t=<start-time> <stop-time>
    uint64_t start;
    uint64_t stop;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "t";}
};

class SdpOrigin : public SdpItem{
public:
    // 5.2.  Origin ("o=")
    // o=jdoe 2890844526 2890842807 IN IP4 10.47.16.5
    // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
    string username;
    string session_id;
    string session_version;
    string nettype;
    string addrtype;
    string address;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "o";}
};

class SdpConnection : public SdpItem {
public:
    // 5.7.  Connection Data ("c=")
    // c=IN IP4 224.2.17.12/127
    // c=<nettype> <addrtype> <connection-address>
    string nettype;
    string addrtype;
    string address;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "c";}
};

class SdpBandwidth : public SdpItem {
public:
    //5.8.  Bandwidth ("b=")
    //b=<bwtype>:<bandwidth>

    //AS、CT
    string bwtype;
    int bandwidth;

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
    vector<string> proto;
    vector<uint8_t> fmt;

    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "m";}
};

class SdpAttr : public SdpItem{
public:
    //5.13.  Attributes ("a=")
    //a=<attribute>
    //a=<attribute>:<value>
    string name;
    string value;
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
    string type;
    vector<string> mid;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() override { return "group";}
};

class RtcMedia {
public:
    //m=<media> <port> <proto> <fmt> ...
    SdpMedia media;
    //c=<nettype> <addrtype> <connection-address>
    SdpConnection connection;
    //a=<attribute>:<value>
    vector<SdpAttr> attributes;

    bool haveAttr(const char *attr) const;
    string getAttrValue(const char *attr) const;
};

class RtcSdp {
public:
    /////Session description（会话级别描述）////
    //v=  (protocol version)
    int version;
    //o=  (session origin information )
    SdpOrigin origin;
    //s=  (session name)
    string session_name;
    //t=  (time the session is active)
    SdpTime time;

    //// 非必须 ////
    //i=* (session information)
    string information;
    //u=* (URI of description)
    string url;
    //e=* (email address)
    string email;
    //p=* (phone number)
    string phone;
    //c=* (connection information -- not required if included in all media)
    SdpConnection connection;
    //b=* (zero or more bandwidth information lines)
    SdpBandwidth bandwidth;
    //z=* (time zone adjustments)
    //z=<adjustment time> <offset> <adjustment time> <offset> ....
    string time_zone;
    //k=* (encryption key)
    //k=<method>, k=<method>:<encryption key>
    string crypt_key;
    //r=* (zero or more repeat times)
    //r=<repeat interval> <active duration> <offsets from start-time>
    string repeat;
    //a=* (zero or more media attribute lines)
    vector<SdpAttr> attributes;
};




#endif //ZLMEDIAKIT_SDP_H
