/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcTransport.h"
#include <iostream>
#include "RtpExt.h"
#include "Rtcp/Rtcp.h"
#include "Rtcp/RtcpFCI.h"
#include "Rtsp/RtpReceiver.h"

#define RTX_SSRC_OFFSET 2
#define RTP_CNAME "zlmediakit-rtp"
#define RTP_LABEL "zlmediakit-label"
#define RTP_MSLABEL "zlmediakit-mslabel"
#define RTP_MSID RTP_MSLABEL " " RTP_LABEL

//RTC配置项目
namespace RTC {
#define RTC_FIELD "rtc."
//rtp和rtcp接受超时时间
const string kTimeOutSec = RTC_FIELD"timeoutSec";
//服务器外网ip
const string kExternIP = RTC_FIELD"externIP";
//设置remb比特率，非0时关闭twcc并开启remb。该设置在rtc推流时有效，可以控制推流画质
const string kRembBitRate = RTC_FIELD"rembBitRate";

static onceToken token([]() {
    mINI::Instance()[kTimeOutSec] = 15;
    mINI::Instance()[kExternIP] = "";
    mINI::Instance()[kRembBitRate] = 0;
});

}//namespace RTC

WebRtcTransport::WebRtcTransport(const EventPoller::Ptr &poller) {
    _poller = poller;
    _dtls_transport = std::make_shared<RTC::DtlsTransport>(poller, this);
    _ice_server = std::make_shared<RTC::IceServer>(this, makeRandStr(4), makeRandStr(28).substr(4));
}

void WebRtcTransport::onCreate(){

}

void WebRtcTransport::onDestory(){
    _dtls_transport = nullptr;
    _ice_server = nullptr;
}

const EventPoller::Ptr& WebRtcTransport::getPoller() const{
    return _poller;
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
    _srtp_session_recv = std::make_shared<RTC::SrtpSession>(RTC::SrtpSession::Type::INBOUND, srtpCryptoSuite, srtpRemoteKey, srtpRemoteKeyLen);
    onStartWebRTC();
}

void WebRtcTransport::OnDtlsTransportSendData(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) {
    onSendSockData((char *)data, len);
}

void WebRtcTransport::OnDtlsTransportConnecting(const RTC::DtlsTransport *dtlsTransport) {
    InfoL;
}

void WebRtcTransport::OnDtlsTransportFailed(const RTC::DtlsTransport *dtlsTransport) {
    InfoL;
    onShutdown(SockException(Err_shutdown, "dtls transport failed"));
}

void WebRtcTransport::OnDtlsTransportClosed(const RTC::DtlsTransport *dtlsTransport) {
    InfoL;
    onShutdown(SockException(Err_shutdown, "dtls close notify received"));
}

void WebRtcTransport::OnDtlsTransportApplicationDataReceived(const RTC::DtlsTransport *dtlsTransport, const uint8_t *data, size_t len) {
    InfoL << hexdump(data, len);
}
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void WebRtcTransport::onSendSockData(const char *buf, size_t len, bool flush){
    auto tuple = _ice_server->GetSelectedTuple();
    assert(tuple);
    onSendSockData(buf, len, (struct sockaddr_in *) tuple, flush);
}

const RtcSession& WebRtcTransport::getSdp(SdpType type) const{
    switch (type) {
        case SdpType::offer: return *_offer_sdp;
        case SdpType::answer: return *_answer_sdp;
        default: throw std::invalid_argument("不识别的sdp类型");
    }
}

RTC::TransportTuple* WebRtcTransport::getSelectedTuple() const{
    return  _ice_server->GetSelectedTuple();
}

void WebRtcTransport::sendRtcpRemb(uint32_t ssrc, size_t bit_rate) {
    auto remb = FCI_REMB::create({ssrc}, (uint32_t)bit_rate);
    auto fb = RtcpFB::create(PSFBType::RTCP_PSFB_REMB, remb.data(), remb.size());
    fb->ssrc = htonl(0);
    fb->ssrc_media = htonl(ssrc);
    sendRtcpPacket((char *) fb.get(), fb->getSize(), true);
}

