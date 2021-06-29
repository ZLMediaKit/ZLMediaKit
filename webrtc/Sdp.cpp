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
using namespace mediakit;

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
    if (sscanf(str.data(), "%" SCNd8 "/%31[^ ] %127s", &id, direction_buf, buf) != 3) {
        if (sscanf(str.data(), "%" SCNd8 " %127s", &id, buf) != 2) {
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
        if (sscanf(str.data(), "%" SCNu8 " %31[^/]/%" SCNd32, &pt, buf, &sample_rate) != 3) {
            SDP_THROW();
        }
        if (getTrackType(getCodecId(buf)) == TrackAudio) {
            //未指定通道数时，且为音频时，那么通道数默认为1
            channel = 1;
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
        auto pos = item.find('=');
        if(pos == string::npos){
            fmtp.emplace(std::make_pair(item, ""));
        }  else {
            fmtp.emplace(std::make_pair(item.substr(0, pos), item.substr(pos + 1)));
        }
    }
    if (fmtp.empty()) {
        SDP_THROW();
    }
}

string SdpAttrFmtp::toString() const  {
    if (value.empty()) {
        value = to_string(pt);
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
    if (vec.size() >= 3) {
        type = std::move(vec[0]);
        CHECK(isFID() || isSIM());
        vec.erase(vec.begin());
        for (auto ssrc : vec) {
            ssrcs.emplace_back((uint32_t)atoll(ssrc.data()));
        }
    } else {
        SDP_THROW();
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
    char foundation_buf[40] = {0};
    char transport_buf[16] = {0};
    char address_buf[32] = {0};
    char type_buf[16] = {0};

    // https://datatracker.ietf.org/doc/html/rfc5245#section-15.1
    if (7 != sscanf(str.data(), "%32[^ ] %" SCNu32 " %15[^ ] %" SCNu32 " %31[^ ] %" SCNu16 " typ %15[^ ]",
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

void SdpAttrSimulcast::parse(const string &str) {
    //https://www.meetecho.com/blog/simulcast-janus-ssrc/
    //a=simulcast:send/recv q;h;f
    //a=simulcast:send/recv [rid=]q;h;f
    //a=simulcast: recv h;m;l
    //
    auto vec = split(str, " ");
    if (vec.size() != 2) {
        SDP_THROW();
    }
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

void RtcSession::loadFrom(const string &str, bool check) {
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
        SdpAttrSSRCGroup *ssrc_group_sim = nullptr;
        SdpAttrSSRCGroup *ssrc_group_fid = nullptr;
        for (auto &group : ssrc_groups) {
            if (group.isFID()) {
                ssrc_group_fid = &group;
            } else if (group.isSIM()) {
                ssrc_group_sim = &group;
            }
        }

        if (ssrc_group_fid) {
            //指定了ssrc-group:FID字段
            for (auto ssrc : ssrc_group_fid->ssrcs) {
                auto it = rtc_ssrc_map.find(ssrc);
                if (it == rtc_ssrc_map.end()) {
                    throw std::invalid_argument("a=ssrc-group:FID字段指定的ssrc未找到");
                }
                rtc_media.rtp_rtx_ssrc.emplace_back(it->second);
            }
            CHECK(rtc_media.rtp_rtx_ssrc.size() == 2);
        } else {
            auto simulcast = media.getItemClass<SdpAttrSimulcast>('a', "simulcast");
            if (simulcast.empty()) {
                //没有指定ssrc-group:FID字段,也不是simulcast，那么只有1个或0个ssrc
                if (rtc_ssrc_map.size() == 1) {
                    rtc_media.rtp_rtx_ssrc.emplace_back(rtc_ssrc_map.begin()->second);
                } else if (rtc_ssrc_map.size() > 1) {
                    throw std::invalid_argument("sdp中不存在a=ssrc-group:FID字段,但是ssrc却大于1个");
                }
            } else {
                //开启simulcast
                rtc_media.rtp_rids = simulcast.rids;
                //simulcast最少要求2种方案
                CHECK(rtc_media.rtp_rids.size() >= 2);
            }
        }

        if (ssrc_group_sim) {
            //指定了a=ssrc-group:SIM
            for (auto ssrc : ssrc_group_sim->ssrcs) {
                auto it = rtc_ssrc_map.find(ssrc);
                if (it == rtc_ssrc_map.end()) {
                    throw std::invalid_argument("a=ssrc-group:SIM字段指定的ssrc未找到");
                }
                rtc_media.rtp_ssrc_sim.emplace_back(it->second);
            }
        } else if (!rtc_media.rtp_rids.empty()) {
            //未指定a=ssrc-group:SIM，但是指定了a=simulcast,且可能指定了ssrc
            for (auto &attr : ssrc_attr) {
                rtc_media.rtp_ssrc_sim.emplace_back(rtc_ssrc_map[attr.ssrc]);
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
    if (check) {
        checkValid();
    }
}

std::shared_ptr<SdpItem> wrapSdpAttr(SdpItem::Ptr item){
    auto ret = std::make_shared<SdpAttr>();
    ret->detail = std::move(item);
    return ret;
}

static void toRtsp(vector <SdpItem::Ptr> &items) {
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
    checkValid();
    RtcSession copy = *this;
    copy.media.clear();
    for (auto &m : media) {
        switch (m.type) {
            case TrackAudio:
            case TrackVideo: {
                copy.media.emplace_back(m);
                copy.media.back().plan.resize(1);
                break;
            }
            default:
                continue;
        }
    }

    auto sdp = copy.toRtcSessionSdp();
    toRtsp(sdp->items);
    int i = 0;
    for (auto &m : sdp->medias) {
        toRtsp(m.items);
        m.items.push_back(wrapSdpAttr(std::make_shared<SdpCommon>("control", string("trackID=") + to_string(i++))));
    }
    return sdp->toString();
}

RtcSessionSdp::Ptr RtcSession::toRtcSessionSdp() const{
    RtcSessionSdp::Ptr ret = std::make_shared<RtcSessionSdp>();
    auto &sdp = *ret;
    sdp.items.emplace_back(std::make_shared<SdpString<'v'> >(to_string(version)));
    sdp.items.emplace_back(std::make_shared<SdpOrigin>(origin));
    sdp.items.emplace_back(std::make_shared<SdpString<'s'> >(session_name));
    if (!session_info.empty()) {
        sdp.items.emplace_back(std::make_shared<SdpString<'i'> >(session_info));
    }
    sdp.items.emplace_back(std::make_shared<SdpTime>(time));
    if(connection.empty()){
        sdp.items.emplace_back(std::make_shared<SdpConnection>(connection));
    }
    if (!bandwidth.empty()) {
        sdp.items.emplace_back(std::make_shared<SdpBandwidth>(bandwidth));
    }
    sdp.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrGroup>(group)));
    sdp.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrMsidSemantic>(msid_semantic)));
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
        sdp_media.items.emplace_back(std::make_shared<SdpConnection>(m.addr));
        if (!m.rtcp_addr.empty()) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrRtcp>(m.rtcp_addr)));
        }

        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrIceUfrag>(m.ice_ufrag)));
        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrIcePwd>(m.ice_pwd)));
        if (m.ice_trickle || m.ice_renomination) {
            auto attr = std::make_shared<SdpAttrIceOption>();
            attr->trickle = m.ice_trickle;
            attr->renomination = m.ice_renomination;
            sdp_media.items.emplace_back(wrapSdpAttr(attr));
        }
        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrFingerprint>(m.fingerprint)));
        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrSetup>(m.role)));
        sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrMid>(m.mid)));
        if (m.ice_lite) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpCommon>("ice-lite")));
        }
        for (auto &ext : m.extmap) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrExtmap>(ext)));
        }
        if (m.direction != RtpDirection::invalid) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<DirectionInterfaceImp>(m.direction)));
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
                //添加a=rtpmap
                sdp_media.items.emplace_back(wrapSdpAttr(std::move(rtp_map)));

                for (auto &fb :  p.rtcp_fb) {
                    auto rtcp_fb = std::make_shared<SdpAttrRtcpFb>();
                    rtcp_fb->pt = p.pt;
                    rtcp_fb->rtcp_type = fb;
                    //添加a=rtcp-fb
                    sdp_media.items.emplace_back(wrapSdpAttr(std::move(rtcp_fb)));
                }

                if (!p.fmtp.empty()) {
                    auto fmtp = std::make_shared<SdpAttrFmtp>();
                    fmtp->pt = p.pt;
                    fmtp->fmtp = p.fmtp;
                    //添加a=fmtp
                    sdp_media.items.emplace_back(wrapSdpAttr(std::move(fmtp)));
                }
            }

            {
                //添加a=msid字段
                if (!m.rtp_rtx_ssrc.empty()) {
                    auto msid = std::make_shared<SdpAttrMsid>();
                    if (!m.rtp_rtx_ssrc[0].msid.empty()) {
                        msid->parse(m.rtp_rtx_ssrc[0].msid);
                    } else {
                        msid->parse("mslabel label");
                    }
                    sdp_media.items.emplace_back(wrapSdpAttr(std::move(msid)));
                }
            }

            static auto addSSRCItem = [](const RtcSSRC &rtp_ssrc, vector<SdpItem::Ptr> &items) {
                CHECK(!rtp_ssrc.empty());
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

            {
                auto group = std::make_shared<SdpAttrSSRCGroup>();
                for (auto &ssrc : m.rtp_rtx_ssrc) {
                    //添加a=ssrc字段
                    addSSRCItem(ssrc, sdp_media.items);
                    group->ssrcs.emplace_back(ssrc.ssrc);
                }
                if (group->ssrcs.size() >= 2) {
                    group->type = "FID";
                    //生成a=ssrc-group:FID字段
                    sdp_media.items.emplace_back(wrapSdpAttr(std::move(group)));
                }
            }

            {
                if (m.rtp_ssrc_sim.size() >= 2) {
                    //simulcast 要求 2~3路
                    auto group = std::make_shared<SdpAttrSSRCGroup>();
                    for (auto &ssrc :  m.rtp_ssrc_sim) {
                        //添加simulcast的ssrc
                        addSSRCItem(ssrc, sdp_media.items);
                        group->ssrcs.emplace_back(ssrc.ssrc);
                    }
                    //添加a=ssrc-group:SIM字段
                    group->type = "SIM";
                    sdp_media.items.emplace_back(wrapSdpAttr(std::move(group)));
                }

                if (m.rtp_rids.size() >= 2) {
                    auto simulcast = std::make_shared<SdpAttrSimulcast>();
                    simulcast->direction = "recv";
                    simulcast->rids = m.rtp_rids;
                    sdp_media.items.emplace_back(wrapSdpAttr(std::move(simulcast)));

                    for (auto &rid : m.rtp_rids) {
                        auto attr_rid = std::make_shared<SdpAttrRid>();
                        attr_rid->rid = rid;
                        attr_rid->direction = "recv";
                        sdp_media.items.emplace_back(wrapSdpAttr(std::move(attr_rid)));
                    }
                }
            }

        } else {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrSctpMap>(m.sctpmap)));
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpCommon>("sctp-port", to_string(m.sctp_port))));
        }

        for (auto &cand : m.candidate) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrCandidate>(cand)));
        }
    }
    return ret;
}

