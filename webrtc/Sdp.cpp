/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "Sdp.h"
#include "Rtsp/Rtsp.h"
#include <cinttypes>

using namespace std;
using namespace toolkit;

namespace mediakit {

namespace Rtc {
#define RTC_FIELD "rtc."
const string kPreferredCodecA = RTC_FIELD"preferredCodecA";
const string kPreferredCodecV = RTC_FIELD"preferredCodecV";
static onceToken token([]() {
    mINI::Instance()[kPreferredCodecA] = "PCMU,PCMA,opus,mpeg4-generic";
    mINI::Instance()[kPreferredCodecV] = "H264,H265,AV1X,VP9,VP8";
});
}

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
    registerSdpItem<SdpAttrMsid>();
    registerSdpItem<SdpAttrExtmapAllowMixed>();
    registerSdpItem<SdpAttrRid>();
    registerSdpItem<SdpAttrSimulcast>();
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
    std::string key(1, key_c);
    for (auto item : items) {
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

//////////////////////////////////////////////////////////////////////////
int RtcSessionSdp::getVersion() const {
    return atoi(getStringItem('v').data());
}

SdpOrigin RtcSessionSdp::getOrigin() const {
    return getItemClass<SdpOrigin>('o');
}

string RtcSessionSdp::getSessionName() const {
    return getStringItem('s');
}

string RtcSessionSdp::getSessionInfo() const {
    return getStringItem('i');
}

SdpTime RtcSessionSdp::getSessionTime() const{
    return getItemClass<SdpTime>('t');
}

SdpConnection RtcSessionSdp::getConnection() const {
    return getItemClass<SdpConnection>('c');
}

SdpBandwidth RtcSessionSdp::getBandwidth() const {
    return getItemClass<SdpBandwidth>('b');
}

string RtcSessionSdp::getUri() const {
    return getStringItem('u');
}

string RtcSessionSdp::getEmail() const {
    return getStringItem('e');
}

string RtcSessionSdp::getPhone() const {
    return getStringItem('p');
}

string RtcSessionSdp::getTimeZone() const {
    return getStringItem('z');
}

string RtcSessionSdp::getEncryptKey() const {
    return getStringItem('k');
}

string RtcSessionSdp::getRepeatTimes() const {
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
            media->addItem(std::move(item));
        } else {
            addItem(std::move(item));
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

#define CHECK_SDP(exp) CHECK(exp, "解析sdp ", getKey(), " 字段失败:", str)

void SdpTime::parse(const string &str) {
    CHECK_SDP(sscanf(str.data(), "%" SCNu64 " %" SCNu64, &start, &stop) == 2);
}

string SdpTime::toString() const {
    if (value.empty()) {
        value = to_string(start) + " " + to_string(stop);
    }
    return SdpItem::toString();
}

void SdpOrigin::parse(const string &str) {
    auto vec = split(str, " ");
    CHECK_SDP(vec.size() == 6);
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
    CHECK_SDP(vec.size() == 3);
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
    CHECK_SDP(vec.size() == 2);
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
    CHECK_SDP(vec.size() >= 4);
    type = getTrackType(vec[0]);
    CHECK_SDP(type != TrackInvalid);
    port = atoi(vec[1].data());
    proto = vec[2];
    for (size_t i = 3; i < vec.size(); ++i) {
        fmts.emplace_back(vec[i]);
    }
}

string SdpMedia::toString() const {
    if (value.empty()) {
        value = string(getTrackString(type)) + " " + to_string(port) + " " + proto;
        for (auto fmt : fmts) {
            value += ' ';
            value += fmt;
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
    CHECK_SDP(vec.size() >= 2);
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
    CHECK_SDP(vec.size() >= 1);
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
    CHECK_SDP(vec.size() == 4);
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

void SdpAttrIceOption::parse(const string &str){
    auto vec = split(str, " ");
    for (auto &v : vec) {
        if (!strcasecmp(v.data(), "trickle")) {
            trickle = true;
            continue;
        }
        if (!strcasecmp(v.data(), "renomination")) {
            renomination = true;
            continue;
        }
    }
}

string SdpAttrIceOption::toString() const{
    if (value.empty()) {
        if (trickle && renomination) {
            value = "trickle renomination";
        } else if (trickle) {
            value = "trickle";
        } else if (renomination) {
            value = "renomination";
        }
    }
    return value;
}

void SdpAttrFingerprint::parse(const string &str)  {
    auto vec = split(str, " ");
    CHECK_SDP(vec.size() == 2);
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
    CHECK_SDP(role != DtlsRole::invalid);
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
    if (sscanf(str.data(), "%" SCNd8 "/%31[^ ] %127s", &id, direction_buf, buf) != 3) {
        CHECK_SDP(sscanf(str.data(), "%" SCNd8 " %127s", &id, buf) == 2);
        direction = RtpDirection::sendrecv;
    } else {
        direction = getRtpDirection(direction_buf);
    }
    ext = buf;
}

string SdpAttrExtmap::toString() const  {
    if (value.empty()) {
        if(direction == RtpDirection::invalid || direction == RtpDirection::sendrecv){
            value = to_string((int)id) + " " + ext;
        } else {
            value = to_string((int)id) + "/" + getRtpDirectionString(direction) +  " " + ext;
        }
    }
    return SdpItem::toString();
}

void SdpAttrRtpMap::parse(const string &str)  {
    char buf[32] = {0};
    if (sscanf(str.data(), "%" SCNu8 " %31[^/]/%" SCNd32 "/%" SCNd32, &pt, buf, &sample_rate, &channel) != 4) {
        CHECK_SDP(sscanf(str.data(), "%" SCNu8 " %31[^/]/%" SCNd32, &pt, buf, &sample_rate) == 3);
        if (getTrackType(getCodecId(buf)) == TrackAudio) {
            //未指定通道数时，且为音频时，那么通道数默认为1
            channel = 1;
        }
    }
    codec = buf;
}

string SdpAttrRtpMap::toString() const  {
    if (value.empty()) {
        value = to_string((int)pt) + " " + codec + "/" + to_string(sample_rate);
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
    CHECK_SDP(sscanf(str.data(), "%" SCNu8 " %31[^\n]", &pt, rtcp_type_buf) == 2);
    rtcp_type = rtcp_type_buf;
}

string SdpAttrRtcpFb::toString() const  {
    if (value.empty()) {
        value = to_string((int)pt) + " " + rtcp_type;
    }
    return SdpItem::toString();
}

void SdpAttrFmtp::parse(const string &str)  {
    auto pos = str.find(' ');
    CHECK_SDP(pos != string::npos);
    pt = atoi(str.substr(0, pos).data());
    auto vec = split(str.substr(pos + 1), ";");
    for (auto &item : vec) {
        trim(item);
        auto pos = item.find('=');
        if(pos == string::npos){
            fmtp.emplace(std::make_pair(item, ""));
        }  else {
            fmtp.emplace(std::make_pair(item.substr(0, pos), item.substr(pos + 1)));
        }
    }
    CHECK_SDP(!fmtp.empty());
}

string SdpAttrFmtp::toString() const  {
    if (value.empty()) {
        value = to_string((int)pt);
        int i = 0;
        for (auto &pr : fmtp) {
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
        CHECK_SDP(0);
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
    CHECK_SDP(vec.size() >= 3);
    type = std::move(vec[0]);
    CHECK(isFID() || isSIM());
    vec.erase(vec.begin());
    for (auto ssrc : vec) {
        ssrcs.emplace_back((uint32_t) atoll(ssrc.data()));
    }
}

string SdpAttrSSRCGroup::toString() const  {
    if (value.empty()) {
        value = type;
        //最少要求2个ssrc
        CHECK(ssrcs.size() >= 2);
        for (auto &ssrc : ssrcs) {
            value += ' ';
            value += to_string(ssrc);
        }
    }
    return SdpItem::toString();
}

void SdpAttrSctpMap::parse(const string &str)  {
    char subtypes_buf[64] = {0};
    CHECK_SDP(3 == sscanf(str.data(), "%" SCNu16 " %63[^ ] %" SCNd32, &port, subtypes_buf, &streams));
    subtypes = subtypes_buf;
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
    char foundation_buf[40] = {0};
    char transport_buf[16] = {0};
    char address_buf[32] = {0};
    char type_buf[16] = {0};

    // https://datatracker.ietf.org/doc/html/rfc5245#section-15.1
    CHECK_SDP(sscanf(str.data(), "%32[^ ] %" SCNu32 " %15[^ ] %" SCNu32 " %31[^ ] %" SCNu16 " typ %15[^ ]",
            foundation_buf, &component, transport_buf, &priority, address_buf, &port, type_buf) == 7);
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

void SdpAttrSimulcast::parse(const string &str) {
    //https://www.meetecho.com/blog/simulcast-janus-ssrc/
    //a=simulcast:send/recv q;h;f
    //a=simulcast:send/recv [rid=]q;h;f
    //a=simulcast: recv h;m;l
    //
    auto vec = split(str, " ");
    CHECK_SDP(vec.size() == 2);
    direction = vec[0];
    rids = split(vec[1], ";");
}

string SdpAttrSimulcast::toString() const {
    if (value.empty()) {
        value = direction + " ";
        bool first = true;
        for (auto &rid : rids) {
            if (first) {
                first = false;
            } else {
                value += ';';
            }
            value += rid;
        }
    }
    return SdpItem::toString();
}

void SdpAttrRid::parse(const string &str) {
    auto vec = split(str, " ");
    CHECK(vec.size() >= 2);
    rid = vec[0];
    direction = vec[1];
}

string SdpAttrRid::toString() const {
    if (value.empty()) {
        value = rid + " " + direction;
    }
    return SdpItem::toString();
}

void RtcSession::loadFrom(const string &str) {
    RtcSessionSdp sdp;
    sdp.parse(str);

    version = sdp.getVersion();
    origin = sdp.getOrigin();
    session_name = sdp.getSessionName();
    session_info = sdp.getSessionInfo();
    connection = sdp.getConnection();
    time = sdp.getSessionTime();
    msid_semantic = sdp.getItemClass<SdpAttrMsidSemantic>('a', "msid-semantic");
    for (auto &media : sdp.medias) {
        auto mline = media.getItemClass<SdpMedia>('m');
        this->media.emplace_back();
        auto &rtc_media = this->media.back();
        rtc_media.mid = media.getStringItem('a', "mid");
        rtc_media.proto = mline.proto;
        rtc_media.type = mline.type;
        rtc_media.port = mline.port;
        rtc_media.addr = media.getItemClass<SdpConnection>('c');
        rtc_media.bandwidth = media.getItemClass<SdpBandwidth>('b');
        rtc_media.ice_ufrag = media.getStringItem('a', "ice-ufrag");
        rtc_media.ice_pwd = media.getStringItem('a', "ice-pwd");
        rtc_media.role = media.getItemClass<SdpAttrSetup>('a', "setup").role;
        rtc_media.fingerprint = media.getItemClass<SdpAttrFingerprint>('a', "fingerprint");
        if (rtc_media.fingerprint.empty()) {
            rtc_media.fingerprint = sdp.getItemClass<SdpAttrFingerprint>('a', "fingerprint");
        }
        rtc_media.ice_lite = media.getItem('a', "ice-lite").operator bool();
        auto ice_options = media.getItemClass<SdpAttrIceOption>('a', "ice-options");
        rtc_media.ice_trickle = ice_options.trickle;
        rtc_media.ice_renomination = ice_options.renomination;
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
        auto ssrc_attr = media.getAllItem<SdpAttrSSRC>('a', "ssrc");
        for (auto &ssrc : ssrc_attr) {
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

        auto ssrc_groups = media.getAllItem<SdpAttrSSRCGroup>('a', "ssrc-group");
        bool have_rtx_ssrc = false;
        SdpAttrSSRCGroup *ssrc_group_sim = nullptr;
        for (auto &group : ssrc_groups) {
            if (group.isFID()) {
                have_rtx_ssrc = true;
                //ssrc-group:FID字段必须包含rtp/rtx的ssrc
                CHECK(group.ssrcs.size() == 2);
                //根据rtp ssrc找到对象
                auto it = rtc_ssrc_map.find(group.ssrcs[0]);
                CHECK(it != rtc_ssrc_map.end());
                //设置rtx ssrc
                it->second.rtx_ssrc = group.ssrcs[1];
                rtc_media.rtp_rtx_ssrc.emplace_back(it->second);
            } else if (group.isSIM()) {
                CHECK(!ssrc_group_sim);
                ssrc_group_sim = &group;
            }
        }

        if (!have_rtx_ssrc) {
            //按照sdp顺序依次添加ssrc
            for (auto &attr : ssrc_attr) {
                if (attr.attribute == "cname") {
                    rtc_media.rtp_rtx_ssrc.emplace_back(rtc_ssrc_map[attr.ssrc]);
                }
            }
        }

        auto simulcast = media.getItemClass<SdpAttrSimulcast>('a', "simulcast");
        if (!simulcast.empty()) {
            // a=rid:h send
            // a=rid:m send
            // a=rid:l send
            // a=simulcast:send h;m;l
            // 风格的simulcast
            unordered_set<string> rid_map;
            for (auto &rid : simulcast.rids) {
                rid_map.emplace(rid);
            }
            for (auto &rid : media.getAllItem<SdpAttrRid>('a', "rid")) {
                CHECK(rid.direction == simulcast.direction);
                CHECK(rid_map.find(rid.rid) != rid_map.end());
            }
            //simulcast最少要求2种方案
            CHECK(simulcast.rids.size() >= 2);
            rtc_media.rtp_rids = simulcast.rids;
        }

        if (ssrc_group_sim) {
            //指定了a=ssrc-group:SIM
            for (auto ssrc : ssrc_group_sim->ssrcs) {
                auto it = rtc_ssrc_map.find(ssrc);
                CHECK(it != rtc_ssrc_map.end());
                rtc_media.rtp_ssrc_sim.emplace_back(it->second);
            }
        } else if (!rtc_media.rtp_rids.empty()) {
            //未指定a=ssrc-group:SIM, 但是指定了a=simulcast, 那么只能根据ssrc顺序来对应rid顺序
            rtc_media.rtp_ssrc_sim = rtc_media.rtp_rtx_ssrc;
        }

        if (!rtc_media.supportSimulcast()) {
            //不支持simulcast的情况下，最多一组ssrc
            CHECK(rtc_media.rtp_rtx_ssrc.size() <= 1);
        } else {
            //simulcast的情况下，要么没有指定ssrc，要么指定的ssrc个数与rid个数一致
            //CHECK(rtc_media.rtp_ssrc_sim.empty() || rtc_media.rtp_ssrc_sim.size() == rtc_media.rtp_rids.size());
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
            //添加失败，有多条
            CHECK(rtpmap_map.emplace(rtpmap.pt, rtpmap).second, "该pt存在多条a=rtpmap:", (int)rtpmap.pt);
        }
        for (auto &rtpfb : rtcpfb_arr) {
            rtcpfb_map.emplace(rtpfb.pt, rtpfb);
        }
        for (auto &fmtp : fmtp_aar) {
            //添加失败，有多条
            CHECK(fmtp_map.emplace(fmtp.pt, fmtp).second, "该pt存在多条a=fmtp:", (int)fmtp.pt);
        }
        for (auto &item : mline.fmts) {
            auto pt = atoi(item.c_str());
            CHECK(pt < 0xFF, "invalid payload type: ", item);
            //遍历所有编码方案的pt
            rtc_media.plan.emplace_back();
            auto &plan = rtc_media.plan.back();
            auto rtpmap_it = rtpmap_map.find(pt);
            if (rtpmap_it == rtpmap_map.end()) {
                plan.pt = pt;
                plan.codec = RtpPayload::getName(pt);
                plan.sample_rate = RtpPayload::getClockRate(pt);
                plan.channel = RtpPayload::getAudioChannel(pt);
            } else {
                plan.pt = rtpmap_it->second.pt;
                plan.codec = rtpmap_it->second.codec;
                plan.sample_rate = rtpmap_it->second.sample_rate;
                plan.channel = rtpmap_it->second.channel;
            }

            auto fmtp_it = fmtp_map.find(pt);
            if (fmtp_it != fmtp_map.end()) {
                plan.fmtp = fmtp_it->second.fmtp;
            }
            for (auto rtpfb_it = rtcpfb_map.find(pt);
                 rtpfb_it != rtcpfb_map.end() && rtpfb_it->second.pt == pt; ++rtpfb_it) {
                plan.rtcp_fb.emplace(rtpfb_it->second.rtcp_type);
            }
        }
    }

    group = sdp.getItemClass<SdpAttrGroup>('a', "group");
}

void RtcSdpBase::toRtsp() {
    for (auto it = items.begin(); it != items.end();) {
        switch ((*it)->getKey()[0]) {
            case 'v':
            case 'o':
            case 's':
            case 'i':
            case 't':
            case 'c':
            case 'b':{
                ++it;
                break;
            }

            case 'm': {
                auto m = dynamic_pointer_cast<SdpMedia>(*it);
                CHECK(m);
                m->proto = "RTP/AVP";
                ++it;
                break;
            }
            case 'a': {
                auto attr = dynamic_pointer_cast<SdpAttr>(*it);
                CHECK(attr);
                if (!strcasecmp(attr->detail->getKey(), "rtpmap")
                    || !strcasecmp(attr->detail->getKey(), "fmtp")) {
                    ++it;
                    break;
                }
            }
            default: {
                it = items.erase(it);
                break;
            }
        }
    }
}

string RtcSession::toRtspSdp() const{
    RtcSession copy = *this;
    copy.media.clear();
    for (auto &m : media) {
        switch (m.type) {
            case TrackAudio:
            case TrackVideo: {
                if (m.direction != RtpDirection::inactive) {
                    copy.media.emplace_back(m);
                    copy.media.back().plan.resize(1);
                }
                break;
            }
            default: continue;
        }
    }

    CHECK(!copy.media.empty());
    auto sdp = copy.toRtcSessionSdp();
    sdp->toRtsp();
    int i = 0;
    for (auto &m : sdp->medias) {
        m.toRtsp();
        m.addAttr(std::make_shared<SdpCommon>("control", string("trackID=") + to_string(i++)));
    }
    return sdp->toString();
}

void addSdpAttrSSRC(const RtcSSRC &rtp_ssrc, RtcSdpBase &media, uint32_t ssrc_num) {
    assert(ssrc_num);
    SdpAttrSSRC ssrc;
    ssrc.ssrc = ssrc_num;

    ssrc.attribute = "cname";
    ssrc.attribute_value = rtp_ssrc.cname;
    media.addAttr(std::make_shared<SdpAttrSSRC>(ssrc));

    if (!rtp_ssrc.msid.empty()) {
        ssrc.attribute = "msid";
        ssrc.attribute_value = rtp_ssrc.msid;
        media.addAttr(std::make_shared<SdpAttrSSRC>(ssrc));
    }

    if (!rtp_ssrc.mslabel.empty()) {
        ssrc.attribute = "mslabel";
        ssrc.attribute_value = rtp_ssrc.mslabel;
        media.addAttr(std::make_shared<SdpAttrSSRC>(ssrc));
    }

    if (!rtp_ssrc.label.empty()) {
        ssrc.attribute = "label";
        ssrc.attribute_value = rtp_ssrc.label;
        media.addAttr(std::make_shared<SdpAttrSSRC>(ssrc));
    }
}

RtcSessionSdp::Ptr RtcSession::toRtcSessionSdp() const{
    RtcSessionSdp::Ptr ret = std::make_shared<RtcSessionSdp>();
    auto &sdp = *ret;
    sdp.addItem(std::make_shared<SdpString<'v'> >(to_string(version)));
    sdp.addItem(std::make_shared<SdpOrigin>(origin));
    sdp.addItem(std::make_shared<SdpString<'s'> >(session_name));
    if (!session_info.empty()) {
        sdp.addItem(std::make_shared<SdpString<'i'> >(session_info));
    }
    sdp.addItem(std::make_shared<SdpTime>(time));
    if(connection.empty()){
        sdp.addItem(std::make_shared<SdpConnection>(connection));
    }
    sdp.addAttr(std::make_shared<SdpAttrGroup>(group));
    sdp.addAttr(std::make_shared<SdpAttrMsidSemantic>(msid_semantic));
    for (auto &m : media) {
        sdp.medias.emplace_back();
        auto &sdp_media = sdp.medias.back();
        auto mline = std::make_shared<SdpMedia>();
        mline->type = m.type;
        mline->port = m.port;
        mline->proto = m.proto;
        for (auto &p : m.plan) {
            mline->fmts.emplace_back(to_string((int)p.pt));
        }
        if (m.type == TrackApplication) {
            mline->fmts.emplace_back("webrtc-datachannel");
        }
        sdp_media.addItem(std::move(mline));
        sdp_media.addItem(std::make_shared<SdpConnection>(m.addr));
        if (!m.bandwidth.empty() && m.type != TrackAudio) {
            sdp_media.addItem(std::make_shared<SdpBandwidth>(m.bandwidth));
        }
        if (!m.rtcp_addr.empty()) {
            sdp_media.addAttr(std::make_shared<SdpAttrRtcp>(m.rtcp_addr));
        }

        sdp_media.addAttr(std::make_shared<SdpAttrIceUfrag>(m.ice_ufrag));
        sdp_media.addAttr(std::make_shared<SdpAttrIcePwd>(m.ice_pwd));
        if (m.ice_trickle || m.ice_renomination) {
            auto attr = std::make_shared<SdpAttrIceOption>();
            attr->trickle = m.ice_trickle;
            attr->renomination = m.ice_renomination;
            sdp_media.addAttr(attr);
        }
        sdp_media.addAttr(std::make_shared<SdpAttrFingerprint>(m.fingerprint));
        sdp_media.addAttr(std::make_shared<SdpAttrSetup>(m.role));
        sdp_media.addAttr(std::make_shared<SdpAttrMid>(m.mid));
        if (m.ice_lite) {
            sdp_media.addAttr(std::make_shared<SdpCommon>("ice-lite"));
        }
        for (auto &ext : m.extmap) {
            sdp_media.addAttr(std::make_shared<SdpAttrExtmap>(ext));
        }
        if (m.direction != RtpDirection::invalid) {
            sdp_media.addAttr(std::make_shared<DirectionInterfaceImp>(m.direction));
        }
        if (m.rtcp_mux) {
            sdp_media.addAttr(std::make_shared<SdpCommon>("rtcp-mux"));
        }
        if (m.rtcp_rsize) {
            sdp_media.addAttr(std::make_shared<SdpCommon>("rtcp-rsize"));
        }

        if(m.type != TrackApplication) {
            for (auto &p : m.plan) {
                auto rtp_map = std::make_shared<SdpAttrRtpMap>();
                rtp_map->pt = p.pt;
                rtp_map->codec = p.codec;
                rtp_map->sample_rate = p.sample_rate;
                rtp_map->channel = p.channel;
                //添加a=rtpmap
                sdp_media.addAttr(std::move(rtp_map));

                for (auto &fb :  p.rtcp_fb) {
                    auto rtcp_fb = std::make_shared<SdpAttrRtcpFb>();
                    rtcp_fb->pt = p.pt;
                    rtcp_fb->rtcp_type = fb;
                    //添加a=rtcp-fb
                    sdp_media.addAttr(std::move(rtcp_fb));
                }

                if (!p.fmtp.empty()) {
                    auto fmtp = std::make_shared<SdpAttrFmtp>();
                    fmtp->pt = p.pt;
                    fmtp->fmtp = p.fmtp;
                    //添加a=fmtp
                    sdp_media.addAttr(std::move(fmtp));
                }
            }

            {
                //添加a=msid字段
                if (!m.rtp_rtx_ssrc.empty()) {
                    if (!m.rtp_rtx_ssrc[0].msid.empty()) {
                        auto msid = std::make_shared<SdpAttrMsid>();
                        msid->parse(m.rtp_rtx_ssrc[0].msid);
                        sdp_media.addAttr(std::move(msid));
                    }
                }
            }

            {
                for (auto &ssrc : m.rtp_rtx_ssrc) {
                    //添加a=ssrc字段
                    CHECK(!ssrc.empty());
                    addSdpAttrSSRC(ssrc, sdp_media, ssrc.ssrc);
                    if (ssrc.rtx_ssrc) {
                        addSdpAttrSSRC(ssrc, sdp_media, ssrc.rtx_ssrc);

                        //生成a=ssrc-group:FID字段
                        //有rtx ssrc
                        auto group = std::make_shared<SdpAttrSSRCGroup>();
                        group->type = "FID";
                        group->ssrcs.emplace_back(ssrc.ssrc);
                        group->ssrcs.emplace_back(ssrc.rtx_ssrc);
                        sdp_media.addAttr(std::move(group));
                    }
                }
            }

            {
                if (m.rtp_ssrc_sim.size() >= 2) {
                    //simulcast 要求 2~3路
                    auto group = std::make_shared<SdpAttrSSRCGroup>();
                    for (auto &ssrc : m.rtp_ssrc_sim) {
                        group->ssrcs.emplace_back(ssrc.ssrc);
                    }
                    //添加a=ssrc-group:SIM字段
                    group->type = "SIM";
                    sdp_media.addAttr(std::move(group));
                }

                if (m.rtp_rids.size() >= 2) {
                    auto simulcast = std::make_shared<SdpAttrSimulcast>();
                    simulcast->direction = "recv";
                    simulcast->rids = m.rtp_rids;
                    sdp_media.addAttr(std::move(simulcast));

                    for (auto &rid : m.rtp_rids) {
                        auto attr_rid = std::make_shared<SdpAttrRid>();
                        attr_rid->rid = rid;
                        attr_rid->direction = "recv";
                        sdp_media.addAttr(std::move(attr_rid));
                    }
                }
            }

        } else {
            if (!m.sctpmap.empty()) {
                sdp_media.addAttr(std::make_shared<SdpAttrSctpMap>(m.sctpmap));
            }
            sdp_media.addAttr(std::make_shared<SdpCommon>("sctp-port", to_string(m.sctp_port)));
        }

        for (auto &cand : m.candidate) {
            sdp_media.addAttr(std::make_shared<SdpAttrCandidate>(cand));
        }
    }
    return ret;
}

string RtcSession::toString() const{
    return toRtcSessionSdp()->toString();
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

uint32_t RtcMedia::getRtpSSRC() const {
    if (rtp_rtx_ssrc.size()) {
        return rtp_rtx_ssrc[0].ssrc;
    }
    return 0;
}

uint32_t RtcMedia::getRtxSSRC() const {
    if (rtp_rtx_ssrc.size()) {
        return rtp_rtx_ssrc[0].rtx_ssrc;
    }
    return 0;
}

bool RtcMedia::supportSimulcast() const {
    if (!rtp_rids.empty()) {
        return true;
    }
    if (!rtp_ssrc_sim.empty()) {
        return true;
    }
    return false;
}

void RtcMedia::checkValid() const{
    CHECK(type != TrackInvalid);
    CHECK(!mid.empty());
    CHECK(!proto.empty());
    CHECK(direction != RtpDirection::invalid || type == TrackApplication);
    CHECK(!plan.empty() || type == TrackApplication);
    CHECK(type == TrackApplication || rtcp_mux, "只支持rtcp-mux模式");

    bool send_rtp = (direction == RtpDirection::sendonly || direction == RtpDirection::sendrecv);
    if (!supportSimulcast()) {
        //非simulcast时，检查有没有指定rtp ssrc
        CHECK(!rtp_rtx_ssrc.empty() || !send_rtp);
    }

#if 0
    //todo 发现Firefox(88.0)在mac平台下，开启rtx后没有指定ssrc
    auto rtx_plan = getPlan("rtx");
    if (rtx_plan) {
        //开启rtx后必须指定rtx_ssrc
        CHECK(rtp_rtx_ssrc.size() >= 2 || !send_rtp);
    }
#endif
}

void RtcSession::checkValid() const{
    CHECK(version == 0);
    CHECK(!origin.empty());
    CHECK(!session_name.empty());
    CHECK(!msid_semantic.empty());
    CHECK(!media.empty());
    CHECK(!group.mids.empty() && group.mids.size() <= media.size(), "只支持group BUNDLE模式");

    bool have_active_media = false;
    for (auto &item : media) {
        item.checkValid();

        if (TrackApplication == item.type) {
            have_active_media = true;
        }
        switch (item.direction) {
            case RtpDirection::sendrecv:
            case RtpDirection::sendonly:
            case RtpDirection::recvonly: have_active_media = true; break;
            default : break;
        }
    }
    CHECK(have_active_media, "必须确保最少有一个活跃的track");
}

const RtcMedia *RtcSession::getMedia(TrackType type) const{
    for(auto &m : media){
        if(m.type == type){
            return &m;
        }
    }
    return nullptr;
}

bool RtcSession::supportRtcpFb(const string &name, TrackType type) const {
    auto media = getMedia(type);
    if (!media) {
        return false;
    }
    auto &ref = media->plan[0].rtcp_fb;
    return ref.find(name) != ref.end();
}

bool RtcSession::supportSimulcast() const {
    for (auto &m : media) {
        if (m.supportSimulcast()) {
            return true;
        }
    }
    return false;
}

bool RtcSession::isOnlyDatachannel() const {
    return 1 == media.size() && TrackApplication == media[0].type;
}

string const SdpConst::kTWCCRtcpFb = "transport-cc";
string const SdpConst::kRembRtcpFb = "goog-remb";

void RtcConfigure::RtcTrackConfigure::enableTWCC(bool enable){
    if (!enable) {
        rtcp_fb.erase(SdpConst::kTWCCRtcpFb);
        extmap.erase(RtpExtType::transport_cc);
    } else {
        rtcp_fb.emplace(SdpConst::kTWCCRtcpFb);
        extmap.emplace(RtpExtType::transport_cc, RtpDirection::sendrecv);
    }
}

void RtcConfigure::RtcTrackConfigure::enableREMB(bool enable){
    if (!enable) {
        rtcp_fb.erase(SdpConst::kRembRtcpFb);
        extmap.erase(RtpExtType::abs_send_time);
    } else {
        rtcp_fb.emplace(SdpConst::kRembRtcpFb);
        extmap.emplace(RtpExtType::abs_send_time, RtpDirection::sendrecv);
    }
}

static vector<CodecId> toCodecArray(const string &str){
    vector<CodecId> ret;
    auto vec = split(str, ",");
    for (auto &s : vec) {
        auto codec = getCodecId(trim(s));
        if (codec != CodecInvalid) {
            ret.emplace_back(codec);
        }
    }
    return ret;
}

void RtcConfigure::RtcTrackConfigure::setDefaultSetting(TrackType type){
    rtcp_mux = true;
    rtcp_rsize = false;
    group_bundle = true;
    support_rtx = true;
    support_red = false;
    support_ulpfec = false;
    ice_lite = true;
    ice_trickle = true;
    ice_renomination = false;
    switch (type) {
        case TrackAudio: {
            //此处调整偏好的编码格式优先级
            GET_CONFIG_FUNC(vector<CodecId>, s_preferred_codec, Rtc::kPreferredCodecA, toCodecArray);
            CHECK(!s_preferred_codec.empty(), "rtc音频偏好codec不能为空");
            preferred_codec = s_preferred_codec;

            rtcp_fb = {SdpConst::kTWCCRtcpFb, SdpConst::kRembRtcpFb};
            extmap = {
                    {RtpExtType::ssrc_audio_level,            RtpDirection::sendrecv},
                    {RtpExtType::csrc_audio_level,            RtpDirection::sendrecv},
                    {RtpExtType::abs_send_time,               RtpDirection::sendrecv},
                    {RtpExtType::transport_cc,                RtpDirection::sendrecv},
                    //rtx重传rtp时，忽略sdes_mid类型的rtp ext,实测发现Firefox在接收rtx时，如果存在sdes_mid的ext,将导致无法播放
                    //{RtpExtType::sdes_mid,RtpDirection::sendrecv},
                    {RtpExtType::sdes_rtp_stream_id,          RtpDirection::sendrecv},
                    {RtpExtType::sdes_repaired_rtp_stream_id, RtpDirection::sendrecv}
            };
            break;
        }
        case TrackVideo: {
            //此处调整偏好的编码格式优先级
            GET_CONFIG_FUNC(vector<CodecId>, s_preferred_codec, Rtc::kPreferredCodecV, toCodecArray);
            CHECK(!s_preferred_codec.empty(), "rtc视频偏好codec不能为空");
            preferred_codec = s_preferred_codec;

            rtcp_fb = {SdpConst::kTWCCRtcpFb, SdpConst::kRembRtcpFb, "nack", "ccm fir", "nack pli"};
            extmap = {
                    {RtpExtType::abs_send_time,               RtpDirection::sendrecv},
                    {RtpExtType::transport_cc,                RtpDirection::sendrecv},
                    //rtx重传rtp时，忽略sdes_mid类型的rtp ext,实测发现Firefox在接收rtx时，如果存在sdes_mid的ext,将导致无法播放
                    //{RtpExtType::sdes_mid,RtpDirection::sendrecv},
                    {RtpExtType::sdes_rtp_stream_id,          RtpDirection::sendrecv},
                    {RtpExtType::sdes_repaired_rtp_stream_id, RtpDirection::sendrecv},
                    {RtpExtType::video_timing,                RtpDirection::sendrecv},
                    {RtpExtType::color_space,                 RtpDirection::sendrecv},
                    {RtpExtType::video_content_type,          RtpDirection::sendrecv},
                    {RtpExtType::playout_delay,               RtpDirection::sendrecv},
                    {RtpExtType::video_orientation,           RtpDirection::sendrecv},
                    {RtpExtType::toffset,                     RtpDirection::sendrecv},
                    {RtpExtType::framemarking,                RtpDirection::sendrecv}
            };
            break;
        }
        case TrackApplication: {
            break;
        }
        default: break;
    }
}

void RtcConfigure::setDefaultSetting(string ice_ufrag, string ice_pwd, RtpDirection direction,
                                     const SdpAttrFingerprint &fingerprint) {
    video.setDefaultSetting(TrackVideo);
    audio.setDefaultSetting(TrackAudio);
    application.setDefaultSetting(TrackApplication);

    video.ice_ufrag = audio.ice_ufrag = application.ice_ufrag = std::move(ice_ufrag);
    video.ice_pwd = audio.ice_pwd = application.ice_pwd = std::move(ice_pwd);
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
            if (audio.group_bundle) {
                audio.candidate.emplace_back(candidate);
            }
            if (video.group_bundle) {
                video.candidate.emplace_back(candidate);
            }
            if (application.group_bundle) {
                application.candidate.emplace_back(candidate);
            }
            break;
        }
    }
}

void RtcConfigure::enableTWCC(bool enable, TrackType type){
    switch (type) {
        case TrackAudio: {
            audio.enableTWCC(enable);
            break;
        }
        case TrackVideo: {
            video.enableTWCC(enable);
            break;
        }
        default: {
            audio.enableTWCC(enable);
            video.enableTWCC(enable);
            break;
        }
    }
}

void RtcConfigure::enableREMB(bool enable, TrackType type){
    switch (type) {
        case TrackAudio: {
            audio.enableREMB(enable);
            break;
        }
        case TrackVideo: {
            video.enableREMB(enable);
            break;
        }
        default: {
            audio.enableREMB(enable);
            video.enableREMB(enable);
            break;
        }
    }
}

shared_ptr<RtcSession> RtcConfigure::createAnswer(const RtcSession &offer) const {
    shared_ptr<RtcSession> ret = std::make_shared<RtcSession>();
    ret->version = offer.version;
    ret->origin = offer.origin;
    ret->session_name = offer.session_name;
    ret->msid_semantic = offer.msid_semantic;

    for (auto &m : offer.media) {
        matchMedia(ret, m);
    }

    //设置音视频端口复用
    if (!offer.group.mids.empty()) {
        for (auto &m : ret->media) {
            ret->group.mids.emplace_back(m.mid);
        }
    }
    return ret;
}

static RtpDirection matchDirection(RtpDirection offer_direction, RtpDirection supported){
    switch (offer_direction) {
        case RtpDirection::sendonly : {
            if (supported != RtpDirection::recvonly && supported != RtpDirection::sendrecv) {
                //我们不支持接收
                return RtpDirection::inactive;
            }
            return RtpDirection::recvonly;
        }

        case RtpDirection::recvonly : {
            if (supported != RtpDirection::sendonly && supported != RtpDirection::sendrecv) {
                //我们不支持发送
                return RtpDirection::inactive;
            }
            return RtpDirection::sendonly;
        }

        //对方支持发送接收，那么最终能力根据配置来决定
        case RtpDirection::sendrecv : return  (supported == RtpDirection::invalid ? RtpDirection::inactive : supported);
        case RtpDirection::inactive : return RtpDirection::inactive;
        default: return RtpDirection::invalid;
    }
}

static DtlsRole mathDtlsRole(DtlsRole role){
    switch (role) {
        case DtlsRole::actpass:
        case DtlsRole::active: return DtlsRole::passive;
        case DtlsRole::passive: return DtlsRole::active;
        default: CHECK(0, "invalid role:", getDtlsRoleString(role)); return DtlsRole::passive;
    }
}

void RtcConfigure::matchMedia(const std::shared_ptr<RtcSession> &ret,const RtcMedia &offer_media) const {
    bool check_profile = true;
    bool check_codec = true;
    const RtcTrackConfigure *cfg_ptr = nullptr;

    switch (offer_media.type) {
        case TrackAudio: cfg_ptr = &audio; break;
        case TrackVideo: cfg_ptr = &video; break;
        case TrackApplication: cfg_ptr = &application; break;
        default: return;
    }
    auto &configure = *cfg_ptr;

RETRY:

    if (offer_media.type == TrackApplication) {
        RtcMedia answer_media = offer_media;
        answer_media.role = mathDtlsRole(offer_media.role);
#ifdef ENABLE_SCTP
        answer_media.direction = matchDirection(offer_media.direction, configure.direction);
        answer_media.candidate = configure.candidate;
        answer_media.ice_ufrag = configure.ice_ufrag;
        answer_media.ice_pwd = configure.ice_pwd;
        answer_media.fingerprint = configure.fingerprint;
        answer_media.ice_lite = configure.ice_lite;
#else
        answer_media.direction = RtpDirection::inactive;
#endif
        ret->media.emplace_back(answer_media);
        return;
    }
    for (auto &codec : configure.preferred_codec) {
        if (offer_media.ice_lite && configure.ice_lite) {
            WarnL << "answer sdp配置为ice_lite模式，与offer sdp中的ice_lite模式冲突";
            continue;
        }
        const RtcCodecPlan *selected_plan = nullptr;
        for (auto &plan : offer_media.plan) {
            //先检查编码格式是否为偏好
            if (check_codec && getCodecId(plan.codec) != codec) {
                continue;
            }
            //命中偏好的编码格式,然后检查规格
            if (check_profile && !onCheckCodecProfile(plan, codec)) {
                continue;
            }
            //找到中意的codec
            selected_plan = &plan;
            break;
        }
        if (!selected_plan) {
            //offer中该媒体的所有的codec都不支持
            continue;
        }
        RtcMedia answer_media;
        answer_media.type = offer_media.type;
        answer_media.mid = offer_media.mid;
        answer_media.proto = offer_media.proto;
        answer_media.port = offer_media.port;
        answer_media.addr = offer_media.addr;
        answer_media.bandwidth = offer_media.bandwidth;
        answer_media.rtcp_addr = offer_media.rtcp_addr;
        answer_media.rtcp_mux = offer_media.rtcp_mux && configure.rtcp_mux;
        answer_media.rtcp_rsize = offer_media.rtcp_rsize && configure.rtcp_rsize;
        answer_media.ice_trickle = offer_media.ice_trickle && configure.ice_trickle;
        answer_media.ice_renomination = offer_media.ice_renomination && configure.ice_renomination;
        answer_media.ice_ufrag = configure.ice_ufrag;
        answer_media.ice_pwd = configure.ice_pwd;
        answer_media.fingerprint = configure.fingerprint;
        answer_media.ice_lite = configure.ice_lite;
        answer_media.candidate = configure.candidate;
        // copy simulicast setting
        answer_media.rtp_rids = offer_media.rtp_rids;
        answer_media.rtp_ssrc_sim = offer_media.rtp_ssrc_sim;

        answer_media.role = mathDtlsRole(offer_media.role);

        //如果codec匹配失败，那么禁用该track
        answer_media.direction = check_codec ? matchDirection(offer_media.direction, configure.direction)
                                             : RtpDirection::inactive;
        if (answer_media.direction == RtpDirection::invalid) {
            continue;
        }
        if (answer_media.direction == RtpDirection::sendrecv) {
            //如果是收发双向，那么我们拷贝offer sdp的ssrc，确保ssrc一致
            answer_media.rtp_rtx_ssrc = offer_media.rtp_rtx_ssrc;
        }

        //添加媒体plan
        answer_media.plan.emplace_back(*selected_plan);
        onSelectPlan(answer_media.plan.back(), codec);

        set<uint8_t> pt_selected = {selected_plan->pt};

        //添加rtx,red,ulpfec plan
        if (configure.support_red || configure.support_rtx || configure.support_ulpfec) {
            for (auto &plan : offer_media.plan) {
                if (!strcasecmp(plan.codec.data(), "rtx")) {
                    if (configure.support_rtx && atoi(plan.getFmtp("apt").data()) == selected_plan->pt) {
                        answer_media.plan.emplace_back(plan);
                        pt_selected.emplace(plan.pt);
                    }
                    continue;
                }
                if (!strcasecmp(plan.codec.data(), "red")) {
                    if (configure.support_red) {
                        answer_media.plan.emplace_back(plan);
                        pt_selected.emplace(plan.pt);
                    }
                    continue;
                }
                if (!strcasecmp(plan.codec.data(), "ulpfec")) {
                    if (configure.support_ulpfec) {
                        answer_media.plan.emplace_back(plan);
                        pt_selected.emplace(plan.pt);
                    }
                    continue;
                }
            }
        }

        //对方和我方都支持的扩展，那么我们才支持
        for (auto &ext : offer_media.extmap) {
            auto it = configure.extmap.find(RtpExt::getExtType(ext.ext));
            if (it != configure.extmap.end()) {
                auto new_dir = matchDirection(ext.direction, it->second);
                switch (new_dir) {
                    case RtpDirection::invalid:
                    case RtpDirection::inactive: continue;
                    default: break;
                }
                answer_media.extmap.emplace_back(ext);
                answer_media.extmap.back().direction = new_dir;
            }
        }

        auto &rtcp_fb_ref = answer_media.plan[0].rtcp_fb;
        rtcp_fb_ref.clear();
        //对方和我方都支持的rtcpfb，那么我们才支持
        for (auto &fp : selected_plan->rtcp_fb) {
            if (configure.rtcp_fb.find(fp) != configure.rtcp_fb.end()) {
                //对方该rtcp被我们支持
                rtcp_fb_ref.emplace(fp);
            }
        }

#if 0
        //todo 此处为添加无效的plan，webrtc sdp通过调节plan pt顺序选择匹配的codec，意味着后面的codec其实放在sdp中是无意义的
        for (auto &plan : offer_media.plan) {
            if (pt_selected.find(plan.pt) == pt_selected.end()) {
                answer_media.plan.emplace_back(plan);
            }
        }
#endif
        ret->media.emplace_back(answer_media);
        return;
    }

    if (check_profile) {
        //如果是由于检查profile导致匹配失败，那么重试一次，且不检查profile
        check_profile = false;
        goto RETRY;
    }

    if (check_codec) {
        //如果是由于检查codec导致匹配失败，那么重试一次，且不检查codec
        check_codec = false;
        goto RETRY;
    }
}

void RtcConfigure::setPlayRtspInfo(const string &sdp){
    RtcSession session;
    video.direction = RtpDirection::inactive;
    audio.direction = RtpDirection::inactive;

    session.loadFrom(sdp);
    for (auto &m : session.media) {
        switch (m.type) {
            case TrackVideo : {
                video.direction = RtpDirection::sendonly;
                _rtsp_video_plan = std::make_shared<RtcCodecPlan>(m.plan[0]);
                video.preferred_codec.clear();
                video.preferred_codec.emplace_back(getCodecId(_rtsp_video_plan->codec));
                break;
            }
            case TrackAudio : {
                 audio.direction = RtpDirection::sendonly;
                _rtsp_audio_plan = std::make_shared<RtcCodecPlan>(m.plan[0]);
                audio.preferred_codec.clear();
                audio.preferred_codec.emplace_back(getCodecId(_rtsp_audio_plan->codec));
                break;
            }
            default: break;
        }
    }
}

static const string kProfile{"profile-level-id"};
static const string kMode{"packetization-mode"};

bool RtcConfigure::onCheckCodecProfile(const RtcCodecPlan &plan, CodecId codec) const {
    if (_rtsp_audio_plan && codec == getCodecId(_rtsp_audio_plan->codec)) {
        if (plan.sample_rate != _rtsp_audio_plan->sample_rate || plan.channel != _rtsp_audio_plan->channel) {
            //音频采样率和通道数必须相同
            return false;
        }
        return true;
    }
    if (_rtsp_video_plan && codec == CodecH264 && getCodecId(_rtsp_video_plan->codec) == CodecH264) {
        //h264时，profile-level-id
        if (strcasecmp(_rtsp_video_plan->fmtp[kProfile].data(), const_cast<RtcCodecPlan &>(plan).fmtp[kProfile].data())) {
            //profile-level-id 不匹配
            return false;
        }
        return true;
    }

    return true;
}

void RtcConfigure::onSelectPlan(RtcCodecPlan &plan, CodecId codec) const {
    if (_rtsp_video_plan && codec == CodecH264 && getCodecId(_rtsp_video_plan->codec) == CodecH264) {
        //h264时，设置packetization-mod为一致
        auto mode = _rtsp_video_plan->fmtp[kMode];
        plan.fmtp[kMode] = mode.empty() ? "0" : mode;
    }
}

} // namespace mediakit