void WebRtcTransport::sendRtcpPli(uint32_t ssrc) {
    auto pli = RtcpFB::create(PSFBType::RTCP_PSFB_PLI);
    pli->ssrc = htonl(0);
    pli->ssrc_media = htonl(ssrc);
    sendRtcpPacket((char *) pli.get(), pli->getSize(), true);
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

void WebRtcTransport::onCheckSdp(SdpType type, RtcSession &sdp){
    for (auto &m : sdp.media) {
        if (m.type != TrackApplication && !m.rtcp_mux) {
            throw std::invalid_argument("只支持rtcp-mux模式");
        }
    }
    if (sdp.group.mids.empty()) {
        throw std::invalid_argument("只支持group BUNDLE模式");
    }
    if (type == SdpType::offer) {
        sdp.checkValidSSRC();
    }
}

void WebRtcTransport::onRtcConfigure(RtcConfigure &configure) const {
    //开启remb后关闭twcc，因为开启twcc后remb无效
    GET_CONFIG(size_t, remb_bit_rate, RTC::kRembBitRate);
    configure.enableTWCC(!remb_bit_rate);
}

std::string WebRtcTransport::getAnswerSdp(const string &offer){
    try {
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
        configure.setDefaultSetting(_ice_server->GetUsernameFragment(), _ice_server->GetPassword(),
                                    RtpDirection::sendrecv, fingerprint);
        onRtcConfigure(configure);

        //// 生成answer sdp ////
        _answer_sdp = configure.createAnswer(*_offer_sdp);
        onCheckSdp(SdpType::answer, *_answer_sdp);
        return _answer_sdp->toString();
    } catch (exception &ex) {
        onShutdown(SockException(Err_shutdown, ex.what()));
        throw;
    }
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
        if (_srtp_session_recv->DecryptSrtp((uint8_t *) buf, &len)) {
            onRtp(buf, len);
        } else {
            RtpHeader *rtp = (RtpHeader *) buf;
            WarnL << "srtp_unprotect rtp failed, pt:" << (int)rtp->pt;
        }
        return;
    }
    if (is_rtcp(buf)) {
        if (_srtp_session_recv->DecryptSrtcp((uint8_t *) buf, &len)) {
            onRtcp(buf, len);
        } else {
            WarnL;
        }
        return;
    }
}

void WebRtcTransport::sendRtpPacket(const char *buf, size_t len, bool flush, void *ctx) {
    if (_srtp_session_send) {
        //预留rtx加入的两个字节
        CHECK(len + SRTP_MAX_TRAILER_LEN + 2 <= sizeof(_srtp_buf));
        memcpy(_srtp_buf, buf, len);
        onBeforeEncryptRtp((char *) _srtp_buf, len, ctx);
        if (_srtp_session_send->EncryptRtp(_srtp_buf, &len)) {
            onSendSockData((char *) _srtp_buf, len, flush);
        }
    }
}

void WebRtcTransport::sendRtcpPacket(const char *buf, size_t len, bool flush, void *ctx){
    if (_srtp_session_send) {
        CHECK(len + SRTP_MAX_TRAILER_LEN <= sizeof(_srtp_buf));
        memcpy(_srtp_buf, buf, len);
        onBeforeEncryptRtcp((char *) _srtp_buf, len, ctx);
        if (_srtp_session_send->EncryptRtcp(_srtp_buf, &len)) {
            onSendSockData((char *) _srtp_buf, len, flush);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////
WebRtcTransportImp::Ptr WebRtcTransportImp::create(const EventPoller::Ptr &poller){
    WebRtcTransportImp::Ptr ret(new WebRtcTransportImp(poller), [](WebRtcTransportImp *ptr){
        ptr->onDestory();
       delete ptr;
    });
    ret->onCreate();
    return ret;
}

void WebRtcTransportImp::onCreate(){
    WebRtcTransport::onCreate();
    _socket = Socket::createSocket(getPoller(), false);
    //随机端口，绑定全部网卡
    _socket->bindUdpSock(0);
    weak_ptr<WebRtcTransportImp> weak_self = shared_from_this();
    _socket->setOnRead([weak_self](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) mutable {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->inputSockData(buf->data(), buf->size(), addr);
        }
    });
    _self = shared_from_this();

    GET_CONFIG(float, timeoutSec, RTC::kTimeOutSec);
    _timer = std::make_shared<Timer>(timeoutSec / 2, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        if (strong_self->_alive_ticker.elapsedTime() > timeoutSec * 1000) {
            strong_self->onShutdown(SockException(Err_timeout, "接受rtp和rtcp超时"));
        }
        return true;
    }, getPoller());
}

WebRtcTransportImp::WebRtcTransportImp(const EventPoller::Ptr &poller) : WebRtcTransport(poller) {
    InfoL << this;
}

WebRtcTransportImp::~WebRtcTransportImp() {
    InfoL << this;
}

void WebRtcTransportImp::onDestory() {
    WebRtcTransport::onDestory();
    uint64_t duration = _alive_ticker.createdTime() / 1000;

    //流量统计事件广播
    GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);

    if (_reader) {
        WarnL << "RTC播放器("
              << _media_info._vhost << "/"
              << _media_info._app << "/"
              << _media_info._streamid
              << ")结束播放,耗时(s):" << duration;
        if (_bytes_usage >= iFlowThreshold * 1024) {
            NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _media_info, _bytes_usage, duration, true, static_cast<SockInfo &>(*_socket));
        }
    }

    if (_push_src) {
        WarnL << "RTC推流器("
              << _media_info._vhost << "/"
              << _media_info._app << "/"
              << _media_info._streamid
              << ")结束推流,耗时(s):" << duration;
        if (_bytes_usage >= iFlowThreshold * 1024) {
            NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _media_info, _bytes_usage, duration, false, static_cast<SockInfo &>(*_socket));
        }
    }
}

