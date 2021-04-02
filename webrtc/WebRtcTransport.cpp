#include "WebRtcTransport.h"
#include <iostream>
#include "Rtcp/Rtcp.h"

WebRtcTransport::WebRtcTransport(const EventPoller::Ptr &poller) {
    dtls_transport_ = std::make_shared<RTC::DtlsTransport>(poller, this);
    ice_server_ = std::make_shared<RTC::IceServer>(this, makeRandStr(4), makeRandStr(24));
}

void WebRtcTransport::onDestory(){
    dtls_transport_ = nullptr;
    ice_server_ = nullptr;
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebRtcTransport::OnIceServerSendStunPacket(const RTC::IceServer *iceServer, const RTC::StunPacket *packet, RTC::TransportTuple *tuple) {
    onWrite((char *)packet->GetData(), packet->GetSize(), (struct sockaddr_in *)tuple);
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
        dtls_transport_->Run(RTC::DtlsTransport::Role::SERVER);
    } else {
        dtls_transport_->Run(RTC::DtlsTransport::Role::CLIENT);
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
    srtp_session_ = std::make_shared<RTC::SrtpSession>(RTC::SrtpSession::Type::OUTBOUND, srtpCryptoSuite, srtpLocalKey, srtpLocalKeyLen);
    srtp_session_recv_ = std::make_shared<RTC::SrtpSession>(RTC::SrtpSession::Type::OUTBOUND, srtpCryptoSuite, srtpRemoteKey, srtpRemoteKeyLen);
    onDtlsConnected();
}

void WebRtcTransport::OnDtlsTransportSendData(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) {
    onWrite((char *)data, len);
}

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebRtcTransport::onWrite(const char *buf, size_t len){
    auto tuple = ice_server_->GetSelectedTuple();
    assert(tuple);
    onWrite(buf, len, (struct sockaddr_in *)tuple);
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

std::string WebRtcTransport::getAnswerSdp(const string &offer){
//    InfoL << offer;
    _offer_sdp = std::make_shared<RtcSession>();
    _offer_sdp->loadFrom(offer);

    SdpAttrFingerprint fingerprint;
    fingerprint.algorithm = _offer_sdp->media[0].fingerprint.algorithm;
    fingerprint.hash = getFingerprint(fingerprint.algorithm, dtls_transport_);

    RtcConfigure configure;
    configure.setDefaultSetting(ice_server_->GetUsernameFragment(), ice_server_->GetPassword(), RtpDirection::recvonly, fingerprint);

    SdpAttrCandidate candidate;
    candidate.foundation = "udpcandidate";
    candidate.component = 1;
    candidate.transport = "udp";
    candidate.priority = getSSRC();
    candidate.address = getIP();
    candidate.port = getPort();
    candidate.type = "host";
    configure.addCandidate(candidate);

    _answer_sdp = configure.createAnswer(*_offer_sdp);

    //设置远端dtls签名
    RTC::DtlsTransport::Fingerprint remote_fingerprint;
    remote_fingerprint.algorithm = RTC::DtlsTransport::GetFingerprintAlgorithm(_offer_sdp->media[0].fingerprint.algorithm);
    remote_fingerprint.value = _offer_sdp->media[0].fingerprint.hash;
    dtls_transport_->SetRemoteFingerprint(remote_fingerprint);

    if (!_offer_sdp->group.mids.empty()) {
        for (auto &m : _answer_sdp->media) {
            _answer_sdp->group.mids.emplace_back(m.mid);
        }
    } else {
        throw std::invalid_argument("支持group BUNDLE模式");
    }

    auto str = _answer_sdp->toString();
    InfoL << "\r\n" << str;
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

void WebRtcTransport::OnInputDataPacket(char *buf, size_t len, RTC::TransportTuple *tuple) {
    if (RTC::StunPacket::IsStun((const uint8_t *) buf, len)) {
        RTC::StunPacket *packet = RTC::StunPacket::Parse((const uint8_t *) buf, len);
        if (packet == nullptr) {
            WarnL << "parse stun error" << std::endl;
            return;
        }
        ice_server_->ProcessStunPacket(packet, tuple);
        return;
    }
    if (is_dtls(buf)) {
        dtls_transport_->ProcessDtlsData((uint8_t *) buf, len);
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

void WebRtcTransport::WritRtpPacket(char *buf, size_t len) {
    const uint8_t *p = (uint8_t *) buf;
    bool ret = false;
    if (srtp_session_) {
        ret = srtp_session_->EncryptRtp(&p, &len);
    }
    if (ret) {
        onWrite((char *) p, len);
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
        OnInputDataPacket(buf->data(), buf->size(), addr);
    });
}

void WebRtcTransportImp::onDestory() {
    WebRtcTransport::onDestory();
}

void WebRtcTransportImp::attach(const RtspMediaSource::Ptr &src) {
    assert(src);
    _src = src;
}

void WebRtcTransportImp::onDtlsConnected() {
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
                strongSelf->WritRtpPacket(rtp->data() + RtpPacket::kRtpTcpHeaderSize,
                                          rtp->size() - RtpPacket::kRtpTcpHeaderSize);
            }
        });
    });
}

void WebRtcTransportImp::onWrite(const char *buf, size_t len, struct sockaddr_in *dst) {
    auto ptr = BufferRaw::create();
    ptr->assign(buf, len);
    _socket->send(ptr, (struct sockaddr *)(dst), sizeof(struct sockaddr));
}

uint32_t WebRtcTransportImp::getSSRC() const {
    return _src->getSsrc(TrackVideo);
}

int WebRtcTransportImp::getPayloadType() const{
    auto sdp = SdpParser(_src->getSdp());
    auto track = sdp.getTrack(TrackVideo);
    assert(track);
    return track ? track->_pt : 0;
}

uint16_t WebRtcTransportImp::getPort() const {
    //todo udp端口号应该与外网映射端口相同
    return _socket->get_local_port();
}

std::string WebRtcTransportImp::getIP() const {
    //todo 替换为外网ip
    return SockUtil::get_local_ip();
}

///////////////////////////////////////////////////////////////////



