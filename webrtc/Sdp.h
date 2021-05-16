/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_SDP_H
#define ZLMEDIAKIT_SDP_H

#include <string>
#include <vector>
#include "RtpExt.h"
#include "assert.h"
#include "Extension/Frame.h"
#include "Common/Parser.h"
using namespace std;
using namespace mediakit;

//https://datatracker.ietf.org/doc/rfc4566/?include_text=1
//https://blog.csdn.net/aggresss/article/details/109850434
//https://aggresss.blog.csdn.net/article/details/106436703
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
    virtual const char* getKey() const = 0;

    void reset() {
        value.clear();
    }

protected:
    mutable string value;
};

template <char KEY>
class SdpString : public SdpItem{
public:
    SdpString() = default;
    SdpString(string val) {value = std::move(val);}
    // *=*
    const char* getKey() const override { static string key(1, KEY); return key.data();}
};

class SdpCommon : public SdpItem {
public:
    string key;
    SdpCommon(string key) { this->key = std::move(key); }
    SdpCommon(string key, string val) {
        this->key = std::move(key);
        this->value = std::move(val);
    }

    const char* getKey() const override { return key.data();}
};

class SdpTime : public SdpItem{
public:
    //5.9.  Timing ("t=")
    // t=<start-time> <stop-time>
    uint64_t start {0};
    uint64_t stop {0};
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "t";}
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
    const char* getKey() const override { return "o";}
    bool empty() const {
        return username.empty() || session_id.empty() || session_version.empty()
               || nettype.empty() || addrtype.empty() || address.empty();
    }
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
    const char* getKey() const override { return "c";}
    bool empty() const {return address.empty();}
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
    const char* getKey() const override { return "b";}
    bool empty() const {return bandwidth == 0;}
};

class SdpMedia : public SdpItem {
public:
    // 5.14.  Media Descriptions ("m=")
    // m=<media> <port> <proto> <fmt> ...
    TrackType type;
    uint16_t port;
    //RTP/AVP：应用场景为视频/音频的 RTP 协议。参考 RFC 3551
    //RTP/SAVP：应用场景为视频/音频的 SRTP 协议。参考 RFC 3711
    //RTP/AVPF: 应用场景为视频/音频的 RTP 协议，支持 RTCP-based Feedback。参考 RFC 4585
    //RTP/SAVPF: 应用场景为视频/音频的 SRTP 协议，支持 RTCP-based Feedback。参考 RFC 5124
    string proto;
    vector<uint32_t> fmts;

    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "m";}
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
    const char* getKey() const override { return "a";}
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
    const char* getKey() const override { return "group";}
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
    const char* getKey() const override { return "msid-semantic";}
    bool empty() const {
        return msid.empty();
    }
};

class SdpAttrRtcp : public SdpItem {
public:
    // a=rtcp:9 IN IP4 0.0.0.0
    uint16_t port{0};
    string nettype {"IN"};
    string addrtype {"IP4"};
    string address {"0.0.0.0"};
    void parse(const string &str) override;;
    string toString() const override;
    const char* getKey() const override { return "rtcp";}
    bool empty() const {
        return address.empty() || !port;
    }
};

class SdpAttrIceUfrag : public SdpItem {
public:
    SdpAttrIceUfrag() = default;
    SdpAttrIceUfrag(string str) {value = std::move(str);}
    //a=ice-ufrag:sXJ3
    const char* getKey() const override { return "ice-ufrag";}
};

class SdpAttrIcePwd : public SdpItem {
public:
    SdpAttrIcePwd() = default;
    SdpAttrIcePwd(string str) {value = std::move(str);}
    //a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV
    const char* getKey() const override { return "ice-pwd";}
};

class SdpAttrIceOption : public SdpItem {
public:
    //a=ice-options:trickle
    bool trickle{false};
    bool renomination{false};
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "ice-options";}
};

class SdpAttrFingerprint : public SdpItem {
public:
    //a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79
    string algorithm;
    string hash;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "fingerprint";}
    bool empty() const { return algorithm.empty() || hash.empty(); }
};

class SdpAttrSetup : public SdpItem {
public:
    //a=setup:actpass
    SdpAttrSetup() = default;
    SdpAttrSetup(DtlsRole r) { role = r; }
    DtlsRole role{DtlsRole::actpass};
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "setup";}
};

class SdpAttrMid : public SdpItem {
public:
    SdpAttrMid() = default;
    SdpAttrMid(string val) { value = std::move(val); }
    //a=mid:audio
    const char* getKey() const override { return "mid";}
};