void WebRtcTransportImp::attach(const RtspMediaSource::Ptr &src, const MediaInfo &info, bool is_play) {
    assert(src);
    _media_info = info;
    if (is_play) {
        _play_src = src;
    } else {
        _push_src = src;
    }
}

void WebRtcTransportImp::onSendSockData(const char *buf, size_t len, struct sockaddr_in *dst, bool flush) {
    auto ptr = BufferRaw::create();
    ptr->assign(buf, len);
    _socket->send(ptr, (struct sockaddr *)(dst), sizeof(struct sockaddr), flush);
}

///////////////////////////////////////////////////////////////////

bool WebRtcTransportImp::canSendRtp() const{
    auto &sdp = getSdp(SdpType::answer);
    return _play_src && (sdp.media[0].direction == RtpDirection::sendrecv || sdp.media[0].direction == RtpDirection::sendonly);
}

bool WebRtcTransportImp::canRecvRtp() const{
    auto &sdp = getSdp(SdpType::answer);
    return _push_src && (sdp.media[0].direction == RtpDirection::sendrecv || sdp.media[0].direction == RtpDirection::recvonly);
}

void WebRtcTransportImp::onStartWebRTC() {
    //获取ssrc和pt相关信息,届时收到rtp和rtcp时分别可以根据pt和ssrc找到相关的信息
    for (auto &m_answer : getSdp(SdpType::answer).media) {
        auto m_offer = getSdp(SdpType::offer).getMedia(m_answer.type);
        auto track = std::make_shared<MediaTrack>();

        track->media = &m_answer;
        track->answer_ssrc_rtp = m_answer.getRtpSSRC();
        track->answer_ssrc_rtx = m_answer.getRtxSSRC();
        track->offer_ssrc_rtp = m_offer->getRtpSSRC();
        track->offer_ssrc_rtx = m_offer->getRtxSSRC();
        track->plan_rtp = &m_answer.plan[0];;
        track->plan_rtx = m_answer.getRelatedRtxPlan(track->plan_rtp->pt);
        track->rtcp_context_send = std::make_shared<RtcpContext>(false);

        //send ssrc --> MediaTrack
        _ssrc_to_track[track->answer_ssrc_rtp] = track;
        _ssrc_to_track[track->answer_ssrc_rtx] = track;

        //recv ssrc --> MediaTrack
        _ssrc_to_track[track->offer_ssrc_rtp] = track;
        _ssrc_to_track[track->offer_ssrc_rtx] = track;

        //rtp pt --> MediaTrack
        _pt_to_track.emplace(track->plan_rtp->pt, std::make_pair(false, track));
        if (track->plan_rtx) {
            //rtx pt --> MediaTrack
            _pt_to_track.emplace(track->plan_rtx->pt, std::make_pair(true, track));
        }
        if (m_offer->type != TrackApplication) {
            //记录rtp ext类型与id的关系，方便接收或发送rtp时修改rtp ext id
            track->rtp_ext_ctx = std::make_shared<RtpExtContext>(*m_offer);
            track->rtp_ext_ctx->setOnGetRtp([this, track](uint8_t pt, uint32_t ssrc, const string &rid) {
                //ssrc --> MediaTrack
                _ssrc_to_track[ssrc] = track;
                InfoL << "get rtp, pt:" << (int) pt << ", ssrc:" << ssrc << ", rid:" << rid;
            });

            int index = 0;
            for (auto &ssrc : m_offer->rtp_ssrc_sim) {
                //记录ssrc对应的MediaTrack
                _ssrc_to_track[ssrc.ssrc] = track;
                if (m_offer->rtp_rids.size() > index) {
                    //支持firefox的simulcast, 提前映射好ssrc和rid的关系
                    track->rtp_ext_ctx->setRid(ssrc.ssrc, m_offer->rtp_rids[index]);
                }
                ++index;
            }
        }
    }

    if (canRecvRtp()) {
        _push_src->setSdp(getSdp(SdpType::answer).toRtspSdp());
    }
    if (canSendRtp()) {
        _reader = _play_src->getRing()->attach(getPoller(), true);
        weak_ptr<WebRtcTransportImp> weak_self = shared_from_this();
        _reader->setReadCB([weak_self](const RtspMediaSource::RingDataType &pkt) {
            auto strongSelf = weak_self.lock();
            if (!strongSelf) {
                return;
            }
            size_t i = 0;
            pkt->for_each([&](const RtpPacket::Ptr &rtp) {
                strongSelf->onSendRtp(rtp, ++i == pkt->size());
            });
        });
        _reader->setDetachCB([weak_self](){
            auto strongSelf = weak_self.lock();
            if (!strongSelf) {
                return;
            }
            strongSelf->onShutdown(SockException(Err_eof, "rtsp ring buffer detached"));
        });

        RtcSession rtsp_send_sdp;
        rtsp_send_sdp.loadFrom(_play_src->getSdp(), false);
        for (auto &m : getSdp(SdpType::answer).media) {
            if (m.type == TrackApplication) {
                continue;
            }
            auto rtsp_media = rtsp_send_sdp.getMedia(m.type);
            if (rtsp_media && getCodecId(rtsp_media->plan[0].codec) == getCodecId(m.plan[0].codec)) {
                auto it = _pt_to_track.find(m.plan[0].pt);
                CHECK(it != _pt_to_track.end());
                //记录发送rtp时约定的信息，届时发送rtp时需要修改pt和ssrc
                _type_to_track[m.type] = it->second.second;
            }
        }
    }
    //使用完毕后，释放强引用，这样确保推流器断开后能及时注销媒体
    _play_src = nullptr;
}

