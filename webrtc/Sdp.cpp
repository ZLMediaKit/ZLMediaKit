//
// Created by xzl on 2021/3/27.
//

#include "Sdp.h"
#include <inttypes.h>

using onCreateSdpItem = function<SdpItem::Ptr(const string &key, const string &value)>;
static unordered_map<string, onCreateSdpItem> sdpItemCreator;

template <typename Item>
void registerSdpItem(){
    onCreateSdpItem func = [](const string &key, const string &value) {
        auto ret = std::make_shared<Item>();
        ret->parse(value);
        return ret;
    };
    Item item;
    sdpItemCreator.emplace(item.getKey(), std::move(func));
}

class DirectionInterface {
public:
    virtual RtpDirection getDirection() const = 0;
};

class SdpDirectionSendonly : public SdpItem, public DirectionInterface{
public:
    const char* getKey() const override { return getRtpDirectionString(getDirection());}
    RtpDirection getDirection() const override {return RtpDirection::sendonly;}
};

class SdpDirectionRecvonly : public SdpItem, public DirectionInterface{
public:
    const char* getKey() const override { return getRtpDirectionString(getDirection());}
    RtpDirection getDirection() const override {return RtpDirection::recvonly;}
};

class SdpDirectionSendrecv : public SdpItem, public DirectionInterface{
public:
    const char* getKey() const override { return getRtpDirectionString(getDirection());}
    RtpDirection getDirection() const override {return RtpDirection::sendrecv;}
};

class SdpDirectionInactive : public SdpItem, public DirectionInterface{
public:
    const char* getKey() const override { return getRtpDirectionString(getDirection());}
    RtpDirection getDirection() const override {return RtpDirection::inactive;}
};

static bool registerAllItem(){
    registerSdpItem<SdpString<'v'> >();
    registerSdpItem<SdpString<'s'> >();
    registerSdpItem<SdpString<'i'> >();
    registerSdpItem<SdpString<'u'> >();
    registerSdpItem<SdpString<'e'> >();
    registerSdpItem<SdpString<'p'> >();
    registerSdpItem<SdpString<'z'> >();
    registerSdpItem<SdpString<'k'> >();
    registerSdpItem<SdpString<'r'> >();
    registerSdpItem<SdpTime>();
    registerSdpItem<SdpOrigin>();
    registerSdpItem<SdpConnection>();
    registerSdpItem<SdpBandwidth>();
    registerSdpItem<SdpMedia>();
    registerSdpItem<SdpAttr>();
    registerSdpItem<SdpAttrGroup>();
    registerSdpItem<SdpAttrMsidSemantic>();
    registerSdpItem<SdpAttrRtcp>();
    registerSdpItem<SdpAttrIceUfrag>();
    registerSdpItem<SdpAttrIcePwd>();
    registerSdpItem<SdpAttrIceOption>();
    registerSdpItem<SdpAttrFingerprint>();
    registerSdpItem<SdpAttrSetup>();
    registerSdpItem<SdpAttrMid>();
    registerSdpItem<SdpAttrExtmap>();
    registerSdpItem<SdpAttrRtpMap>();
    registerSdpItem<SdpAttrRtcpFb>();
    registerSdpItem<SdpAttrFmtp>();
    registerSdpItem<SdpAttrSSRC>();
    registerSdpItem<SdpAttrSSRCGroup>();
    registerSdpItem<SdpAttrSctpMap>();
    registerSdpItem<SdpAttrCandidate>();
    registerSdpItem<SdpDirectionSendonly>();
    registerSdpItem<SdpDirectionRecvonly>();
    registerSdpItem<SdpDirectionSendrecv>();
    registerSdpItem<SdpDirectionInactive>();

    return true;
}

TrackType getTrackType(const string &str){
    if (str == "video") {
        return TrackVideo;
    }
    if (str == "audio") {
        return TrackAudio;
    }
    if (str == "application") {
        return TrackApplication;
    }
    return TrackInvalid;
}