class SdpAttrExtmap : public SdpItem {
public:
    //https://aggresss.blog.csdn.net/article/details/106436703
    //a=extmap:1[/sendonly] urn:ietf:params:rtp-hdrext:ssrc-audio-level
    uint8_t id;
    RtpDirection direction{RtpDirection::invalid};
    string ext;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "extmap";}
};

class SdpAttrRtpMap : public SdpItem {
public:
    //a=rtpmap:111 opus/48000/2
    uint8_t pt;
    string codec;
    uint32_t sample_rate;
    uint32_t channel {0};
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "rtpmap";}
};

class SdpAttrRtcpFb : public SdpItem {
public:
    //a=rtcp-fb:98 nack pli
    //a=rtcp-fb:120 nack 支持 nack 重传，nack (Negative-Acknowledgment) 。
    //a=rtcp-fb:120 nack pli 支持 nack 关键帧重传，PLI (Picture Loss Indication) 。
    //a=rtcp-fb:120 ccm fir 支持编码层关键帧请求，CCM (Codec Control Message)，FIR (Full Intra Request )，通常与 nack pli 有同样的效果，但是 nack pli 是用于重传时的关键帧请求。
    //a=rtcp-fb:120 goog-remb 支持 REMB (Receiver Estimated Maximum Bitrate) 。
    //a=rtcp-fb:120 transport-cc 支持 TCC (Transport Congest Control) 。
    uint8_t pt;
    string rtcp_type;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "rtcp-fb";}
};

class SdpAttrFmtp : public SdpItem {
public:
    //fmtp:96 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f
    uint8_t pt;
    map<string/*key*/, string/*value*/, StrCaseCompare> fmtp;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "fmtp";}
};

class SdpAttrSSRC : public SdpItem {
public:
    //a=ssrc:3245185839 cname:Cx4i/VTR51etgjT7
    //a=ssrc:3245185839 msid:cb373bff-0fea-4edb-bc39-e49bb8e8e3b9 0cf7e597-36a2-4480-9796-69bf0955eef5
    //a=ssrc:3245185839 mslabel:cb373bff-0fea-4edb-bc39-e49bb8e8e3b9
    //a=ssrc:3245185839 label:0cf7e597-36a2-4480-9796-69bf0955eef5
    //a=ssrc:<ssrc-id> <attribute>
    //a=ssrc:<ssrc-id> <attribute>:<value>
    //cname 是必须的，msid/mslabel/label 这三个属性都是 WebRTC 自创的，或者说 Google 自创的，可以参考 https://tools.ietf.org/html/draft-ietf-mmusic-msid-17，
    // 理解它们三者的关系需要先了解三个概念：RTP stream / MediaStreamTrack / MediaStream ：
    //一个 a=ssrc 代表一个 RTP stream ；
    //一个 MediaStreamTrack 通常包含一个或多个 RTP stream，例如一个视频 MediaStreamTrack 中通常包含两个 RTP stream，一个用于常规传输，一个用于 nack 重传；
    //一个 MediaStream 通常包含一个或多个 MediaStreamTrack ，例如 simulcast 场景下，一个 MediaStream 通常会包含三个不同编码质量的 MediaStreamTrack ；
    //这种标记方式并不被 Firefox 认可，在 Firefox 生成的 SDP 中一个 a=ssrc 通常只有一行，例如：
    //a=ssrc:3245185839 cname:Cx4i/VTR51etgjT7

    uint32_t ssrc;
    string attribute;
    string attribute_value;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "ssrc";}
};

class SdpAttrSSRCGroup : public SdpItem {
public:
    //a=ssrc-group 定义参考 RFC 5576(https://tools.ietf.org/html/rfc5576) ，用于描述多个 ssrc 之间的关联，常见的有两种：
    //a=ssrc-group:FID 2430709021 3715850271
    // FID (Flow Identification) 最初用在 FEC 的关联中，WebRTC 中通常用于关联一组常规 RTP stream 和 重传 RTP stream 。
    //a=ssrc-group:SIM 360918977 360918978 360918980
    // 在 Chrome 独有的 SDP munging 风格的 simulcast 中使用，将三组编码质量由低到高的 MediaStreamTrack 关联在一起。
    string type{"FID"};
    vector<uint32_t> ssrcs;

    bool isFID() const { return type == "FID"; }
    bool isSIM() const { return type == "SIM"; }
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "ssrc-group";}
};

class SdpAttrSctpMap : public SdpItem {
public:
    //https://tools.ietf.org/html/draft-ietf-mmusic-sctp-sdp-05
    //a=sctpmap:5000 webrtc-datachannel 1024
    //a=sctpmap: sctpmap-number media-subtypes [streams]
    uint16_t port;
    string subtypes;
    uint32_t streams;
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "sctpmap";}
};