void WebRtcTransportImp::onCheckSdp(SdpType type, RtcSession &sdp){
    WebRtcTransport::onCheckSdp(type, sdp);
    if (type != SdpType::answer) {
        //我们只修改answer sdp
        return;
    }

    //修改answer sdp的ip、端口信息
    GET_CONFIG(string, extern_ip, RTC::kExternIP);
    for (auto &m : sdp.media) {
        m.addr.reset();
        m.addr.address = extern_ip.empty() ? SockUtil::get_local_ip() : extern_ip;
        m.rtcp_addr.reset();
        m.rtcp_addr.address = m.addr.address;
        m.rtcp_addr.port = _socket->get_local_port();
        m.port = m.rtcp_addr.port;
        sdp.origin.address = m.addr.address;
    }

    if (!canSendRtp()) {
        //设置我们发送的rtp的ssrc
        return;
    }

    for (auto &m : sdp.media) {
        if (m.type == TrackApplication) {
            continue;
        }
        //添加answer sdp的ssrc信息
        m.rtp_rtx_ssrc.emplace_back();
        m.rtp_rtx_ssrc[0].ssrc = _play_src->getSsrc(m.type);
        m.rtp_rtx_ssrc[0].cname = RTP_CNAME;
        m.rtp_rtx_ssrc[0].label = RTP_LABEL;
        m.rtp_rtx_ssrc[0].mslabel = RTP_MSLABEL;
        m.rtp_rtx_ssrc[0].msid = RTP_MSID;

        if (m.getRelatedRtxPlan(m.plan[0].pt)) {
            m.rtp_rtx_ssrc.emplace_back();
            m.rtp_rtx_ssrc[1] = m.rtp_rtx_ssrc[0];
            m.rtp_rtx_ssrc[1].ssrc += RTX_SSRC_OFFSET;
        }
    }
}

