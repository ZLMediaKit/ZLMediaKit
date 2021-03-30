//
// Created by xzl on 2021/3/27.
//

#include "Sdp.h"
#include "assert.h"
#include "Common/Parser.h"
#include <inttypes.h>

#define CHECK(exp) Assert_Throw(!(exp), #exp, __FUNCTION__, __FILE__, __LINE__);

using onCreateSdpItem = function<SdpItem::Ptr(const string &key, const string &value)>;
static map<string, onCreateSdpItem, StrCaseCompare> sdpItemCreator;

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

class DirectionInterfaceImp : public SdpItem, public DirectionInterface{
public:
    DirectionInterfaceImp(RtpDirection direct){
        direction = direct;
    }
    const char* getKey() const override { return getRtpDirectionString(getDirection());}
    RtpDirection getDirection() const override {return direction;}

private:
    RtpDirection direction;
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

static map<string, DtlsRole, StrCaseCompare> dtls_role_map = {
        {"active",  DtlsRole::active},
        {"passive", DtlsRole::passive},
        {"actpass", DtlsRole::actpass}
};

DtlsRole getDtlsRole(const string &str) {
    auto it = dtls_role_map.find(str);
    return it == dtls_role_map.end() ? DtlsRole::invalid : it->second;
}

const char* getDtlsRoleString(DtlsRole role){
    switch (role) {
        case DtlsRole::active : return "active";
        case DtlsRole::passive : return "passive";
        case DtlsRole::actpass : return "actpass";
        default: return "invalid";
    }
}

static map<string, RtpDirection, StrCaseCompare> direction_map = {
        {"sendonly", RtpDirection::sendonly},
        {"recvonly", RtpDirection::recvonly},
        {"sendrecv", RtpDirection::sendrecv},
        {"inactive", RtpDirection::inactive}
};

RtpDirection getRtpDirection(const string &str) {
    auto it = direction_map.find(str);
    return it == direction_map.end() ? RtpDirection::invalid : it->second;
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

string RtcSdpBase::toString() const {
    _StrPrinter printer;
    for (auto &item : items) {
        printer << item->getKey() << "=" << item->toString() << "\r\n";
    }
    return std::move(printer);
}

RtpDirection RtcSdpBase::getDirection() const{
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

SdpItem::Ptr RtcSdpBase::getItem(char key_c, const char *attr_key) const {
    for (auto item : items) {
        string key(1, key_c);
        if (strcasecmp(item->getKey(), key.data()) == 0) {
            if (!attr_key) {
                return item;
            }
            auto attr = dynamic_pointer_cast<SdpAttr>(item);
            if (attr && !strcasecmp(attr->detail->getKey() , attr_key)) {
                return attr->detail;
            }
        }
    }
    return SdpItem::Ptr();
}

int RtcSdpBase::getVersion() const {
    return atoi(getStringItem('v').data());
}

SdpOrigin RtcSdpBase::getOrigin() const {
    return getItemClass<SdpOrigin>('o');
}

string RtcSdpBase::getSessionName() const {
    return getStringItem('s');
}

string RtcSdpBase::getSessionInfo() const {
    return getStringItem('i');
}

SdpTime RtcSdpBase::getSessionTime() const{
    return getItemClass<SdpTime>('t');
}

SdpConnection RtcSdpBase::getConnection() const {
    return getItemClass<SdpConnection>('c');
}

SdpBandwidth RtcSdpBase::getBandwidth() const {
    return getItemClass<SdpBandwidth>('b');
}

string RtcSdpBase::getUri() const {
    return getStringItem('u');
}

string RtcSdpBase::getEmail() const {
    return getStringItem('e');
}

string RtcSdpBase::getPhone() const {
    return getStringItem('p');
}

string RtcSdpBase::getTimeZone() const {
    return getStringItem('z');
}

string RtcSdpBase::getEncryptKey() const {
    return getStringItem('k');
}

string RtcSdpBase::getRepeatTimes() const {
    return getStringItem('r');
}

//////////////////////////////////////////////////////////////////////

void RtcSessionSdp::parse(const string &str) {
    static auto flag = registerAllItem();
    RtcSdpBase *media = nullptr;
    auto lines = split(str, "\n");
    for(auto &line : lines){
        trim(line);
        if(line.size() < 3 || line[1] != '='){
            continue;
        }
        auto key = line.substr(0, 1);
        auto value = line.substr(2);
        if (!strcasecmp(key.data(), "m")) {
            medias.emplace_back(RtcSdpBase());
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
    printer << RtcSdpBase::toString();
    for (auto &media : medias) {
        printer << media.toString();
    }

    return std::move(printer);
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
        direction = RtpDirection::sendrecv;
    } else {
        direction = getRtpDirection(direction_buf);
    }
    ext = buf;
}

string SdpAttrExtmap::toString() const  {
    if (value.empty()) {
        if(direction == RtpDirection::invalid || direction == RtpDirection::sendrecv){
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

void SdpAttrRtcpFb::parse(const string &str_in)  {
    auto str = str_in + "\n";
    char rtcp_type_buf[32] = {0};
    if (2 != sscanf(str.data(), "%" SCNu8 " %31[^\n]", &pt, rtcp_type_buf)) {
        SDP_THROW();
    }
    rtcp_type = rtcp_type_buf;
}

string SdpAttrRtcpFb::toString() const  {
    if (value.empty()) {
        value = to_string(pt) + " " + rtcp_type;
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
        if (isFID()) {
            value = type + " " + to_string(u.fid.rtp_ssrc) + " " + to_string(u.fid.rtx_ssrc);
        } else if (isSIM()) {
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
    char foundation_buf[32] = {0};
    char transport_buf[16] = {0};
    char address_buf[32] = {0};
    char type_buf[16] = {0};

    if (7 != sscanf(str.data(), "%31[^ ] %" SCNu32 " %15[^ ] %" SCNu32 " %31[^ ] %" SCNu16 " typ %15[^ ]",
                    foundation_buf, &component, transport_buf, &priority, address_buf, &port, type_buf)) {
        SDP_THROW();
    }
    foundation = foundation_buf;
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
        value = foundation + " " + to_string(component) + " " + transport + " " + to_string(priority) +
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

    RtcSession session1;
    session1.loadFrom(str1);

    RtcSession session2;
    session2.loadFrom(str2);
    DebugL << session1.toString();
    DebugL << session2.toString();
}

void RtcSession::loadFrom(const string &str) {
    RtcSessionSdp sdp;
    sdp.parse(str);

    version = sdp.getVersion();
    origin = sdp.getOrigin();
    session_name = sdp.getSessionName();
    session_info = sdp.getSessionInfo();
    connection = sdp.getConnection();
    bandwidth = sdp.getBandwidth();
    time = sdp.getSessionTime();
    msid_semantic = sdp.getItemClass<SdpAttrMsidSemantic>('a', "msid-semantic");
    for (auto &media : sdp.medias) {
        auto mline = media.getItemClass<SdpMedia>('m');
        switch (mline.type) {
            case TrackVideo:
            case TrackAudio:
            case TrackApplication: break;
            default: throw std::invalid_argument(StrPrinter << "不识别的media类型:" << mline.toString());
        }
        this->media.emplace_back();
        auto &rtc_media = this->media.back();
        rtc_media.type = mline.type;
        rtc_media.mid = media.getStringItem('a', "mid");
        rtc_media.proto = mline.proto;
        rtc_media.type = mline.type;
        rtc_media.port = mline.port;
        rtc_media.addr = media.getItemClass<SdpConnection>('c');
        rtc_media.ice_ufrag = media.getStringItem('a', "ice-ufrag");
        rtc_media.ice_pwd = media.getStringItem('a', "ice-pwd");
        rtc_media.role = media.getItemClass<SdpAttrSetup>('a', "setup").role;
        rtc_media.fingerprint = media.getItemClass<SdpAttrFingerprint>('a', "fingerprint");
        rtc_media.ice_trickle = media.getItem('a', "ice-trickle").operator bool();
        rtc_media.ice_lite = media.getItem('a', "ice-lite").operator bool();
        rtc_media.ice_renomination = media.getItem('a', "ice-renomination").operator bool();
        rtc_media.candidate = media.getAllItem<SdpAttrCandidate>('a', "candidate");

        if (mline.type == TrackType::TrackApplication) {
            rtc_media.sctp_port = atoi(media.getStringItem('a', "sctp-port").data());
            rtc_media.sctpmap = media.getItemClass<SdpAttrSctpMap>('a', "sctpmap");
            continue;
        }
        rtc_media.rtcp_addr = media.getItemClass<SdpAttrRtcp>('a', "rtcp");
        rtc_media.direction = media.getDirection();
        rtc_media.extmap = media.getAllItem<SdpAttrExtmap>('a', "extmap");
        rtc_media.rtcp_mux = media.getItem('a', "rtcp-mux").operator bool();
        rtc_media.rtcp_rsize = media.getItem('a', "rtcp-rsize").operator bool();

        map<uint32_t, RtcSSRC> rtc_ssrc_map;
        for (auto &ssrc : media.getAllItem<SdpAttrSSRC>('a', "ssrc")) {
            auto &rtc_ssrc = rtc_ssrc_map[ssrc.ssrc];
            rtc_ssrc.ssrc = ssrc.ssrc;
            if (!strcasecmp(ssrc.attribute.data(), "cname")) {
                rtc_ssrc.cname = ssrc.attribute_value;
                continue;
            }
            if (!strcasecmp(ssrc.attribute.data(), "msid")) {
                rtc_ssrc.msid = ssrc.attribute_value;
                continue;
            }
            if (!strcasecmp(ssrc.attribute.data(), "mslabel")) {
                rtc_ssrc.mslabel = ssrc.attribute_value;
                continue;
            }
            if (!strcasecmp(ssrc.attribute.data(), "label")) {
                rtc_ssrc.label = ssrc.attribute_value;
                continue;
            }
        }

        uint32_t ssrc_rtp = 0, ssrc_rtx = 0, ssrc_rtp_low = 0, ssrc_rtp_mid = 0, ssrc_rtp_high = 0;
        auto ssrc_groups = media.getAllItem<SdpAttrSSRCGroup>('a', "ssrc-group");
        for (auto &group : ssrc_groups) {
            if (group.isFID()) {
                ssrc_rtp = group.u.fid.rtp_ssrc;
                ssrc_rtx = group.u.fid.rtx_ssrc;
            } else if (group.isSIM()) {
                ssrc_rtp_low = group.u.sim.rtp_ssrc_low;
                ssrc_rtp_mid = group.u.sim.rtp_ssrc_mid;
                ssrc_rtp_high = group.u.sim.rtp_ssrc_high;
            }
        }

        if (!ssrc_rtp) {
            //没有指定ssrc-group字段，那么只有一个ssrc
            if (rtc_ssrc_map.size() > 1) {
                throw std::invalid_argument("sdp中不存在a=ssrc-group:FID字段,但是ssrc却有多个");
            }
            ssrc_rtp = rtc_ssrc_map.begin()->second.ssrc;
        }
        for (auto &pr : rtc_ssrc_map) {
            auto &rtc_ssrc = pr.second;
            if (rtc_ssrc.ssrc == ssrc_rtp) {
                rtc_media.rtp_ssrc = rtc_ssrc;
            }
            if (rtc_ssrc.ssrc == ssrc_rtx) {
                rtc_media.rtx_ssrc = rtc_ssrc;
            }
            if (rtc_ssrc.ssrc == ssrc_rtp_low) {
                rtc_media.rtp_ssrc_low = rtc_ssrc;
            }
            if (rtc_ssrc.ssrc == ssrc_rtp_mid) {
                rtc_media.rtp_ssrc_mid = rtc_ssrc;
            }
            if (rtc_ssrc.ssrc == ssrc_rtp_high) {
                rtc_media.rtp_ssrc_high = rtc_ssrc;
            }
        }

        auto rtpmap_arr = media.getAllItem<SdpAttrRtpMap>('a', "rtpmap");
        auto rtcpfb_arr = media.getAllItem<SdpAttrRtcpFb>('a', "rtcp-fb");
        auto fmtp_aar = media.getAllItem<SdpAttrFmtp>('a', "fmtp");
        //方便根据pt查找rtpmap,一个pt必有一条
        map<uint8_t, SdpAttrRtpMap &> rtpmap_map;
        //方便根据pt查找rtcp-fb,一个pt可能有多条或0条
        multimap<uint8_t, SdpAttrRtcpFb &> rtcpfb_map;
        //方便根据pt查找fmtp，一个pt最多一条
        map<uint8_t, SdpAttrFmtp &> fmtp_map;

        for (auto &rtpmap : rtpmap_arr) {
            if (!rtpmap_map.emplace(rtpmap.pt, rtpmap).second) {
                //添加失败，有多条
                throw std::invalid_argument(StrPrinter << "该pt存在多条a=rtpmap:" << rtpmap.pt);
            }
        }
        for (auto &rtpfb : rtcpfb_arr) {
            rtcpfb_map.emplace(rtpfb.pt, rtpfb);
        }
        for (auto &fmtp : fmtp_aar) {
            if (!fmtp_map.emplace(fmtp.pt, fmtp).second) {
                //添加失败，有多条
                throw std::invalid_argument(StrPrinter << "该pt存在多条a=fmtp:" << fmtp.pt);
            }
        }
        for (auto &pt : mline.fmts) {
            //遍历所有编码方案的pt
            rtc_media.plan.emplace_back();
            auto &plan = rtc_media.plan.back();
            auto rtpmap_it = rtpmap_map.find(pt);
            if (rtpmap_it == rtpmap_map.end()) {
                throw std::invalid_argument(StrPrinter << "该pt不存在相对于的a=rtpmap:" << pt);
            }
            plan.pt = rtpmap_it->second.pt;
            plan.codec = rtpmap_it->second.codec;
            plan.sample_rate = rtpmap_it->second.sample_rate;
            plan.channel = rtpmap_it->second.channel;
            auto fmtp_it = fmtp_map.find(pt);
            if (fmtp_it != fmtp_map.end()) {
                plan.fmtp = fmtp_it->second.arr;
            }
            for (auto rtpfb_it = rtcpfb_map.find(pt);
                 rtpfb_it != rtcpfb_map.end() && rtpfb_it->second.pt == pt; ++rtpfb_it) {
                plan.rtcp_fb.emplace_back(rtpfb_it->second.rtcp_type);
            }
        }
    }

    group = sdp.getItemClass<SdpAttrGroup>('a', "group");
    checkValid();
}

std::shared_ptr<SdpItem> wrapSdpAttr(SdpItem::Ptr item){
    auto ret = std::make_shared<SdpAttr>();
    ret->detail = std::move(item);
    return ret;
}

string RtcSession::toString() const{
    checkValid();
    RtcSessionSdp sdp;
    sdp.items.emplace_back(std::make_shared<SdpString<'v'> >(to_string(version)));
    sdp.items.emplace_back(std::make_shared<SdpOrigin>(origin));
    sdp.items.emplace_back(std::make_shared<SdpString<'s'> >(session_name));
    if (!session_info.empty()) {
        sdp.items.emplace_back(std::make_shared<SdpString<'i'> >(session_info));
    }
    sdp.items.emplace_back(std::make_shared<SdpTime>(time));
    sdp.items.emplace_back(std::make_shared<SdpConnection>(connection));
    if (!bandwidth.empty()) {
        sdp.items.emplace_back(std::make_shared<SdpBandwidth>(bandwidth));
    }
    sdp.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrMsidSemantic>(msid_semantic)));
    sdp.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrGroup>(group)));
    for (auto &m : media) {
        sdp.medias.emplace_back();
        auto &sdp_media = sdp.medias.back();
        auto mline = std::make_shared<SdpMedia>();
        mline->type = m.type;
        mline->port = m.port;
        mline->proto = m.proto;
        for (auto &p : m.plan) {
            mline->fmts.emplace_back(p.pt);
        }
        if (m.type == TrackApplication) {
            mline->fmts.emplace_back(m.sctp_port);
        }
        sdp_media.items.emplace_back(std::move(mline));
        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrMid>(m.mid)));
        sdp_media.items.emplace_back(std::make_shared<SdpConnection>(m.addr));
        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrIceUfrag>(m.ice_ufrag)));
        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrIcePwd>(m.ice_pwd)));
        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrFingerprint>(m.fingerprint)));
        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrSetup>(m.role)));
        if (m.ice_trickle) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpCommon>("ice-trickle")));
        }
        if (m.ice_lite) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpCommon>("ice-lite")));
        }
        if (m.ice_renomination) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpCommon>("ice-renomination")));
        }
        for (auto &ext : m.extmap) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrExtmap>(ext)));
        }
        if (m.direction != RtpDirection::invalid) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<DirectionInterfaceImp>(m.direction)));
        }
        if (m.rtcp_addr.port) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrRtcp>(m.rtcp_addr)));
        }
        if (m.rtcp_mux) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpCommon>("rtcp-mux")));
        }
        if (m.rtcp_rsize) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpCommon>("rtcp-rsize")));
        }

        if(m.type != TrackApplication) {
            for (auto &p : m.plan) {
                auto rtp_map = std::make_shared<SdpAttrRtpMap>();
                rtp_map->pt = p.pt;
                rtp_map->codec = p.codec;
                rtp_map->sample_rate = p.sample_rate;
                rtp_map->channel = p.channel;
                sdp_media.items.emplace_back(wrapSdpAttr(std::move(rtp_map)));

                for (auto &fb :  p.rtcp_fb) {
                    auto rtcp_fb = std::make_shared<SdpAttrRtcpFb>();
                    rtcp_fb->pt = p.pt;
                    rtcp_fb->rtcp_type = fb;
                    sdp_media.items.emplace_back(wrapSdpAttr(std::move(rtcp_fb)));
                }

                if (!p.fmtp.empty()) {
                    auto fmtp = std::make_shared<SdpAttrFmtp>();
                    fmtp->pt = p.pt;
                    fmtp->arr = p.fmtp;
                    sdp_media.items.emplace_back(wrapSdpAttr(std::move(fmtp)));
                }
            }

            if (!m.rtp_ssrc.empty() && !m.rtx_ssrc.empty()) {
                auto group = std::make_shared<SdpAttrSSRCGroup>();
                group->type = "FID";
                group->u.fid.rtp_ssrc = m.rtp_ssrc.ssrc;
                group->u.fid.rtx_ssrc = m.rtx_ssrc.ssrc;
                sdp_media.items.emplace_back(wrapSdpAttr(std::move(group)));
            }

            static auto addSSRCItem = [](const RtcSSRC &rtp_ssrc, vector<SdpItem::Ptr> &items) {
                SdpAttrSSRC ssrc;
                ssrc.ssrc = rtp_ssrc.ssrc;

                ssrc.attribute = "cname";
                ssrc.attribute_value = rtp_ssrc.cname;
                items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrSSRC>(ssrc)));

                if (!rtp_ssrc.msid.empty()) {
                    ssrc.attribute = "msid";
                    ssrc.attribute_value = rtp_ssrc.msid;
                    items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrSSRC>(ssrc)));
                }

                if (!rtp_ssrc.mslabel.empty()) {
                    ssrc.attribute = "mslabel";
                    ssrc.attribute_value = rtp_ssrc.mslabel;
                    items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrSSRC>(ssrc)));
                }

                if (!rtp_ssrc.label.empty()) {
                    ssrc.attribute = "label";
                    ssrc.attribute_value = rtp_ssrc.label;
                    items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrSSRC>(ssrc)));
                }
            };
            if (!m.rtp_ssrc.empty()) {
                addSSRCItem(m.rtp_ssrc, sdp_media.items);
            }
            if (!m.rtx_ssrc.empty()) {
                addSSRCItem(m.rtx_ssrc, sdp_media.items);
            }

            bool enable_sim = false;
            if (!m.rtp_ssrc_low.empty() && !m.rtp_ssrc_mid.empty() && !m.rtp_ssrc_high.empty()) {
                auto group = std::make_shared<SdpAttrSSRCGroup>();
                group->type = "SIM";
                group->u.sim.rtp_ssrc_low = m.rtp_ssrc_low.ssrc;
                group->u.sim.rtp_ssrc_mid = m.rtp_ssrc_mid.ssrc;
                group->u.sim.rtp_ssrc_high = m.rtp_ssrc_high.ssrc;
                sdp_media.items.emplace_back(wrapSdpAttr(std::move(group)));
                enable_sim = true;
            }
            if (enable_sim) {
                addSSRCItem(m.rtp_ssrc_low, sdp_media.items);
                addSSRCItem(m.rtp_ssrc_mid, sdp_media.items);
                addSSRCItem(m.rtp_ssrc_high, sdp_media.items);
            }
        } else {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrSctpMap>(m.sctpmap)));
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpCommon>("sctp-port", to_string(m.sctp_port))));
        }

        for (auto &cand : m.candidate) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrCandidate>(cand)));
        }
    }
    return sdp.toString();
}