const char* getTrackString(TrackType type){
    switch (type) {
        case TrackVideo : return "video";
        case TrackAudio : return "audio";
        case TrackApplication : return "application";
        default: return "invalid";
    }
}

DtlsRole getDtlsRole(const string &str){
    if (str == "active") {
        return DtlsRole::active;
    }
    if (str == "passive") {
        return DtlsRole::passive;
    }
    if (str == "actpass") {
        return DtlsRole::actpass;
    }
    return DtlsRole::invalid;
}

const char* getDtlsRoleString(DtlsRole role){
    switch (role) {
        case DtlsRole::active : return "active";
        case DtlsRole::passive : return "passive";
        case DtlsRole::actpass : return "actpass";
        default: return "invalid";
    }
}

RtpDirection getRtpDirection(const string &str){
    if (str == "sendonly") {
        return RtpDirection::sendonly;
    }
    if (str == "recvonly") {
        return RtpDirection::recvonly;
    }
    if (str == "sendrecv") {
        return RtpDirection::sendrecv;
    }
    if (str == "inactive") {
        return RtpDirection::inactive;
    }
    return RtpDirection::invalid;
}

const char* getRtpDirectionString(RtpDirection val){
    switch (val) {
        case RtpDirection::sendonly : return "sendonly";
        case RtpDirection::recvonly : return "recvonly";
        case RtpDirection::sendrecv : return "sendrecv";
        case RtpDirection::inactive : return "inactive";
        default: return "invalid";
    }
}

//////////////////////////////////////////////////////////////////////////////////////////

void RtcSessionSdp::parse(const string &str) {
    static auto flag = registerAllItem();
    RtcMediaSdp *media = nullptr;
    auto lines = split(str, "\n");
    for(auto &line : lines){
        trim(line);
        if(line.size() < 3 || line[1] != '='){
            continue;
        }
        auto key = line.substr(0, 1);
        auto value = line.substr(2);
        if (key == "m") {
            medias.emplace_back(RtcMediaSdp());
            media = &medias.back();
        }

        SdpItem::Ptr item;
        auto it = sdpItemCreator.find(key);
        if (it != sdpItemCreator.end()) {
            item = it->second(key, value);
        } else {
            item = std::make_shared<SdpCommon>(key);
            item->parse(value);
        }
        if (media) {
            media->items.push_back(std::move(item));
        } else {
            items.push_back(std::move(item));
        }
    }
}

string RtcSessionSdp::toString() const {
    _StrPrinter printer;
    for (auto &item : items) {
        printer << item->getKey() << "=" << item->toString() << "\r\n";
    }
    for (auto &media : medias) {
        printer << media.toString();
    }

    return std::move(printer);
}

//////////////////////////////////////////////////////////////////////

string RtcMediaSdp::toString() const {
    _StrPrinter printer;
    for (auto &item : items) {
        printer << item->getKey() << "=" << item->toString() << "\r\n";
    }
    return std::move(printer);
}

RtpDirection RtcMediaSdp::getDirection() const{
    for (auto &item : items) {
        auto attr = dynamic_pointer_cast<SdpAttr>(item);
        if (attr) {
            auto dir = dynamic_pointer_cast<DirectionInterface>(attr->detail);
            if (dir) {
                return dir->getDirection();
            }
        }
    }
    return RtpDirection::invalid;
}


//////////////////////////////////////////////////////////////////////////////////////////

#define SDP_THROW() throw std::invalid_argument(StrPrinter << "解析sdp " << getKey() << " 字段失败:" << str)
#define SDP_THROW2() throw std::invalid_argument(StrPrinter << "生成sdp " << getKey() << " 字段失败")

void SdpTime::parse(const string &str) {
    if (sscanf(str.data(), "%" SCNu64 " %" SCNu64, &start, &stop) != 2) {
        SDP_THROW();
    }
}

string SdpTime::toString() const {
    if (value.empty()) {
        value = to_string(start) + " " + to_string(stop);
    }
    return SdpItem::toString();
}