void WebRtcTransportImp::onRtcConfigure(RtcConfigure &configure) const {
    WebRtcTransport::onRtcConfigure(configure);

    if (_play_src) {
        //这是播放,同时也可能有推流
        configure.video.direction = _push_src ? RtpDirection::sendrecv : RtpDirection::sendonly;
        configure.audio.direction = configure.video.direction;
        configure.setPlayRtspInfo(_play_src->getSdp());
    } else if (_push_src) {
        //这只是推流
        configure.video.direction = RtpDirection::recvonly;
        configure.audio.direction = RtpDirection::recvonly;
    } else {
        throw std::invalid_argument("未设置播放或推流的媒体源");
    }

    //添加接收端口candidate信息
    configure.addCandidate(*getIceCandidate());
}

SdpAttrCandidate::Ptr WebRtcTransportImp::getIceCandidate() const{
    auto candidate = std::make_shared<SdpAttrCandidate>();
    candidate->foundation = "udpcandidate";
    //rtp端口
    candidate->component = 1;
    candidate->transport = "udp";
    //优先级，单candidate时随便
    candidate->priority = 100;
    GET_CONFIG(string, extern_ip, RTC::kExternIP);
    candidate->address = extern_ip.empty() ? SockUtil::get_local_ip() : extern_ip;
    candidate->port = _socket->get_local_port();
    candidate->type = "host";
    return candidate;
}

///////////////////////////////////////////////////////////////////

class RtpChannel : public RtpTrackImp {
public:
    RtpChannel(RtpTrackImp::OnSorted cb, function<void(const FCI_NACK &nack)> on_nack) {
        setOnSorted(std::move(cb));
        _nack_ctx.setOnNack(std::move(on_nack));
    }

    ~RtpChannel() override = default;

    bool inputRtp(TrackType type, int sample_rate, uint8_t *ptr, size_t len, bool is_rtx){
        if (!is_rtx) {
            RtpHeader *rtp = (RtpHeader *) ptr;
            auto seq = ntohs(rtp->seq);
            //统计rtp接受情况，便于生成nack rtcp包
            _nack_ctx.received(seq);
            //统计rtp收到的情况，好做rr汇报
            _rtcp_context.onRtp(seq, ntohl(rtp->stamp), sample_rate, len);
        }
        return RtpTrack::inputRtp(type, sample_rate, ptr, len);
    }

    Buffer::Ptr createRtcpRR(RtcpHeader *sr, uint32_t ssrc) {
        _rtcp_context.onRtcp(sr);
        return _rtcp_context.createRtcpRR(ssrc, getSSRC());
    }

private:
    NackContext _nack_ctx;
    RtcpContext _rtcp_context{true};
};

std::shared_ptr<RtpChannel> MediaTrack::getRtpChannel(uint32_t ssrc) const{
    auto it_chn = rtp_channel.find(rtp_ext_ctx->getRid(ssrc));
    if (it_chn == rtp_channel.end()) {
        return nullptr;
    }
    return it_chn->second;
}