string RtcCodecPlan::getFmtp(const char *key) const{
    for (auto &item : fmtp) {
        if (strcasecmp(item.first.data(), key) == 0) {
            return item.second;
        }
    }
    return "";
}

const RtcCodecPlan *RtcMedia::getPlan(uint8_t pt) const{
    for (auto &item : plan) {
        if (item.pt == pt) {
            return &item;
        }
    }
    return nullptr;
}

const RtcCodecPlan *RtcMedia::getPlan(const char *codec) const{
    for (auto &item : plan) {
        if (strcasecmp(item.codec.data(), codec) == 0) {
            return &item;
        }
    }
    return nullptr;
}

const RtcCodecPlan *RtcMedia::getRelatedRtxPlan(uint8_t pt) const{
    for (auto &item : plan) {
        if (strcasecmp(item.codec.data(), "rtx") == 0) {
            auto apt = atoi(item.getFmtp("apt").data());
            if (pt == apt) {
                return &item;
            }
        }
    }
    return nullptr;
}

void RtcMedia::checkValid() const{
    CHECK(type != TrackInvalid);
    CHECK(!mid.empty());
    CHECK(!proto.empty());
    CHECK(direction != RtpDirection::invalid || type == TrackApplication);
    CHECK(!plan.empty() || type == TrackApplication );
    bool send_rtp = (direction == RtpDirection::sendonly || direction == RtpDirection::sendrecv);
    CHECK(!rtp_ssrc.empty() || !send_rtp);
    auto rtx_plan = getPlan("rtx");
    if (rtx_plan) {
        //开启rtx后必须指定rtx_ssrc
        CHECK(!rtx_ssrc.empty() || !send_rtp);
    }
}

