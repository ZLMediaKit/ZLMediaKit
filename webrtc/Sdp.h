//
// Created by xzl on 2021/3/27.
//

#ifndef ZLMEDIAKIT_SDP_H
#define ZLMEDIAKIT_SDP_H

#include <list>
#include <string>
using namespace std;

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

//
//v=0
//o=- 7268199939077294076 2 IN IP4 127.0.0.1
//s=-
//t=0 0
//a=group:BUNDLE video
//a=msid-semantic: WMS
//m=video 9 RTP/SAVPF 96
//c=IN IP4 0.0.0.0
//a=rtcp:9 IN IP4 0.0.0.0
//a=ice-ufrag:y94W
//a=ice-pwd:fuz1hZCAarezk35fruVGfdyP
//a=ice-options:trickle
//a=fingerprint:sha-256 FF:8C:29:B8:B3:2B:45:F5:21:D2:47:D5:EE:B7:F8:BB:F1:DC:95:47:7B:20:B4:59:75:0F:16:93:D0:AC:D2:73
//a=setup:active
//a=mid:video
//a=recvonly
//a=rtcp-mux
//a=rtpmap:96 H264/90000
//a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f

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

class SdpItem {
public:
    SdpItem() = default;
    virtual ~SdpItem() = default;
    virtual void parse(const string &str) = 0;
    virtual string toString() const = 0;
};

class SdpTime : public SdpItem{
public:
    float start;
    float end;
    void parse(const string &str) override;
    string toString() const override;
};

class SdpAttr : public SdpItem{
public:
    string name;
    string value;
    void parse(const string &str) override;
    string toString() const override;
};

class SdpOrigin : public SdpItem{
public:
    // https://datatracker.ietf.org/doc/rfc4566/?include_text=1 5.2.  Origin ("o=")
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
};

class SdpConnection : public SdpItem {
public:
    //  https://datatracker.ietf.org/doc/rfc4566/?include_text=1 5.7.  Connection Data ("c=")
    // c=IN IP4 224.2.17.12/127
    // c=<nettype> <addrtype> <connection-address>
    string nettype;
    string addrtype;
    string address;
    void parse(const string &str) override;
    string toString() const override;
};

class RtcMedia {
public:

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
    //i=* (session information)
    string session_information;
    //c=* (connection information -- not required if included in all media)
    SdpConnection connection;

    //// Time description ////
    //t=  (time the session is active)
    SdpTime time;
    //r=* (zero or more repeat times)
    int repeat;
    //a=* (zero or more media attribute lines)
    list<SdpAttr> attributes;

};




#endif //ZLMEDIAKIT_SDP_H