void WebRtcTransportImp::onRtcp(const char *buf, size_t len) {
    _bytes_usage += len;
    auto rtcps = RtcpHeader::loadFromBytes((char *) buf, len);
    for (auto rtcp : rtcps) {
        switch ((RtcpType) rtcp->pt) {
            case RtcpType::RTCP_SR : {
                //对方汇报rtp发送情况
                RtcpSR *sr = (RtcpSR *) rtcp;
                auto it = _ssrc_to_track.find(sr->ssrc);
                if (it != _ssrc_to_track.end()) {
                    auto &track = it->second;
                    auto rtp_chn = track->getRtpChannel(sr->ssrc);
                    if(!rtp_chn){
                        WarnL << "未识别的sr rtcp包:" << rtcp->dumpString();
                    } else {
                        auto rr = rtp_chn->createRtcpRR(sr, track->answer_ssrc_rtp);
                        sendRtcpPacket(rr->data(), rr->size(), true);
                    }
                } else {
                    WarnL << "未识别的sr rtcp包:" << rtcp->dumpString();
                }
                break;
            }
            case RtcpType::RTCP_RR : {
                _alive_ticker.resetTime();
                //对方汇报rtp接收情况
                RtcpRR *rr = (RtcpRR *) rtcp;
                for (auto item : rr->getItemList()) {
                    auto it = _ssrc_to_track.find(item->ssrc);
                    if (it != _ssrc_to_track.end()) {
                        auto &track = it->second;
                        track->rtcp_context_send->onRtcp(rtcp);
                        auto sr = track->rtcp_context_send->createRtcpSR(track->answer_ssrc_rtp);
                        sendRtcpPacket(sr->data(), sr->size(), true);
                    } else {
                        WarnL << "未识别的rr rtcp包:" << rtcp->dumpString();
                    }
                }
                break;
            }
            case RtcpType::RTCP_BYE : {
                //对方汇报停止发送rtp
                RtcpBye *bye = (RtcpBye *) rtcp;
                for (auto ssrc : bye->getSSRC()) {
                    auto it = _ssrc_to_track.find(*ssrc);
                    if (it == _ssrc_to_track.end()) {
                        WarnL << "未识别的bye rtcp包:" << rtcp->dumpString();
                        continue;
                    }
                    _ssrc_to_track.erase(it);
                }
                onShutdown(SockException(Err_eof, "rtcp bye message received"));
                break;
            }
            case RtcpType::RTCP_PSFB:
            case RtcpType::RTCP_RTPFB: {
                if ((RtcpType) rtcp->pt == RtcpType::RTCP_PSFB) {
                    break;
                }
                //RTPFB
                switch ((RTPFBType) rtcp->report_count) {
                    case RTPFBType::RTCP_RTPFB_NACK : {
                        RtcpFB *fb = (RtcpFB *) rtcp;
                        auto it = _ssrc_to_track.find(fb->ssrc_media);
                        if (it == _ssrc_to_track.end()) {
                            WarnL << "未识别的 rtcp包:" << rtcp->dumpString();
                            return;
                        }
                        auto &track = it->second;
                        auto &fci = fb->getFci<FCI_NACK>();
                        track->nack_list.for_each_nack(fci, [&](const RtpPacket::Ptr &rtp) {
                            //rtp重传
                            onSendRtp(rtp, true, true);
                        });
                        break;
                    }
                    default: break;
                }
                break;
            }
            default: break;
        }
    }
}

///////////////////////////////////////////////////////////////////

void WebRtcTransportImp::createRtpChannel(const string &rid, uint32_t ssrc, const MediaTrack::Ptr &track) {
    //rid --> RtpReceiverImp
    auto &ref = track->rtp_channel[rid];
    ref = std::make_shared<RtpChannel>([track, this, rid](RtpPacket::Ptr rtp) mutable {
        onSortedRtp(*track, rid, std::move(rtp));
    }, [track, this, ssrc](const FCI_NACK &nack) mutable {
        onSendNack(*track, nack, ssrc);
    });
    InfoL << "create rtp receiver of ssrc:" << ssrc << ", rid:" << rid << ", codec:" << track->plan_rtp->codec;
}