void RtcSession::checkValid() const{
    CHECK(version == 0);
    CHECK(!origin.empty());
    CHECK(!session_name.empty());
    CHECK(!msid_semantic.empty());
    CHECK(!media.empty());
    CHECK(group.mids.size() <= media.size());
    for (auto &item : media) {
        item.checkValid();
    }
}

void RtcConfigure::RtcTrackConfigure::setDefaultSetting(TrackType type){
    enable = true;
    rtcp_mux = true;
    rtcp_rsize = false;
    group_bundle = true;
    unified_plan = false;
    support_rtx = true;
    support_red = false;
    support_ulpfec = false;
    support_simulcast = false;
    ice_lite = true;
    ice_trickle = true;
    ice_renomination = false;
    switch (type) {
        case TrackVideo: {
            preferred_codec = {CodecAAC, CodecOpus, CodecG711U, CodecG711A};
            rtcp_fb = {"transport-cc"};
            extmap = {"1 urn:ietf:params:rtp-hdrext:ssrc-audio-level"};
            break;
        }
        case TrackAudio: {
            preferred_codec = {CodecH264, CodecH265};
            rtcp_fb = {"nack", "ccm fir", "nack pli", "goog-remb", "transport-cc"};
            extmap = {"2 urn:ietf:params:rtp-hdrext:toffset",
                      "3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time",
                      "4 urn:3gpp:video-orientation",
                      "5 http://www.ietf.org/id/draft-holmer-rmcat-transport-wide-cc-extensions-01",
                      "6 http://www.webrtc.org/experiments/rtp-hdrext/playout-delay"};
            break;
        }
        case TrackApplication: {
            enable = false;
            break;
        }
        default: break;
    }
}

