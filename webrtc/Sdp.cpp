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
#include <inttypes.h>
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
    if (vec.size() == 3) {
        if (vec[0] != "FID") {
            SDP_THROW();
        }
        type = std::move(vec[0]);
        u.fid.rtp_ssrc = atoll(vec[1].data()) & 0xFFFFFFFF;
        u.fid.rtx_ssrc = atoll(vec[2].data()) & 0xFFFFFFFF;
    } else if (vec.size() == 4) {
        if (vec[0] != "SIM") {
            SDP_THROW();
        }
        type = std::move(vec[0]);
        u.sim.rtp_ssrc_low = atoll(vec[1].data()) & 0xFFFFFFFF;
        u.sim.rtp_ssrc_mid = atoll(vec[2].data()) & 0xFFFFFFFF;
        u.sim.rtp_ssrc_high = atoll(vec[3].data()) & 0xFFFFFFFF;
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
        {
            rtc_media.extmap.clear();
            auto arr = media.getAllItem<SdpAttrExtmap>('a', "extmap");
            for (auto &ext : arr) {
                rtc_media.extmap.emplace(ext.id, ext);
            }
        }
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
            if (rtc_ssrc_map.size() == 1) {
                ssrc_rtp = rtc_ssrc_map.begin()->second.ssrc;
            }
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

    copy.session_name = "zlmediakit rtsp stream from webrtc";
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
        for (auto &pr : m.extmap) {
            sdp_media.items.emplace_back(wrapSdpAttr(std::make_shared<SdpAttrExtmap>(pr.second)));
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
                    fmtp->fmtp = p.fmtp;
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
        //todo 此处不确定
//        CHECK(!rtx_ssrc.empty() || !send_rtp);
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

string const SdpConst::kTWCCRtcpFb = "transport-cc";
string const SdpConst::kRembRtcpFb = "goog-remb";

void RtcConfigure::RtcTrackConfigure::enableTWCC(bool enable){
    if (!enable) {
        rtcp_fb.erase(SdpConst::kTWCCRtcpFb);
        extmap.erase(RtpExtType::abs_send_time);
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
        extmap.emplace(RtpExtType::transport_cc);
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
                    RtpExtType::sdes_mid,
                    RtpExtType::sdes_rtp_stream_id,
                    RtpExtType::sdes_repaired_rtp_stream_id
            };
            break;
        }
        case TrackVideo: {
            //此处调整偏好的编码格式优先级
            preferred_codec = {CodecH264, CodecH265};
            rtcp_fb = {SdpConst::kTWCCRtcpFb, SdpConst::kRembRtcpFb, "nack", "ccm fir", "nack pli"};
            extmap = {
                    RtpExtType::abs_send_time,
                    RtpExtType::transport_cc,
                    RtpExtType::sdes_mid,
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
            const RtcCodecPlan *offer_plan_ptr = nullptr;
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
                offer_plan_ptr = &plan;
                break;
            }
            if (!offer_plan_ptr) {
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
                        continue;
                    }
                    answer_media.direction = RtpDirection::recvonly;
                    break;
                }
                case RtpDirection::recvonly : {
                    if (configure.direction != RtpDirection::sendonly &&
                        configure.direction != RtpDirection::sendrecv) {
                        //我们不支持发送
                        continue;
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
            answer_media.plan.emplace_back(*offer_plan_ptr);
            onSelectPlan(answer_media.plan.back(), codec);

            //添加rtx,red,ulpfec plan
            if (configure.support_red || configure.support_rtx || configure.support_ulpfec) {
                for (auto &plan : offer_media.plan) {
                    if (!strcasecmp(plan.codec.data(), "rtx")) {
                        if (configure.support_rtx && atoi(plan.getFmtp("apt").data()) == offer_plan_ptr->pt) {
                            answer_media.plan.emplace_back(plan);
                        }
                        continue;
                    }
                    if (!strcasecmp(plan.codec.data(), "red")) {
                        if (configure.support_red) {
                            answer_media.plan.emplace_back(plan);
                        }
                        continue;
                    }
                    if (!strcasecmp(plan.codec.data(), "ulpfec")) {
                        if (configure.support_ulpfec) {
                            answer_media.plan.emplace_back(plan);
                        }
                        continue;
                    }
                }
            }

            //对方和我方都支持的扩展，那么我们才支持
            for (auto &pr : offer_media.extmap) {
                if (configure.extmap.find(RtpExt::getExtType(pr.second.ext)) != configure.extmap.end()) {
                    answer_media.extmap.emplace(pr);
                }
            }

            auto &rtcp_fb_ref = answer_media.plan[0].rtcp_fb;
            rtcp_fb_ref.clear();
            //对方和我方都支持的rtcpfb，那么我们才支持
            for (auto &fp : offer_plan_ptr->rtcp_fb) {
                if (configure.rtcp_fb.find(fp) != configure.rtcp_fb.end()) {
                    //对方该rtcp被我们支持
                    rtcp_fb_ref.emplace(fp);
                }
            }
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