void WebRtcTransportImp::onRtp(const char *buf, size_t len) {
    _bytes_usage += len;
    _alive_ticker.resetTime();

    RtpHeader *rtp = (RtpHeader *) buf;
    //根据接收到的rtp的pt信息，找到该流的信息
    auto it = _pt_to_track.find(rtp->pt);
    if (it == _pt_to_track.end()) {
        WarnL << "unknown rtp pt:" << (int)rtp->pt;
        return;
    }
    bool is_rtx = it->second.first;
    auto ssrc = ntohl(rtp->ssrc);
    auto &track = it->second.second;

    //修改ext id至统一
    string rid;
    track->rtp_ext_ctx->changeRtpExtId(rtp, true, &rid);

    auto &ref = track->rtp_channel[rid];
    if (!ref) {
        if (is_rtx) {
            //再接收到对应的rtp前，丢弃rtx包
            WarnL << "unknown rtx rtp, rid:" << rid << ", ssrc:" << ssrc << ", codec:" << track->plan_rtp->codec << ", seq:" << ntohs(rtp->seq);
            return;
        }
        createRtpChannel(rid, ssrc, track);
    }

    if (!is_rtx) {
        //这是普通的rtp数据
#if 0
        auto seq = ntohs(rtp->seq);
        if (track->media->type == TrackVideo && seq % 100 == 0) {
            //此处模拟接受丢包
            return;
        }
#endif
        //解析并排序rtp
        ref->inputRtp(track->media->type, track->plan_rtp->sample_rate, (uint8_t *) buf, len, false);
        return;
    }

    //这里是rtx重传包
    //https://datatracker.ietf.org/doc/html/rfc4588#section-4
    auto payload = rtp->getPayloadData();
    auto size = rtp->getPayloadSize(len);
    if (size < 2) {
        return;
    }

    //前两个字节是原始的rtp的seq
    auto origin_seq = payload[0] << 8 | payload[1];
    //rtx 转换为 rtp
    rtp->pt = track->plan_rtp->pt;
    rtp->seq = htons(origin_seq);
    rtp->ssrc = htonl(ref->getSSRC());

    memmove((uint8_t *) buf + 2, buf, payload - (uint8_t *) buf);
    buf += 2;
    len -= 2;
    ref->inputRtp(track->media->type, track->plan_rtp->sample_rate, (uint8_t *) buf, len, true);
}

void WebRtcTransportImp::onSendNack(MediaTrack &track, const FCI_NACK &nack, uint32_t ssrc) {
    auto rtcp = RtcpFB::create(RTPFBType::RTCP_RTPFB_NACK, &nack, FCI_NACK::kSize);
    rtcp->ssrc = htons(track.answer_ssrc_rtp);
    rtcp->ssrc_media = htonl(ssrc);
    DebugL << htonl(ssrc) << " " << nack.getPid();
    sendRtcpPacket((char *) rtcp.get(), rtcp->getSize(), true);
}

///////////////////////////////////////////////////////////////////

void WebRtcTransportImp::onSortedRtp(MediaTrack &track, const string &rid, RtpPacket::Ptr rtp) {
    if (track.media->type == TrackVideo && _pli_ticker.elapsedTime() > 2000) {
        //定期发送pli请求关键帧，方便非rtc等协议
        _pli_ticker.resetTime();
        sendRtcpPli(rtp->getSSRC());

        //开启remb，则发送remb包调节比特率
        GET_CONFIG(size_t, remb_bit_rate, RTC::kRembBitRate);
        if (remb_bit_rate && getSdp(SdpType::answer).supportRtcpFb(SdpConst::kRembRtcpFb)) {
            sendRtcpRemb(rtp->getSSRC(), remb_bit_rate);
        }
    }

    if (_push_src) {
        if (rtp->type == TrackAudio) {
            //音频
            for (auto &pr : _push_src_simulcast) {
                pr.second->onWrite(rtp, false);
            }
        } else {
            //视频
            auto &src = _push_src_simulcast[rid];
            if (!src) {
                auto stream_id = rid.empty() ? _push_src->getId() : _push_src->getId() + "_" + rid;
                auto src_imp = std::make_shared<RtspMediaSourceImp>(_push_src->getVhost(), _push_src->getApp(), stream_id);
                src_imp->setSdp(_push_src->getSdp());
                src_imp->setProtocolTranslation(_push_src->isRecording(Recorder::type_hls),_push_src->isRecording(Recorder::type_mp4));
                src_imp->setListener(shared_from_this());
                src = src_imp;
            }
            src->onWrite(std::move(rtp), false);
        }
    }
}

///////////////////////////////////////////////////////////////////

