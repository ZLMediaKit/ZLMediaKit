#include "webrtc_transport.h"
#include <iostream>
#include "Rtcp/Rtcp.h"

WebRtcTransport::WebRtcTransport() {
    static onceToken token([](){
        Utils::Crypto::ClassInit();
        RTC::DtlsTransport::ClassInit();
        RTC::DepLibSRTP::ClassInit();
        RTC::SrtpSession::ClassInit();
    });

    ice_server_ = std::make_shared<IceServer>(Utils::Crypto::GetRandomString(4), Utils::Crypto::GetRandomString(24));
    ice_server_->SetIceServerCompletedCB([this]() {
        this->OnIceServerCompleted();
    });
    ice_server_->SetSendCB([this](char *buf, size_t len, struct sockaddr_in *remote_address) {
        this->WritePacket(buf, len, remote_address);
    });

    // todo dtls服务器或客户端模式
    dtls_transport_ = std::make_shared<DtlsTransport>(true);
    dtls_transport_->SetHandshakeCompletedCB([this](std::string client_key, std::string server_key, RTC::CryptoSuite srtp_crypto_suite) {
        this->OnDtlsCompleted(client_key, server_key, srtp_crypto_suite);
    });
    dtls_transport_->SetOutPutCB([this](char *buf, size_t len) { this->WritePacket(buf, len); });
}

WebRtcTransport::~WebRtcTransport() {}

std::string WebRtcTransport::GetLocalSdp() {
    char sdp[1024 * 10] = {0};
    auto ssrc = getSSRC();
    auto ip = getIP();
    auto pt = getPayloadType();
    auto port = getPort();
    sprintf(sdp,
            "v=0\r\n"
            "o=- 1495799811084970 1495799811084970 IN IP4 %s\r\n"
            "s=Streaming Test\r\n"
            "t=0 0\r\n"
            "a=group:BUNDLE video\r\n"
            "a=msid-semantic: WMS janus\r\n"
            "m=video %u RTP/SAVPF %u\r\n"
            "c=IN IP4 %s\r\n"
            "a=mid:video\r\n"
            "a=sendonly\r\n"
            "a=rtcp-mux\r\n"
            "a=ice-ufrag:%s\r\n"
            "a=ice-pwd:%s\r\n"
            "a=ice-options:trickle\r\n"
            "a=fingerprint:sha-256 %s\r\n"
            "a=setup:actpass\r\n"
            "a=connection:new\r\n"
            "a=rtpmap:%u H264/90000\r\n"
            "a=ssrc:%u cname:janusvideo\r\n"
            "a=ssrc:%u msid:janus janusv0\r\n"
            "a=ssrc:%u mslabel:janus\r\n"
            "a=ssrc:%u label:janusv0\r\n"
            "a=candidate:%s 1 udp %u %s %u typ %s\r\n",
            ip.c_str(), port, pt, ip.c_str(),
            ice_server_->GetUsernameFragment().c_str(),ice_server_->GetPassword().c_str(),
            dtls_transport_->GetMyFingerprint().c_str(),  pt, ssrc, ssrc, ssrc, ssrc, "4", ssrc, ip.c_str(), port, "host");
    return sdp;
}

void WebRtcTransport::OnIceServerCompleted() {
    InfoL;
    dtls_transport_->Start();
    onIceConnected();
}

void WebRtcTransport::OnDtlsCompleted(std::string client_key, std::string server_key, RTC::CryptoSuite srtp_crypto_suite) {
    InfoL << client_key << " " << server_key << " " << (int)srtp_crypto_suite;
    srtp_session_ = std::make_shared<RTC::SrtpSession>(RTC::SrtpSession::Type::OUTBOUND, srtp_crypto_suite, (uint8_t *) client_key.c_str(), client_key.size());
    onDtlsCompleted();
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

void WebRtcTransport::OnInputDataPacket(char *buf, size_t len, struct sockaddr_in *remote_address) {
    if (RTC::StunPacket::IsStun((const uint8_t *) buf, len)) {
        InfoL << "stun:" << hexdump(buf, len);
        RTC::StunPacket *packet = RTC::StunPacket::Parse((const uint8_t *) buf, len);
        if (packet == nullptr) {
            WarnL << "parse stun error" << std::endl;
            return;
        }
        ice_server_->ProcessStunPacket(packet, remote_address);
        return;
    }
    if (DtlsTransport::IsDtlsPacket(buf, len)) {
        InfoL << "dtls:" << hexdump(buf, len);
        dtls_transport_->InputData(buf, len);
        return;
    }
    if (is_rtp(buf)) {
        RtpHeader *header = (RtpHeader *) buf;
        InfoL << "rtp:" << header->dumpString(len);
        return;
    }
    if (is_rtcp(buf)) {
        RtcpHeader *header = (RtcpHeader *) buf;
//        InfoL << "rtcp:" << header->dumpString();
        return;
    }
}

void WebRtcTransport::WritePacket(char *buf, size_t len, struct sockaddr_in *remote_address) {
    onWrite(buf, len, remote_address ? remote_address : (ice_server_ ? ice_server_->GetSelectAddr() : nullptr));
}

void WebRtcTransport::WritRtpPacket(char *buf, size_t len) {
    const uint8_t *p = (uint8_t *) buf;
    bool ret = false;
    if (srtp_session_) {
        ret = srtp_session_->EncryptRtp(&p, &len);
    }
    if (ret) {
        onWrite((char *) p, len, ice_server_->GetSelectAddr());
    }
}

///////////////////////////////////////////////////////////////////////////////////

WebRtcTransportImp::WebRtcTransportImp(const EventPoller::Ptr &poller) {
    _socket = Socket::createSocket(poller, false);
    //随机端口，绑定全部网卡
    _socket->bindUdpSock(0);
    _socket->setOnRead([this](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len){
        OnInputDataPacket(buf->data(), buf->size(), (struct sockaddr_in*)addr);
    });
}

void WebRtcTransportImp::attach(const RtspMediaSource::Ptr &src) {
    assert(src);
    _src = src;
}

void WebRtcTransportImp::onDtlsCompleted() {
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

void WebRtcTransportImp::onIceConnected(){

}

void WebRtcTransportImp::onWrite(const char *buf, size_t len, struct sockaddr_in *dst) {
    auto ptr = BufferRaw::create();
    ptr->assign(buf, len);
//    InfoL << len << " " << SockUtil::inet_ntoa(dst->sin_addr) << " " << ntohs(dst->sin_port);
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

INSTANCE_IMP(WebRtcManager)

WebRtcManager::WebRtcManager() {

}

WebRtcManager::~WebRtcManager() {

}