class SdpAttrCandidate : public SdpItem {
public:
    using Ptr = std::shared_ptr<SdpAttrCandidate>;
    //https://tools.ietf.org/html/rfc5245
    //15.1.  "candidate" Attribute
    //a=candidate:4 1 udp 2 192.168.1.7 58107 typ host
    //a=candidate:<foundation> <component-id> <transport> <priority> <address> <port> typ <cand-type>
    string foundation;
    //传输媒体的类型,1代表RTP;2代表 RTCP。
    uint32_t component;
    string transport {"udp"};
    uint32_t priority;
    string address;
    uint16_t port;
    string type;
    vector<std::pair<string, string> > arr;

    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "candidate";}
};

class SdpAttrMsid : public SdpItem{
public:
    const char* getKey() const override { return "msid";}
};

class SdpAttrExtmapAllowMixed : public SdpItem{
public:
    const char* getKey() const override { return "extmap-allow-mixed";}
};

class SdpAttrSimulcast : public SdpItem{
public:
    //https://www.meetecho.com/blog/simulcast-janus-ssrc/
    //https://tools.ietf.org/html/draft-ietf-mmusic-sdp-simulcast-14
    const char* getKey() const override { return "simulcast";}
    void parse(const string &str) override;
    string toString() const override;
    bool empty() const { return rids.empty(); }
    string direction;
    vector<string> rids;
};

class SdpAttrRid : public SdpItem{
public:
    void parse(const string &str) override;
    string toString() const override;
    const char* getKey() const override { return "rid";}
    string direction;
    string rid;
};

class RtcSdpBase {
public:
    vector<SdpItem::Ptr> items;

public:
    virtual ~RtcSdpBase() = default;
    virtual string toString() const;

    int getVersion() const;
    SdpOrigin getOrigin() const;
    string getSessionName() const;
    string getSessionInfo() const;
    SdpTime getSessionTime() const;
    SdpConnection getConnection() const;
    SdpBandwidth getBandwidth() const;

    string getUri() const;
    string getEmail() const;
    string getPhone() const;
    string getTimeZone() const;
    string getEncryptKey() const;
    string getRepeatTimes() const;
    RtpDirection getDirection() const;

    template<typename cls>
    cls getItemClass(char key, const char *attr_key = nullptr) const{
        auto item = dynamic_pointer_cast<cls>(getItem(key, attr_key));
        if (!item) {
            return cls();
        }
        return *item;
    }

    string getStringItem(char key, const char *attr_key = nullptr) const{
        auto item = getItem(key, attr_key);
        if (!item) {
            return "";
        }
        return item->toString();
    }

    SdpItem::Ptr getItem(char key, const char *attr_key = nullptr) const;

    template<typename cls>
    vector<cls> getAllItem(char key_c, const char *attr_key = nullptr) const {
        vector<cls> ret;
        for (auto item : items) {
            string key(1, key_c);
            if (strcasecmp(item->getKey(), key.data()) == 0) {
                if (!attr_key) {
                    auto c = dynamic_pointer_cast<cls>(item);
                    if (c) {
                        ret.emplace_back(*c);
                    }
                } else {
                    auto attr = dynamic_pointer_cast<SdpAttr>(item);
                    if (attr && !strcasecmp(attr->detail->getKey(), attr_key)) {
                        auto c = dynamic_pointer_cast<cls>(attr->detail);
                        if (c) {
                            ret.emplace_back(*c);
                        }
                    }
                }
            }
        }
        return ret;
    }
};

class RtcSessionSdp : public RtcSdpBase{
public:
    using Ptr = std::shared_ptr<RtcSessionSdp>;

    vector<RtcSdpBase> medias;
    void parse(const string &str);
    string toString() const override;
};

//////////////////////////////////////////////////////////////////

//ssrc相关信息
class RtcSSRC{
public:
    uint32_t ssrc {0};
    string cname;
    string msid;
    string mslabel;
    string label;

    bool empty() const {return ssrc == 0 && cname.empty();}
};

//rtc传输编码方案
class RtcCodecPlan{
public:
    using Ptr = shared_ptr<RtcCodecPlan>;
    uint8_t pt;
    string codec;
    uint32_t sample_rate;
    //音频时有效
    uint32_t channel = 0;
    //rtcp反馈
    set<string> rtcp_fb;
    map<string/*key*/, string/*value*/, StrCaseCompare> fmtp;

    string getFmtp(const char *key) const;
};

//rtc 媒体描述
class RtcMedia{
public:
    TrackType type{TrackType::TrackInvalid};
    string mid;
    uint16_t port{0};
    SdpConnection addr;
    string proto;
    RtpDirection direction{RtpDirection::invalid};
    vector<RtcCodecPlan> plan;