void WebRtcTransportImp::onSendRtp(const RtpPacket::Ptr &rtp, bool flush, bool rtx){
    auto &track = _type_to_track[rtp->type];
    if (!track) {
        //忽略，对方不支持该编码类型
        return;
    }
    if (!rtx) {
        //统计rtp发送情况，好做sr汇报
        track->rtcp_context_send->onRtp(rtp->getSeq(), ntohl(rtp->getHeader()->stamp), rtp->sample_rate, rtp->size() - RtpPacket::kRtpTcpHeaderSize);
        track->nack_list.push_back(rtp);
#if 0
        //此处模拟发送丢包
        if (rtp->type == TrackVideo && rtp->getSeq() % 100 == 0) {
            return;
        }
#endif
    } else {
        WarnL << "send rtx rtp:" << rtp->getSeq();
    }
    pair<bool/*rtx*/, MediaTrack *> ctx{rtx, track.get()};
    sendRtpPacket(rtp->data() + RtpPacket::kRtpTcpHeaderSize, rtp->size() - RtpPacket::kRtpTcpHeaderSize, flush, &ctx);
    _bytes_usage += rtp->size() - RtpPacket::kRtpTcpHeaderSize;
}

void WebRtcTransportImp::onBeforeEncryptRtp(const char *buf, size_t &len, void *ctx) {
    auto pr = (pair<bool/*rtx*/, MediaTrack *> *) ctx;
    auto header = (RtpHeader *) buf;

    if (!pr->first || !pr->second->plan_rtx) {
        //普通的rtp,或者不支持rtx, 修改目标pt和ssrc
        pr->second->rtp_ext_ctx->changeRtpExtId(header, false);
        header->pt = pr->second->plan_rtp->pt;
        header->ssrc = htonl(pr->second->answer_ssrc_rtp);
    } else {
        //重传的rtp, rtx
        pr->second->rtp_ext_ctx->changeRtpExtId(header, false);
        header->pt = pr->second->plan_rtx->pt;
        if (pr->second->answer_ssrc_rtx) {
            //有rtx单独的ssrc,有些情况下，浏览器支持rtx，但是未指定rtx单独的ssrc
            header->ssrc = htonl(pr->second->answer_ssrc_rtx);
        } else {
            //未单独指定rtx的ssrc，那么使用rtp的ssrc
            header->ssrc = htonl(pr->second->answer_ssrc_rtp);
        }

        auto origin_seq = ntohs(header->seq);
        //seq跟原来的不一样
        header->seq = htons(_rtx_seq[pr->second->media->type]++);
        auto payload = header->getPayloadData();
        auto payload_size = header->getPayloadSize(len);
        if (payload_size) {
            //rtp负载后移两个字节，这两个字节用于存放osn
            //https://datatracker.ietf.org/doc/html/rfc4588#section-4
            memmove(payload + 2, payload, payload_size);
        }
        payload[0] = origin_seq >> 8;
        payload[1] = origin_seq & 0xFF;
        len += 2;
    }
}

void WebRtcTransportImp::onShutdown(const SockException &ex){
    WarnL << ex.what();
    _self = nullptr;
}

/////////////////////////////////////////////////////////////////////////////////////////////

bool WebRtcTransportImp::close(MediaSource &sender, bool force) {
    //此回调在其他线程触发
    if (!force && totalReaderCount(sender)) {
        return false;
    }
    string err = StrPrinter << "close media:" << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    onShutdown(SockException(Err_shutdown,err));
    return true;
}

int WebRtcTransportImp::totalReaderCount(MediaSource &sender) {
    auto total_count = 0;
    for (auto &src : _push_src_simulcast) {
        total_count += src.second->totalReaderCount();
    }
    return total_count;
}

MediaOriginType WebRtcTransportImp::getOriginType(MediaSource &sender) const {
    return MediaOriginType::rtc_push;
}

string WebRtcTransportImp::getOriginUrl(MediaSource &sender) const {
    return "";
}

std::shared_ptr<SockInfo> WebRtcTransportImp::getOriginSock(MediaSource &sender) const {
    return const_cast<WebRtcTransportImp *>(this)->shared_from_this();
}

/////////////////////////////////////////////////////////////////////////////////////////////

string WebRtcTransportImp::get_local_ip() {
    return getSdp(SdpType::answer).media[0].candidate[0].address;
}

uint16_t WebRtcTransportImp::get_local_port() {
    return _socket->get_local_port();
}

string WebRtcTransportImp::get_peer_ip() {
    return SockUtil::inet_ntoa(((struct sockaddr_in *) getSelectedTuple())->sin_addr);
}

uint16_t WebRtcTransportImp::get_peer_port() {
    return ntohs(((struct sockaddr_in *) getSelectedTuple())->sin_port);
}

string WebRtcTransportImp::getIdentifier() const {
    return StrPrinter << this;
}