void RtcConfigure::setDefaultSetting(string ice_ufrag,
                                     string ice_pwd,
                                     DtlsRole role,
                                     RtpDirection direction,
                                     const SdpAttrFingerprint &fingerprint) {

    video.setDefaultSetting(TrackVideo);
    audio.setDefaultSetting(TrackAudio);
    application.setDefaultSetting(TrackApplication);

    video.ice_ufrag = audio.ice_ufrag = application.ice_ufrag = ice_ufrag;
    video.ice_pwd = audio.ice_pwd = application.ice_pwd = ice_ufrag;
    video.role = audio.role = application.role = role;
    video.direction = audio.direction = application.direction = direction;
    video.fingerprint = audio.fingerprint = application.fingerprint = fingerprint;
}

void RtcConfigure::addCandidate(const SdpAttrCandidate &candidate, TrackType type) {
    switch (type) {
        case TrackAudio: {
            audio.candidate.emplace_back(candidate);
            break;
        }
        case TrackVideo: {
            video.candidate.emplace_back(candidate);
            break;
        }
        case TrackApplication: {
            application.candidate.emplace_back(candidate);
            break;
        }
        default: {
            audio.candidate.emplace_back(candidate);
            video.candidate.emplace_back(candidate);
            application.candidate.emplace_back(candidate);
            break;
        }
    }
}