void SdpOrigin::parse(const string &str) {
    auto vec = split(str, " ");
    if (vec.size() != 6) {
        SDP_THROW();
    }
    username = vec[0];
    session_id = vec[1];
    session_version = vec[2];
    nettype = vec[3];
    addrtype = vec[4];
    address = vec[5];
}

string SdpOrigin::toString() const {
    if (value.empty()) {
        value = username + " " + session_id + " " + session_version + " " + nettype + " " + addrtype + " " + address;
    }
    return SdpItem::toString();
}

void SdpConnection::parse(const string &str) {
    auto vec = split(str, " ");
    if (vec.size() != 3) {
        SDP_THROW();
    }
    nettype = vec[0];
    addrtype = vec[1];
    address = vec[2];
}

string SdpConnection::toString() const {
    if (value.empty()) {
        value = nettype + " " + addrtype + " " + address;
    }
    return SdpItem::toString();
}

void SdpBandwidth::parse(const string &str) {
    auto vec = split(str, ":");
    if (vec.size() != 2) {
        SDP_THROW();
    }
    bwtype = vec[0];
    bandwidth = atoi(vec[1].data());
}

string SdpBandwidth::toString() const {
    if (value.empty()) {
        value = bwtype + ":" + to_string(bandwidth);
    }
    return SdpItem::toString();
}

void SdpMedia::parse(const string &str) {
    auto vec = split(str, " ");
    if (vec.size() < 4) {
        SDP_THROW();
    }
    type = getTrackType(vec[0]);
    if (type == TrackInvalid) {
        SDP_THROW();
    }
    port = atoi(vec[1].data());
    proto = vec[2];
    for (size_t i = 3; i < vec.size(); ++i) {
        auto pt = atoi(vec[i].data());
        if (type != TrackApplication && pt > 0xFF) {
            SDP_THROW();
        }
        fmts.emplace_back(pt);
    }
}

string SdpMedia::toString() const {
    if (value.empty()) {
        value = string(getTrackString(type)) + " " + to_string(port) + " " + proto;
        for (auto fmt : fmts) {
            value += ' ';
            value += to_string(fmt);
        }
    }
    return SdpItem::toString();
}

void SdpAttr::parse(const string &str) {
    auto pos = str.find(':');
    auto key = pos == string::npos ? str : str.substr(0, pos);
    auto value = pos == string::npos ? string() : str.substr(pos + 1);
    auto it = sdpItemCreator.find(key);
    if (it != sdpItemCreator.end()) {
        detail = it->second(key, value);
    } else {
        detail = std::make_shared<SdpCommon>(key);
        detail->parse(value);
    }
}

string SdpAttr::toString() const {
    if (value.empty()) {
        auto detail_value = detail->toString();
        if (detail_value.empty()) {
            value = detail->getKey();
        } else {
            value = string(detail->getKey()) + ":" + detail_value;
        }
    }
    return SdpItem::toString();
}

void SdpAttrGroup::parse(const string &str)  {
    auto vec = split(str, " ");
    if (vec.size() < 2) {
        SDP_THROW();
    }
    type = vec[0];
    vec.erase(vec.begin());
    mids = std::move(vec);
}

string SdpAttrGroup::toString() const  {
    if (value.empty()) {
        value = type;
        for (auto mid : mids) {
            value += ' ';
            value += mid;
        }
    }
    return SdpItem::toString();
}

void SdpAttrMsidSemantic::parse(const string &str)  {
    auto vec = split(str, " ");
    if (vec.size() < 1) {
        SDP_THROW();
    }
    msid = vec[0];
    token = vec.size() > 1 ? vec[1] : "";
}

string SdpAttrMsidSemantic::toString() const  {
    if (value.empty()) {
        if (token.empty()) {
            value = string(" ") + msid;
        } else {
            value = string(" ") + msid + " " + token;
        }
    }
    return SdpItem::toString();
}