string RtcSession::toString() const{
    checkValid();
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
    if (rtp_rtx_ssrc.size() > 1) {
        return rtp_rtx_ssrc[1].ssrc;
    }
    return 0;
}

void RtcMedia::checkValid() const{
    CHECK(type != TrackInvalid);
    CHECK(!mid.empty());
    CHECK(!proto.empty());
    CHECK(direction != RtpDirection::invalid || type == TrackApplication);
    CHECK(!plan.empty() || type == TrackApplication );
}

void RtcMedia::checkValidSSRC() const {
    bool send_rtp = (direction == RtpDirection::sendonly || direction == RtpDirection::sendrecv);
    if (rtp_rids.empty() && rtp_ssrc_sim.empty()) {
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
    CHECK(group.mids.size() <= media.size());
    for (auto &item : media) {
        item.checkValid();
    }
}

void RtcSession::checkValidSSRC() const{
    for (auto &item : media) {
        item.checkValidSSRC();
    }
}

const RtcMedia *RtcSession::getMedia(TrackType type) const{
    for(auto &m : media){
        if(m.type == type){
            return &m;
        }
    }
    return nullptr;
}

bool RtcSession::haveSSRC() const {
    for (auto &m : media) {
        if (!m.rtp_rtx_ssrc.empty()) {
            return true;
        }
    }
    return false;
}

bool RtcSession::supportRtcpFb(const string &name, TrackType type) const {
    auto media = getMedia(type);
    if (!media) {
        return false;
    }
    auto &ref = media->plan[0].rtcp_fb;
    return ref.find(name) != ref.end();
}

string const SdpConst::kTWCCRtcpFb = "transport-cc";
string const SdpConst::kRembRtcpFb = "goog-remb";

void RtcConfigure::RtcTrackConfigure::enableTWCC(bool enable){
    if (!enable) {
        rtcp_fb.erase(SdpConst::kTWCCRtcpFb);
        extmap.erase(RtpExtType::transport_cc);
    } else {
        rtcp_fb.emplace(SdpConst::kTWCCRtcpFb);
        extmap.emplace(RtpExtType::transport_cc);
    }
}

void RtcConfigure::RtcTrackConfigure::enableREMB(bool enable){
    if (!enable) {
        rtcp_fb.erase(SdpConst::kRembRtcpFb);
        extmap.erase(RtpExtType::abs_send_time);
    } else {
        rtcp_fb.emplace(SdpConst::kRembRtcpFb);
        extmap.emplace(RtpExtType::abs_send_time);
    }
}

void RtcConfigure::RtcTrackConfigure::setDefaultSetting(TrackType type){
    enable = true;
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
            preferred_codec = {CodecAAC, CodecG711U, CodecG711A, CodecOpus};
            rtcp_fb = {SdpConst::kTWCCRtcpFb, SdpConst::kRembRtcpFb};
            extmap = {
                    RtpExtType::ssrc_audio_level,
                    RtpExtType::csrc_audio_level,
                    RtpExtType::abs_send_time,
                    RtpExtType::transport_cc,
                    //rtx重传rtp时，忽略sdes_mid类型的rtp ext,实测发现Firefox在接收rtx时，如果存在sdes_mid的ext,将导致无法播放
                    //RtpExtType::sdes_mid,
                    RtpExtType::sdes_rtp_stream_id,
                    RtpExtType::sdes_repaired_rtp_stream_id
            };
            break;
        }
        case TrackVideo: {
            //此处调整偏好的编码格式优先级
            preferred_codec = {CodecH264, CodecH265, CodecAV1};
            rtcp_fb = {SdpConst::kTWCCRtcpFb, SdpConst::kRembRtcpFb, "nack", "ccm fir", "nack pli"};
            extmap = {
                    RtpExtType::abs_send_time,
                    RtpExtType::transport_cc,
                    //rtx重传rtp时，忽略sdes_mid类型的rtp ext,实测发现Firefox在接收rtx时，如果存在sdes_mid的ext,将导致无法播放
                    //RtpExtType::sdes_mid,
                    RtpExtType::sdes_rtp_stream_id,
                    RtpExtType::sdes_repaired_rtp_stream_id,
                    RtpExtType::video_timing,
                    RtpExtType::color_space,
                    RtpExtType::video_content_type,
                    RtpExtType::playout_delay,
                    RtpExtType::video_orientation,
                    RtpExtType::toffset,
                    RtpExtType::framemarking
            };
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
                                     RtpDirection direction,
                                     const SdpAttrFingerprint &fingerprint) {
    video.setDefaultSetting(TrackVideo);
    audio.setDefaultSetting(TrackAudio);
    application.setDefaultSetting(TrackApplication);

    video.ice_ufrag = audio.ice_ufrag = application.ice_ufrag = ice_ufrag;
    video.ice_pwd = audio.ice_pwd = application.ice_pwd = ice_pwd;
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
        case TrackApplication: {
            application.enableTWCC(enable);
            break;
        }
        default: {
            audio.enableTWCC(enable);
            video.enableTWCC(enable);
            application.enableTWCC(enable);
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
        case TrackApplication: {
            application.enableREMB(enable);
            break;
        }
        default: {
            audio.enableREMB(enable);
            video.enableREMB(enable);
            application.enableREMB(enable);
            break;
        }
    }
}

shared_ptr<RtcSession> RtcConfigure::createAnswer(const RtcSession &offer){
    shared_ptr<RtcSession> ret = std::make_shared<RtcSession>();
    ret->version = offer.version;
    //todo 此处设置会话id与会话地址，貌似没什么作用
    ret->origin = offer.origin;
    ret->session_name = offer.session_name;
    ret->msid_semantic = offer.msid_semantic;
    matchMedia(ret, TrackAudio, offer.media, audio);
    matchMedia(ret, TrackVideo, offer.media, video);
    matchMedia(ret, TrackApplication, offer.media, application);
    if (ret->media.empty()) {
        throw std::invalid_argument("生成的answer sdp中媒体个数为0");
    }

    //设置音视频端口复用
    if (!offer.group.mids.empty()) {
        for (auto &m : ret->media) {
            ret->group.mids.emplace_back(m.mid);
        }
    }
    return ret;
}

void RtcConfigure::matchMedia(shared_ptr<RtcSession> &ret, TrackType type, const vector<RtcMedia> &medias, const RtcTrackConfigure &configure){
    if (!configure.enable) {
        return;
    }
    bool check_profile = true;
    bool check_codec = true;

RETRY:

    for (auto &codec : configure.preferred_codec) {
        for (auto &offer_media : medias) {
            if (offer_media.type != type) {
                continue;
            }
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
            //todo(此处设置rtp端口，貌似没什么作用)
            answer_media.port = offer_media.port;
            //todo(此处设置rtp的ip地址，貌似没什么作用)
            answer_media.addr = offer_media.addr;
            //todo(此处设置rtcp地址，貌似没什么作用)
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
            answer_media.rtp_rids = offer_media.rtp_rids;
            answer_media.rtp_ssrc_sim = offer_media.rtp_ssrc_sim;
            switch (offer_media.role) {
                case DtlsRole::actpass :
                case DtlsRole::active : {
                    answer_media.role = DtlsRole::passive;
                    break;
                }
                case DtlsRole::passive : {
                    answer_media.role = DtlsRole::active;
                    break;
                }
                default: continue;
            }

            switch (offer_media.direction) {
                case RtpDirection::sendonly : {
                    if (configure.direction != RtpDirection::recvonly &&
                        configure.direction != RtpDirection::sendrecv) {
                        //我们不支持接收
                        answer_media.direction = RtpDirection::inactive;
                        break;
                    }
                    answer_media.direction = RtpDirection::recvonly;
                    break;
                }
                case RtpDirection::recvonly : {
                    if (configure.direction != RtpDirection::sendonly &&
                        configure.direction != RtpDirection::sendrecv) {
                        //我们不支持发送
                        answer_media.direction = RtpDirection::inactive;
                        break;
                    }
                    answer_media.direction = RtpDirection::sendonly;
                    break;
                }
                case RtpDirection::sendrecv : {
                    //对方支持发送接收，那么最终能力根据配置来决定
                    answer_media.direction = configure.direction;
                    break;
                }
                default: continue;
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
                if (configure.extmap.find(RtpExt::getExtType(ext.ext)) != configure.extmap.end()) {
                    answer_media.extmap.emplace_back(ext);
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
    session.loadFrom(sdp, false);
    for (auto &m : session.media) {
        switch (m.type) {
            case TrackVideo : {
                _rtsp_video_plan = std::make_shared<RtcCodecPlan>(m.plan[0]);
                video.preferred_codec.clear();
                video.preferred_codec.emplace_back(getCodecId(_rtsp_video_plan->codec));
                break;
            }
            case TrackAudio : {
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

bool RtcConfigure::onCheckCodecProfile(const RtcCodecPlan &plan, CodecId codec){
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

void RtcConfigure::onSelectPlan(RtcCodecPlan &plan, CodecId codec){
    if (_rtsp_video_plan && codec == CodecH264 && getCodecId(_rtsp_video_plan->codec) == CodecH264) {
        //h264时，设置packetization-mod为一致
        auto mode = _rtsp_video_plan->fmtp[kMode];
        plan.fmtp[kMode] = mode.empty() ? "0" : mode;
    }
}
