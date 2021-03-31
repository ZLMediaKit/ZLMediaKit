//
// Created by xzl on 2021/3/27.
//

#include "Sdp.h"
#include "Common/Parser.h"
#include <inttypes.h>

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
                                     RtpDirection direction,
                                     const SdpAttrFingerprint &fingerprint) {
    video.setDefaultSetting(TrackVideo);
    audio.setDefaultSetting(TrackAudio);
    application.setDefaultSetting(TrackApplication);

    video.ice_ufrag = audio.ice_ufrag = application.ice_ufrag = ice_ufrag;
    video.ice_pwd = audio.ice_pwd = application.ice_pwd = ice_ufrag;
    video.direction = audio.direction = application.direction = direction;
    video.fingerprint = audio.fingerprint = application.fingerprint = fingerprint;
    application.enable = false;
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

shared_ptr<RtcSession> RtcConfigure::createOffer(){
    shared_ptr<RtcSession> ret = std::make_shared<RtcSession>();
    ret->version = 0;
    ret->origin.parse("- 0 0 IN IP4 0.0.0.0");
    ret->session_name = "zlmediakit_webrtc_session";
    ret->session_info = "zlmediakit_webrtc_session";
    ret->connection.parse("IN IP4 0.0.0.0");
    ret->msid_semantic.parse("WMS *");
    return nullptr;
}

shared_ptr<RtcSession> RtcConfigure::createAnswer(const RtcSession &offer){
    shared_ptr<RtcSession> ret = std::make_shared<RtcSession>();
    ret->version = offer.version;
    ret->origin.parse("- 0 0 IN IP4 0.0.0.0");
    ret->session_name = "zlmediakit_webrtc_session";
    ret->session_info = "zlmediakit_webrtc_session";
    ret->connection.parse("IN IP4 0.0.0.0");
    ret->msid_semantic.parse("WMS *");
    matchMedia(ret, TrackVideo, offer.media, video);
    matchMedia(ret, TrackAudio, offer.media, audio);
    matchMedia(ret, TrackApplication, offer.media, application);
    return ret;
}

void RtcConfigure::matchMedia(shared_ptr<RtcSession> &ret, TrackType type, const vector<RtcMedia> &medias, const RtcTrackConfigure &configure){
    if (!configure.enable) {
        return;
    }
    for (auto &codec : configure.preferred_codec) {
        for (auto &media : medias) {
            if (media.type != type) {
                continue;
            }
            if (media.ice_lite && configure.ice_lite) {
                WarnL << "offer sdp开启了ice_lite模式，但是answer sdp配置为不支持";
                continue;
            }

            const RtcCodecPlan *plan_ptr = nullptr;
            for (auto &plan : media.plan) {
                if (getCodecId(plan.codec) != codec) {
                    continue;
                }
                //命中偏好的编码格式
                if (!onMatchCodecPlan(plan, codec)) {
                    continue;
                }
                plan_ptr = &plan;
            }
            if (!plan_ptr) {
                continue;
            }
            RtcMedia answer_media;
            answer_media.type = media.type;
            answer_media.mid = media.type;
            answer_media.proto = media.proto;
            answer_media.rtcp_mux = media.rtcp_mux && configure.rtcp_mux;
            answer_media.rtcp_rsize = media.rtcp_rsize && configure.rtcp_rsize;
            answer_media.ice_trickle = media.ice_trickle && configure.ice_trickle;
            answer_media.ice_renomination = media.ice_renomination && configure.ice_renomination;
            switch (media.role) {
                case DtlsRole::actpass : {
                    answer_media.role = DtlsRole::passive;
                    break;
                }
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

            switch (media.direction) {
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
            answer_media.plan.emplace_back(*plan_ptr);
            ret->media.emplace_back(answer_media);
        }
    }

}