void SdpAttrRtcp::parse(const string &str)  {
    auto vec = split(str, " ");
    if (vec.size() != 4) {
        SDP_THROW();
    }
    port = atoi(vec[0].data());
    nettype = vec[1];
    addrtype = vec[2];
    address = vec[3];
}

string SdpAttrRtcp::toString() const  {
    if (value.empty()) {
        value = to_string(port) + " " + nettype + " " + addrtype + " " + address;
    }
    return SdpItem::toString();
}

void SdpAttrFingerprint::parse(const string &str)  {
    auto vec = split(str, " ");
    if (vec.size() != 2) {
        SDP_THROW();
    }
    algorithm = vec[0];
    hash = vec[1];
}

string SdpAttrFingerprint::toString() const  {
    if (value.empty()) {
        value = algorithm + " " + hash;
    }
    return SdpItem::toString();
}

void SdpAttrSetup::parse(const string &str)  {
    role = getDtlsRole(str);
    if (role == DtlsRole::invalid) {
        SDP_THROW();
    }
}

string SdpAttrSetup::toString() const  {
    if (value.empty()) {
        value = getDtlsRoleString(role);
    }
    return SdpItem::toString();
}

void SdpAttrExtmap::parse(const string &str)  {
    char buf[128] = {0};
    char direction_buf[32] = {0};
    if (sscanf(str.data(), "%" SCNd32 "/%31[^ ] %127s", &index, direction_buf, buf) != 3) {
        if (sscanf(str.data(), "%" SCNd32 " %127s", &index, buf) != 2) {
            SDP_THROW();
        }
    } else {
        direction = getRtpDirection(direction_buf);
    }
    ext = buf;
}

string SdpAttrExtmap::toString() const  {
    if (value.empty()) {
        if(direction == RtpDirection::invalid){
            value = to_string(index) + " " + ext;
        } else {
            value = to_string(index) + "/" + getRtpDirectionString(direction) +  " " + ext;
        }
    }
    return SdpItem::toString();
}

void SdpAttrRtpMap::parse(const string &str)  {
    char buf[32] = {0};
    if (sscanf(str.data(), "%" SCNu8 " %31[^/]/%" SCNd32 "/%" SCNd32, &pt, buf, &sample_rate, &channel) != 4) {
        if (sscanf(str.data(), "%" SCNu8 " %31[^/]/%" SCNd32, &pt, buf, &sample_rate) != 3) {
            SDP_THROW();
        }
    }
    codec = buf;
}

string SdpAttrRtpMap::toString() const  {
    if (value.empty()) {
        value = to_string(pt) + " " + codec + "/" + to_string(sample_rate);
        if (channel) {
            value += '/';
            value += to_string(channel);
        }
    }
    return SdpItem::toString();
}

void SdpAttrRtcpFb::parse(const string &str)  {
    auto vec = split(str, " ");
    if (vec.size() < 2) {
        SDP_THROW();
    }
    pt = atoi(vec[0].data());
    vec.erase(vec.begin());
    arr = std::move(vec);
}

string SdpAttrRtcpFb::toString() const  {
    if (value.empty()) {
        value = to_string(pt);
        for (auto &item : arr) {
            value += ' ';
            value += item;
        }
    }
    return SdpItem::toString();
}

void SdpAttrFmtp::parse(const string &str)  {
    auto pos = str.find(' ');
    if (pos == string::npos) {
        SDP_THROW();
    }
    pt = atoi(str.substr(0, pos).data());
    auto vec = split(str.substr(pos + 1), ";");
    for (auto &item : vec) {
        trim(item);
        auto pr_vec = split(item, "=");
        if (pr_vec.size() != 2) {
            SDP_THROW();
        }
        arr.emplace_back(std::make_pair(pr_vec[0], pr_vec[1]));
    }
    if (arr.empty()) {
        SDP_THROW();
    }
}

string SdpAttrFmtp::toString() const  {
    if (value.empty()) {
        value = to_string(pt);
        int i = 0;
        for (auto &pr : arr) {
            value += (i++  ? ';' : ' ');
            value += pr.first + "=" + pr.second;
        }
    }
    return SdpItem::toString();
}

