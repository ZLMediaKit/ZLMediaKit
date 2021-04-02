#include "WebRtcTransport.h"
#include <iostream>
#include "Rtcp/Rtcp.h"

WebRtcTransport::WebRtcTransport(const EventPoller::Ptr &poller) {
    _dtls_transport = std::make_shared<RTC::DtlsTransport>(poller, this);
    _ice_server = std::make_shared<RTC::IceServer>(this, makeRandStr(4), makeRandStr(24));
}

void WebRtcTransport::onDestory(){
    _dtls_transport = nullptr;
    _ice_server = nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebRtcTransport::OnIceServerSendStunPacket(const RTC::IceServer *iceServer, const RTC::StunPacket *packet, RTC::TransportTuple *tuple) {
    onSendSockData((char *) packet->GetData(), packet->GetSize(), (struct sockaddr_in *) tuple);
}

void WebRtcTransport::OnIceServerSelectedTuple(const RTC::IceServer *iceServer, RTC::TransportTuple *tuple) {
    InfoL;
}

void WebRtcTransport::OnIceServerConnected(const RTC::IceServer *iceServer) {
    InfoL;
}

void WebRtcTransport::OnIceServerCompleted(const RTC::IceServer *iceServer) {
    InfoL;
    if (_answer_sdp->media[0].role == DtlsRole::passive) {
        _dtls_transport->Run(RTC::DtlsTransport::Role::SERVER);
    } else {
        _dtls_transport->Run(RTC::DtlsTransport::Role::CLIENT);
    }
}

void WebRtcTransport::OnIceServerDisconnected(const RTC::IceServer *iceServer) {
    InfoL;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebRtcTransport::OnDtlsTransportConnected(
        const RTC::DtlsTransport *dtlsTransport,
        RTC::SrtpSession::CryptoSuite srtpCryptoSuite,
        uint8_t *srtpLocalKey,
        size_t srtpLocalKeyLen,
        uint8_t *srtpRemoteKey,
        size_t srtpRemoteKeyLen,
        std::string &remoteCert) {
    InfoL;
    _srtp_session_send = std::make_shared<RTC::SrtpSession>(RTC::SrtpSession::Type::OUTBOUND, srtpCryptoSuite, srtpLocalKey, srtpLocalKeyLen);
    _srtp_session_recv = std::make_shared<RTC::SrtpSession>(RTC::SrtpSession::Type::OUTBOUND, srtpCryptoSuite, srtpRemoteKey, srtpRemoteKeyLen);
    onStartWebRTC();
}

void WebRtcTransport::OnDtlsTransportSendData(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) {
    onSendSockData((char *)data, len);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebRtcTransport::onSendSockData(const char *buf, size_t len){
    auto tuple = _ice_server->GetSelectedTuple();
    assert(tuple);
    onSendSockData(buf, len, (struct sockaddr_in *) tuple);
}

string getFingerprint(const string &algorithm_str, const std::shared_ptr<RTC::DtlsTransport> &transport){
    auto algorithm = RTC::DtlsTransport::GetFingerprintAlgorithm(algorithm_str);
    for (auto &finger_prints : transport->GetLocalFingerprints()) {
        if (finger_prints.algorithm == algorithm) {
            return finger_prints.value;
        }
    }
    throw std::invalid_argument(StrPrinter << "不支持的加密算法:" << algorithm_str);
}

void WebRtcTransport::setRemoteDtlsFingerprint(const RtcSession &remote){
    //设置远端dtls签名
    RTC::DtlsTransport::Fingerprint remote_fingerprint;
    remote_fingerprint.algorithm = RTC::DtlsTransport::GetFingerprintAlgorithm(_offer_sdp->media[0].fingerprint.algorithm);
    remote_fingerprint.value = _offer_sdp->media[0].fingerprint.hash;
    _dtls_transport->SetRemoteFingerprint(remote_fingerprint);
}

void WebRtcTransport::onCheckSdp(SdpType type, const RtcSession &sdp) const{
    for (auto &m : sdp.media) {
        if (m.type != TrackApplication && !m.rtcp_mux) {
            throw std::invalid_argument("只支持rtcp-mux模式");
        }
    }
    if (sdp.group.mids.empty()) {
        throw std::invalid_argument("只支持group BUNDLE模式");
    }
}

std::string WebRtcTransport::getAnswerSdp(const string &offer){
    //// 解析offer sdp ////
    _offer_sdp = std::make_shared<RtcSession>();
    _offer_sdp->loadFrom(offer);
    onCheckSdp(SdpType::offer, *_offer_sdp);
    setRemoteDtlsFingerprint(*_offer_sdp);

    //// sdp 配置 ////
    SdpAttrFingerprint fingerprint;
    fingerprint.algorithm = _offer_sdp->media[0].fingerprint.algorithm;
    fingerprint.hash = getFingerprint(fingerprint.algorithm, _dtls_transport);
    RtcConfigure configure;
    configure.setDefaultSetting(_ice_server->GetUsernameFragment(), _ice_server->GetPassword(), RtpDirection::recvonly, fingerprint);
    configure.addCandidate(*getIceCandidate());
    onRtcConfigure(configure);

    //// 生成answer sdp ////
    _answer_sdp = configure.createAnswer(*_offer_sdp);
    onCheckSdp(SdpType::answer, *_answer_sdp);

    auto str = _answer_sdp->toString();
    TraceL << "\r\n" << str;
    return str;
}

bool is_dtls(char *buf) {
    return ((*buf > 19) && (*buf < 64));
}

bool is_rtp(char *buf) {
    RtpHeader *header = (RtpHeader *) buf;
    return ((header->pt < 64) || (header->pt >= 96));
}

bool is_rtcp(char *buf) {
    RtpHeader *header = (RtpHeader *) buf;
    return ((header->pt >= 64) && (header->pt < 96));
}

void WebRtcTransport::inputSockData(char *buf, size_t len, RTC::TransportTuple *tuple) {
    if (RTC::StunPacket::IsStun((const uint8_t *) buf, len)) {
        RTC::StunPacket *packet = RTC::StunPacket::Parse((const uint8_t *) buf, len);
        if (packet == nullptr) {
            WarnL << "parse stun error" << std::endl;
            return;
        }
        _ice_server->ProcessStunPacket(packet, tuple);
        return;
    }
    if (is_dtls(buf)) {
        _dtls_transport->ProcessDtlsData((uint8_t *) buf, len);
        return;
    }
    if (is_rtp(buf)) {
        RtpHeader *header = (RtpHeader *) buf;
        return;
    }
    if (is_rtcp(buf)) {
        RtcpHeader *header = (RtcpHeader *) buf;
        return;
    }
}

void WebRtcTransport::sendRtpPacket(char *buf, size_t len) {
    const uint8_t *p = (uint8_t *) buf;
    bool ret = false;
    if (_srtp_session_send) {
        ret = _srtp_session_send->EncryptRtp(&p, &len);
    }
    if (ret) {
        onSendSockData((char *) p, len);
    }
}

///////////////////////////////////////////////////////////////////////////////////
WebRtcTransportImp::Ptr WebRtcTransportImp::create(const EventPoller::Ptr &poller){
    WebRtcTransportImp::Ptr ret(new WebRtcTransportImp(poller), [](WebRtcTransportImp *ptr){
        ptr->onDestory();
       delete ptr;
    });
    return ret;
}

WebRtcTransportImp::WebRtcTransportImp(const EventPoller::Ptr &poller) : WebRtcTransport(poller) {
    _socket = Socket::createSocket(poller, false);
    //随机端口，绑定全部网卡
    _socket->bindUdpSock(0);
    _socket->setOnRead([this](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) mutable {
        inputSockData(buf->data(), buf->size(), addr);
    });
}

void WebRtcTransportImp::onDestory() {
    WebRtcTransport::onDestory();
}

void WebRtcTransportImp::attach(const RtspMediaSource::Ptr &src) {
    assert(src);
    _src = src;
}

void WebRtcTransportImp::onStartWebRTC() {
    _reader = _src->getRing()->attach(_socket->getPoller(), true);
    weak_ptr<WebRtcTransportImp> weak_self = shared_from_this();
    _reader->setReadCB([weak_self](const RtspMediaSource::RingDataType &pkt){
        auto strongSelf = weak_self.lock();
        if (!strongSelf) {
            return;
        }
        pkt->for_each([&](const RtpPacket::Ptr &rtp) {
            if(rtp->type == TrackVideo) {
                //目前只支持视频
                strongSelf->sendRtpPacket(rtp->data() + RtpPacket::kRtpTcpHeaderSize,
                                          rtp->size() - RtpPacket::kRtpTcpHeaderSize);
            }
        });
    });
}

void WebRtcTransportImp::onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst) {
    auto ptr = BufferRaw::create();
    ptr->assign(buf, len);
    _socket->send(ptr, (struct sockaddr *)(dst), sizeof(struct sockaddr));
}

uint32_t WebRtcTransportImp::getSSRC() const {
    return _src->getSsrc(TrackVideo);
}

uint16_t WebRtcTransportImp::getPort() const {
    //todo udp端口号应该与外网映射端口相同
    return _socket->get_local_port();
}

std::string WebRtcTransportImp::getIP() const {
    //todo 替换为外网ip
    return SockUtil::get_local_ip();
}

SdpAttrCandidate::Ptr WebRtcTransportImp::getIceCandidate() const{
    auto candidate = std::make_shared<SdpAttrCandidate>();
    candidate->foundation = "udpcandidate";
    candidate->component = 1;
    candidate->transport = "udp";
    candidate->priority = 100;
    candidate->address = getIP();
    candidate->port = getPort();
    candidate->type = "host";
    return candidate;
}

///////////////////////////////////////////////////////////////////