    //////// rtp ////////
    vector<RtcSSRC> rtp_rtx_ssrc;

    //////// simulcast ////////
    vector<RtcSSRC> rtp_ssrc_sim;
    vector<string> rtp_rids;

    ////////  rtcp  ////////
    bool rtcp_mux{false};
    bool rtcp_rsize{false};
    SdpAttrRtcp rtcp_addr;

    //////// ice ////////
    bool ice_trickle{false};
    bool ice_lite{false};
    bool ice_renomination{false};
    string ice_ufrag;
    string ice_pwd;
    std::vector<SdpAttrCandidate> candidate;

    //////// dtls ////////
    DtlsRole role{DtlsRole::invalid};
    SdpAttrFingerprint fingerprint;

    //////// extmap ////////
    vector<SdpAttrExtmap> extmap;

    //////// sctp ////////////
    SdpAttrSctpMap sctpmap;
    uint32_t sctp_port{0};

    void checkValid() const;
    //offer sdp,如果指定了发送rtp,那么应该指定ssrc
    void checkValidSSRC() const;
    const RtcCodecPlan *getPlan(uint8_t pt) const;
    const RtcCodecPlan *getPlan(const char *codec) const;
    const RtcCodecPlan *getRelatedRtxPlan(uint8_t pt) const;
    uint32_t getRtpSSRC() const;
    uint32_t getRtxSSRC() const;
};

class RtcSession{
public:
    using Ptr = std::shared_ptr<RtcSession>;

    uint32_t version;
    SdpOrigin origin;
    string session_name;
    string session_info;
    SdpTime time;
    SdpConnection connection;
    SdpBandwidth bandwidth;
    SdpAttrMsidSemantic msid_semantic;
    vector<RtcMedia> media;
    SdpAttrGroup group;

    void loadFrom(const string &sdp, bool check = true);
    void checkValid() const;
    //offer sdp,如果指定了发送rtp,那么应该指定ssrc
    void checkValidSSRC() const;
    string toString() const;
    string toRtspSdp() const;
    const  RtcMedia *getMedia(TrackType type) const;
    bool haveSSRC() const;
    bool supportRtcpFb(const string &name, TrackType type = TrackType::TrackVideo) const;

private:
    RtcSessionSdp::Ptr toRtcSessionSdp() const;
};

class RtcConfigure {
public:
    using Ptr = std::shared_ptr<RtcConfigure>;
    class RtcTrackConfigure {
    public:
        bool enable;
        bool rtcp_mux;
        bool rtcp_rsize;
        bool group_bundle;
        bool support_rtx;
        bool support_red;
        bool support_ulpfec;
        bool ice_lite;
        bool ice_trickle;
        bool ice_renomination;
        string ice_ufrag;
        string ice_pwd;

        RtpDirection direction{RtpDirection::invalid};
        SdpAttrFingerprint fingerprint;

        set<string> rtcp_fb;
        set<RtpExtType> extmap;
        vector<CodecId> preferred_codec;
        vector<SdpAttrCandidate> candidate;

        void setDefaultSetting(TrackType type);
        void enableTWCC(bool enable = true);
        void enableREMB(bool enable = true);
    };

    RtcTrackConfigure video;
    RtcTrackConfigure audio;
    RtcTrackConfigure application;

    void setDefaultSetting(string ice_ufrag,
                           string ice_pwd,
                           RtpDirection direction,
                           const SdpAttrFingerprint &fingerprint);
    void addCandidate(const SdpAttrCandidate &candidate, TrackType type = TrackInvalid);

    shared_ptr<RtcSession> createAnswer(const RtcSession &offer);

    void setPlayRtspInfo(const string &sdp);

    void enableTWCC(bool enable = true, TrackType type = TrackInvalid);
    void enableREMB(bool enable = true, TrackType type = TrackInvalid);

private:
    void matchMedia(shared_ptr<RtcSession> &ret, TrackType type, const vector<RtcMedia> &medias, const RtcTrackConfigure &configure);
    bool onCheckCodecProfile(const RtcCodecPlan &plan, CodecId codec);
    void onSelectPlan(RtcCodecPlan &plan, CodecId codec);

private:
    RtcCodecPlan::Ptr _rtsp_video_plan;
    RtcCodecPlan::Ptr _rtsp_audio_plan;
};

class SdpConst {
public:
    static string const kTWCCRtcpFb;
    static string const kRembRtcpFb;

private:
    SdpConst() = delete;
    ~SdpConst() = delete;
};


#endif //ZLMEDIAKIT_SDP_H