void SdpAttrSSRC::parse(const string &str_in)  {
    auto str = str_in + '\n';
    char attr_buf[32] = {0};
    char attr_val_buf[128] = {0};
    if (3 == sscanf(str.data(), "%" SCNu32 " %31[^:]:%127[^\n]", &ssrc, attr_buf, attr_val_buf)) {
        attribute = attr_buf;
        attribute_value = attr_val_buf;
    } else if (2 == sscanf(str.data(), "%" SCNu32 " %31s[^\n]", &ssrc, attr_buf)) {
        attribute = attr_buf;
    } else {
        SDP_THROW();
    }
}

string SdpAttrSSRC::toString() const  {
    if (value.empty()) {
        value = to_string(ssrc) + ' ';
        value += attribute;
        if (!attribute_value.empty()) {
            value += ':';
            value += attribute_value;
        }
    }
    return SdpItem::toString();
}

void SdpAttrSSRCGroup::parse(const string &str) {
    auto vec = split(str, " ");
    if (vec.size() == 3) {
        if (vec[0] != "FID") {
            SDP_THROW();
        }
        type = std::move(vec[0]);
        u.fid.rtp_ssrc = atoi(vec[1].data());
        u.fid.rtx_ssrc = atoi(vec[2].data());
    } else if (vec.size() == 4) {
        if (vec[0] != "SIM") {
            SDP_THROW();
        }
        type = std::move(vec[0]);
        u.sim.rtp_ssrc_low = atoi(vec[1].data());
        u.sim.rtp_ssrc_mid = atoi(vec[2].data());
        u.sim.rtp_ssrc_high = atoi(vec[3].data());
    } else {
        SDP_THROW();
    }
}

string SdpAttrSSRCGroup::toString() const  {
    if (value.empty()) {
        if (type == "FID") {
            value = type + " " + to_string(u.fid.rtp_ssrc) + " " + to_string(u.fid.rtx_ssrc);
        } else if (type == "SIM") {
            value = type + " " + to_string(u.sim.rtp_ssrc_low) + " " + to_string(u.sim.rtp_ssrc_mid) + " " + to_string(u.sim.rtp_ssrc_high);
        } else {
            SDP_THROW2();
        }
    }
    return SdpItem::toString();
}

void SdpAttrSctpMap::parse(const string &str)  {
    char subtypes_buf[64] = {0};
    if (3 == sscanf(str.data(), "%" SCNu16 " %63[^ ] %" SCNd32, &port, subtypes_buf, &streams)) {
        subtypes = subtypes_buf;
    } else {
        SDP_THROW();
    }
}

string SdpAttrSctpMap::toString() const  {
    if (value.empty()) {
        value = to_string(port);
        value += ' ';
        value += subtypes;
        value += ' ';
        value += to_string(streams);
    }
    return SdpItem::toString();
}

void SdpAttrCandidate::parse(const string &str)  {
    char transport_buf[16] = {0};
    char address_buf[32] = {0};
    char type_buf[16] = {0};

    if (7 != sscanf(str.data(), "%" SCNu32 " %" SCNu32 " %15[^ ] %" SCNu32 " %31[^ ] %" SCNu16 " typ %15[^ ]",
                    &foundation, &component, transport_buf, &priority, address_buf, &port, type_buf)) {
        SDP_THROW();
    }
    transport = transport_buf;
    address = address_buf;
    type = type_buf;
    auto pos = str.find(type);
    if (pos != string::npos) {
        auto remain = str.substr(pos + type.size());
        trim(remain);
        if (!remain.empty()) {
            auto vec = split(remain, " ");
            string key;
            for (auto &item : vec) {
                if (key.empty()) {
                    key = item;
                } else {
                    arr.emplace_back(std::make_pair(std::move(key), std::move(item)));
                }
            }
        }
    }
}

string SdpAttrCandidate::toString() const  {
    if (value.empty()) {
        value = to_string(foundation) + " " + to_string(component) + " " + transport + " " + to_string(priority) +
                " " + address + " " + to_string(port) + " typ " + type;
        for (auto &pr : arr) {
            value += ' ';
            value += pr.first;
            value += ' ';
            value += pr.second;
        }
    }
    return SdpItem::toString();
}

void test_sdp(){
    char str1[] = "v=0\n"
                 "o=- 380154348540553537 2 IN IP4 127.0.0.1\n"
                 "s=-\n"
                 "b=CT:1900\n"
                 "t=0 0\n"
                 "a=group:BUNDLE video\n"
                 "a=msid-semantic: WMS\n"
                 "m=video 9 RTP/SAVPF 96\n"
                 "c=IN IP4 0.0.0.0\n"
                 "a=rtcp:9 IN IP4 0.0.0.0\n"
                 "a=ice-ufrag:1ZFN\n"
                 "a=ice-pwd:70P3H0jPlGz1fiJl5XZfXMZH\n"
                 "a=ice-options:trickle\n"
                 "a=fingerprint:sha-256 3E:10:35:6B:9A:9E:B0:55:AC:2A:88:F5:74:C1:70:32:B5:8D:88:1D:37:B0:9C:69:A6:DD:07:10:73:27:1A:16\n"
                 "a=setup:active\n"
                 "a=mid:video\n"
                 "a=recvonly\n"
                 "a=rtcp-mux\n"
                 "a=rtpmap:96 H264/90000\n"
                 "a=fmtp:96 level-asymmetry-allowed=1;packetization-mode=0;profile-level-id=42e01f";
    char str2[] = "v=0\n"
                  "o=- 2584450093346841581 2 IN IP4 127.0.0.1\n"
                  "s=-\n"
                  "t=0 0\n"
                  "a=group:BUNDLE audio video data\n"
                  "a=msid-semantic: WMS 616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "m=audio 9 UDP/TLS/RTP/SAVPF 111 103 104 9 0 8 106 105 13 110 112 113 126\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=rtcp:9 IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:sXJ3\n"
                  "a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV\n"
                  "a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79\n"
                  "a=setup:actpass\n"
                  "a=mid:audio\n"
                  "a=extmap:1/sendonly urn:ietf:params:rtp-hdrext:ssrc-audio-level\n"
                  "a=sendrecv\n"
                  "a=rtcp-mux\n"
                  "a=rtpmap:111 opus/48000/2\n"
                  "a=rtcp-fb:111 transport-cc\n"
                  "a=fmtp:111 minptime=10;useinbandfec=1\n"
                  "a=rtpmap:103 ISAC/16000\n"
                  "a=rtpmap:104 ISAC/32000\n"
                  "a=rtpmap:9 G722/8000\n"
                  "a=rtpmap:0 PCMU/8000\n"
                  "a=rtpmap:8 PCMA/8000\n"
                  "a=rtpmap:106 CN/32000\n"
                  "a=rtpmap:105 CN/16000\n"
                  "a=rtpmap:13 CN/8000\n"
                  "a=rtpmap:110 telephone-event/48000\n"
                  "a=rtpmap:112 telephone-event/32000\n"
                  "a=rtpmap:113 telephone-event/16000\n"
                  "a=rtpmap:126 telephone-event/8000\n"
                  "a=ssrc:120276603 cname:iSkJ2vn5cYYubTve\n"
                  "a=ssrc:120276603 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 1da3d329-7399-4fe9-b20f-69606bebd363\n"
                  "a=ssrc:120276603 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "a=ssrc:120276603 label:1da3d329-7399-4fe9-b20f-69606bebd363\n"
                  "m=video 9 UDP/TLS/RTP/SAVPF 96 98 100 102 127 97 99 101 125\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=rtcp:9 IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:sXJ3\n"
                  "a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV\n"
                  "a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79\n"
                  "a=setup:actpass\n"
                  "a=mid:video\n"
                  "a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\n"
                  "a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\n"
                  "a=extmap:4 urn:3gpp:video-orientation\n"
                  "a=extmap:5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01\n"
                  "a=extmap:6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay\n"
                  "a=sendrecv\n"
                  "a=rtcp-mux\n"
                  "a=rtcp-rsize\n"
                  "a=rtpmap:96 VP8/90000\n"
                  "a=rtcp-fb:96 ccm fir\n"
                  "a=rtcp-fb:96 nack\n"
                  "a=rtcp-fb:96 nack pli\n"
                  "a=rtcp-fb:96 goog-remb\n"
                  "a=rtcp-fb:96 transport-cc\n"
                  "a=rtpmap:98 VP9/90000\n"
                  "a=rtcp-fb:98 ccm fir\n"
                  "a=rtcp-fb:98 nack\n"
                  "a=rtcp-fb:98 nack pli\n"
                  "a=rtcp-fb:98 goog-remb\n"
                  "a=rtcp-fb:98 transport-cc\n"
                  "a=rtpmap:100 H264/90000\n"
                  "a=rtcp-fb:100 ccm fir\n"
                  "a=rtcp-fb:100 nack\n"
                  "a=rtcp-fb:100 nack pli\n"
                  "a=rtcp-fb:100 goog-remb\n"
                  "a=rtcp-fb:100 transport-cc\n"
                  "a=fmtp:100 level-asymmetry-allowed=1;packetization-mode=1;profile-level-id=42e01f\n"
                  "a=rtpmap:102 red/90000\n"
                  "a=rtpmap:127 ulpfec/90000\n"
                  "a=rtpmap:97 rtx/90000\n"
                  "a=fmtp:97 apt=96\n"
                  "a=rtpmap:99 rtx/90000\n"
                  "a=fmtp:99 apt=98\n"
                  "a=rtpmap:101 rtx/90000\n"
                  "a=fmtp:101 apt=100\n"
                  "a=rtpmap:125 rtx/90000\n"
                  "a=fmtp:125 apt=102\n"
                  "a=ssrc-group:FID 2580761338 611523443\n"
                  "a=ssrc:2580761338 cname:iSkJ2vn5cYYubTve\n"
                  "a=ssrc:2580761338 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=ssrc:2580761338 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "a=ssrc:2580761338 label:bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=ssrc:611523443 cname:iSkJ2vn5cYYubTve\n"
                  "a=ssrc:611523443 msid:616cfbb1-33a3-4d8c-8275-a199d6005549 bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=ssrc:611523443 mslabel:616cfbb1-33a3-4d8c-8275-a199d6005549\n"
                  "a=ssrc:611523443 label:bf270496-a23e-47b5-b901-ef23096cd961\n"
                  "a=candidate:3575467457 1 udp 2113937151 10.15.83.23 57857 typ host generation 0 ufrag 6R0z network-cost 999\n"
                  "m=application 9 DTLS/SCTP 5000\n"
                  "c=IN IP4 0.0.0.0\n"
                  "a=ice-ufrag:sXJ3\n"
                  "a=ice-pwd:yEclOTrLg1gEubBFefOqtmyV\n"
                  "a=fingerprint:sha-256 22:14:B5:AF:66:12:C7:C7:8D:EF:4B:DE:40:25:ED:5D:8F:17:54:DD:88:33:C0:13:2E:FD:1A:FA:7E:7A:1B:79\n"
                  "a=setup:actpass\n"
                  "a=mid:data\n"
                  "a=sctpmap:5000 webrtc-datachannel 1024\n"
                  "a=sctp-port:5000";

    RtcSessionSdp sdp1;
    sdp1.parse(str1);

    RtcSessionSdp sdp2;
    sdp2.parse(str2);

    for (auto media : sdp1.medias) {
        InfoL << getRtpDirectionString(media.getDirection());
    }
    for (auto media : sdp2.medias) {
        InfoL << getRtpDirectionString(media.getDirection());
    }
    InfoL << sdp1.toString();
    InfoL << sdp2.toString();
}
