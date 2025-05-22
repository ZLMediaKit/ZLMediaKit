/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "SrtCaller.h"
#include "srt/Ack.hpp"
#include "srt/SrtTransport.hpp"
#include "Common/config.h"
#include "Common/Parser.h"
#include <random>

using namespace toolkit;
using namespace std;
using namespace SRT;

namespace mediakit {

//zlm play format
//srt://127.0.0.1:9000?streamid=#!::r=live/test
//srt://127.0.0.1:9000?streamid=#!::r=live/test,h=__defaultVhost__
//zlm push format
//srt://127.0.0.1:9000?streamid=#!::r=live/test,m=publish
//srt://127.0.0.1:9000?streamid=#!::r=live/test,h=__defaultVhost__,m=publish
void SrtUrl::parse(const string &strUrl) {
    //DebugL << "url: " << strUrl;
    _full_url = strUrl;
    auto url = strUrl;

    auto ip = findSubString(url.data(), "://", "?");
    splitUrl(ip, _host, _port);

    auto _params = findSubString(url.data(), "?" , NULL);

    auto kv = Parser::parseArgs(_params);
    auto it = kv.find("streamid");
    if (it != kv.end()) {
        auto streamid = it->second;
        if (!toolkit::start_with(streamid, "#!::")) {
            return;
        }
        _streamid = streamid;
    }

    //TraceL << "ip:      " << ip;
    //TraceL << "_host:   " << _host;
    //TraceL << "_port:   " << _port;
    //TraceL << "_params: " << _params;
    //TraceL << "_streamid: " << _streamid;
    return;
}


////////////  SrtCaller //////////////////////////
SrtCaller::SrtCaller(const toolkit::EventPoller::Ptr &poller) {
    _poller = poller ? std::move(poller) : EventPollerPool::Instance().getPoller();
    _start_timestamp = SteadyClock::now();
    _socket_id       = generateSocketId();

    /* _init_seq_number = generateInitSeq(); */
    _init_seq_number = 0;

    _last_pkt_seq                    = _init_seq_number - 1;
    _pkt_recv_rate_context           = std::make_shared<SRT::PacketRecvRateContext>(_start_timestamp);
    _estimated_link_capacity_context = std::make_shared<SRT::EstimatedLinkCapacityContext>(_start_timestamp);
    _estimated_link_capacity_context->setLastSeq(_last_pkt_seq);

    _send_packet_seq_number = _init_seq_number;
}

SrtCaller::~SrtCaller(void) {
    DebugL;
}

void SrtCaller::onConnect() {
    //DebugL;

    auto peer_addr = SockUtil::make_sockaddr(_url._host.c_str(), (_url._port));
    _socket = Socket::createSocket(_poller, false);
    _socket->bindUdpSock(0, SockUtil::is_ipv4(_url._host.data()) ? "0.0.0.0" : "::");
    _socket->bindPeerAddr((struct sockaddr *)&peer_addr, 0, true);

    weak_ptr<SrtCaller> weak_self = shared_from_this();
    _socket->setOnRead([weak_self](const Buffer::Ptr &buf, struct sockaddr *addr, int addr_len) mutable {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->inputSockData((uint8_t*)buf->data(), buf->size(), addr);
    });

    doHandshake();
}

void SrtCaller::onResult(const SockException &ex) {
    if (!ex) {
        // 会话建立成功
    } else {
        if (ex.getErrCode() == Err_shutdown) {
            // 主动shutdown的，不触发回调
            return;
        }

        if (_socket && _is_handleshake_finished) {
            sendShutDown();
        }
        _is_handleshake_finished = false;
        _handleshake_timer.reset();
        _keeplive_timer.reset();
        _announce_timer.reset();
    }
    return;
}

void SrtCaller::onHandShakeFinished() {
    DebugL;
    _is_handleshake_finished = true;
    if (_handleshake_timer) {
        _handleshake_timer.reset();
    }
    _handleshake_req = nullptr;

    std::weak_ptr<SrtCaller> weak_self = std::static_pointer_cast<SrtCaller>(shared_from_this());
    _keeplive_timer = std::make_shared<Timer>(0.2, [weak_self]()->bool{
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }

        //Keep-alive control packets are sent after a certain timeout from the last time any packet (Control or Data) was sent. 
        //The default timeout for a keep-alive packet to be sent is 1 second.
        if (strong_self->_send_ticker.elapsedTime() > 1000) {
            strong_self->sendKeepLivePacket();
        }
        return true;
    }, getPoller());

    return;
}

void SrtCaller::onSRTData(DataPacket::Ptr pkt) {
    InfoL;
    if (!isPlayer()) {
        WarnL << "this is not a player data ignore";
        return;
    }
}

void SrtCaller::onSendTSData(const Buffer::Ptr &buffer, bool flush) {
    // TraceL;
    //
    DataPacket::Ptr pkt;
    size_t payloadSize = getPayloadSize();
    size_t size = buffer->size();
    char *ptr = buffer->data();
    char *end = buffer->data() + size;


    while (ptr < end && size >= payloadSize) {
        pkt = std::make_shared<DataPacket>();
        pkt->f = 0;
        pkt->packet_seq_number = _send_packet_seq_number & 0x7fffffff;
        _send_packet_seq_number = (_send_packet_seq_number + 1) & 0x7fffffff;
        pkt->PP = 3;
        pkt->O = 0;
        pkt->KK = 0;
        pkt->R = 0;
        pkt->msg_number = _send_msg_number++;
        pkt->dst_socket_id = _peer_socket_id;
        pkt->timestamp = DurationCountMicroseconds(SteadyClock::now() - _start_timestamp);

        sendDataPacket(pkt, ptr, (int)payloadSize, flush);
        ptr += payloadSize;
        size -= payloadSize;
    }

    if (size > 0 && ptr < end) {
        pkt = std::make_shared<DataPacket>();
        pkt->f = 0;
        pkt->packet_seq_number = _send_packet_seq_number & 0x7fffffff;
        _send_packet_seq_number = (_send_packet_seq_number + 1) & 0x7fffffff;
        pkt->PP = 3;
        pkt->O = 0;
        pkt->KK = 0;
        pkt->R = 0;
        pkt->msg_number = _send_msg_number++;
        pkt->dst_socket_id = _peer_socket_id;
        pkt->timestamp = DurationCountMicroseconds(SteadyClock::now() - _start_timestamp);
        sendDataPacket(pkt, ptr, (int)size, flush);
    }
}

void SrtCaller::inputSockData(uint8_t *buf, int len, struct sockaddr *addr) {
    //TraceL << hexdump((void*)buf, len);

    using srt_control_handler = void (SrtCaller::*)(uint8_t * buf, int len, struct sockaddr *addr);
    static std::unordered_map<uint16_t, srt_control_handler> s_control_functions;
    static onceToken token([]() {
        s_control_functions.emplace(SRT::ControlPacket::HANDSHAKE, &SrtCaller::handleHandshake);
        s_control_functions.emplace(SRT::ControlPacket::ACK, &SrtCaller::handleACK);
        s_control_functions.emplace(SRT::ControlPacket::ACKACK, &SrtCaller::handleACKACK);
        s_control_functions.emplace(SRT::ControlPacket::NAK, &SrtCaller::handleNAK);
        s_control_functions.emplace(SRT::ControlPacket::DROPREQ, &SrtCaller::handleDropReq);
        s_control_functions.emplace(SRT::ControlPacket::KEEPALIVE, &SrtCaller::handleKeeplive);
        s_control_functions.emplace(SRT::ControlPacket::SHUTDOWN, &SrtCaller::handleShutDown);
        s_control_functions.emplace(SRT::ControlPacket::PEERERROR, &SrtCaller::handlePeerError);
        s_control_functions.emplace(SRT::ControlPacket::CONGESTIONWARNING, &SrtCaller::handleCongestionWarning);
        s_control_functions.emplace(SRT::ControlPacket::USERDEFINEDTYPE, &SrtCaller::handleUserDefinedType);
    });

    _alive_ticker.resetTime();
    _now = SteadyClock::now();

    // 处理srt数据
    if (DataPacket::isDataPacket(buf, len)) {
        uint32_t socketId = DataPacket::getSocketID(buf, len);
        if (isPlayer()) {
            if (socketId == _socket_id) {
                _pkt_recv_rate_context->inputPacket(_now, len + UDP_HDR_SIZE);
                handleDataPacket(buf, len, addr);
                checkAndSendAckNak();
            }
        }
    } else if (ControlPacket::isControlPacket(buf, len)) {
            uint32_t socketId = ControlPacket::getSocketID(buf, len);
            uint16_t type = ControlPacket::getControlType(buf, len);

            auto it = s_control_functions.find(type);
            if (it == s_control_functions.end()) {
                WarnL << " not support type ignore: " << ControlPacket::getControlType(buf, len);
                return;
            } else {
                (this->*(it->second))(buf, len, addr);
            }

            if (_is_handleshake_finished && isPlayer()){
                checkAndSendAckNak();
            }

    } else {
        // not reach
        WarnL << "not reach this";
    }
}

void SrtCaller::doHandshake() {
    _alive_ticker.resetTime();
    if (!_alive_timer) {
        createTimerForCheckAlive();
    }

    if (!getPassphrase().empty()) {
        _crypto = std::make_shared<SRT::Crypto>(getPassphrase());
    }
 
    sendHandshakeInduction();
    return;
}

void SrtCaller::sendHandshakeInduction() {
    DebugL;
    _induction_ts = SteadyClock::now();

    SRT::HandshakePacket::Ptr req = std::make_shared<SRT::HandshakePacket>();
    req->timestamp     = DurationCountMicroseconds(_induction_ts - _start_timestamp);
    req->dst_socket_id = 0;

    req->version                        = 4;
    req->encryption_field               = 0;
    req->extension_field                = 0x0002;
    req->initial_packet_sequence_number = _init_seq_number;
    req->mtu                            = _mtu;
    req->max_flow_window_size           = _max_flow_window_size;
    req->handshake_type                 = SRT::HandshakePacket::HS_TYPE_INDUCTION;
    req->srt_socket_id                  = _socket_id;
    req->syn_cookie                     = 0;

    auto dataSenderAddr = SockUtil::make_sockaddr(_url._host.c_str(), _url._port);
    req->assignPeerIPBE(&dataSenderAddr);
    req->storeToData();
    _handleshake_req = req;
    sendControlPacket(req, true);

    std::weak_ptr<SrtCaller> weak_self = std::static_pointer_cast<SrtCaller>(shared_from_this());
    _handleshake_timer = std::make_shared<Timer>(0.2, [weak_self]()->bool{
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }

        if (strong_self->_is_handleshake_finished) {
            return false;
        }
        strong_self->sendControlPacket(strong_self->_handleshake_req, true);
        return true;
    }, getPoller());

    return;
}

void SrtCaller::sendHandshakeConclusion() {
    DebugL;
 
    SRT::HandshakePacket::Ptr req = std::make_shared<SRT::HandshakePacket>();
    req->timestamp     = DurationCountMicroseconds(_now - _start_timestamp);
    req->dst_socket_id = 0;

    req->version                         = 5;
    req->encryption_field                = SRT::HandshakePacket::NO_ENCRYPTION;
    req->extension_field                 = HandshakePacket::HS_EXT_FILED_HSREQ | HandshakePacket::HS_EXT_FILED_CONFIG;
    if (_crypto) {
        //The default value is 0 (no encryption advertised). 
        //If neither peer advertises encryption, AES-128 is selected by default 
        /* req->encryption_field = SRT::HandshakePacket::AES_128; */
        req->extension_field |= HandshakePacket::HS_EXT_FILED_KMREQ;
    }
    req->initial_packet_sequence_number  = _init_seq_number;
    req->mtu                             = _mtu;
    req->max_flow_window_size            = _max_flow_window_size;
    req->handshake_type                  = SRT::HandshakePacket::HS_TYPE_CONCLUSION;
    req->srt_socket_id                   = _socket_id;
    req->syn_cookie                      = _sync_cookie;

    auto addr = SockUtil::make_sockaddr(_url._host.c_str(), _url._port);
    req->assignPeerIPBE(&addr);

    HSExtMessage::Ptr ext = std::make_shared<HSExtMessage>();
    ext->extension_type = HSExt::SRT_CMD_HSREQ;
    ext->srt_version = srtVersion(1, 5, 0);
    ext->srt_flag = 0xbf;

    // if set latency, use set value
    _delay = getLatency();
    if (0 == _delay) {
        //The value of minimum TsbpdDelay is negotiated during the SRT handshake exchange and is equal to 120 milliseconds. 
        //The recommended value of TsbpdDelay is 3-4 times RTT.
        _delay = DurationCountMicroseconds(_now - _induction_ts) * getLatencyMul() / 1000;
        if (_delay <= 120) {
            _delay = 120;
        }
    }

    ext->recv_tsbpd_delay = _delay;
    ext->send_tsbpd_delay = _delay;
    req->ext_list.push_back(std::move(ext));

    HSExtStreamID::Ptr extStreamId = std::make_shared<HSExtStreamID>();
    extStreamId->streamid = generateStreamId();
    req->ext_list.push_back(std::move(extStreamId));

    if (_crypto) {
        HSExtKeyMaterial::Ptr keyMaterial = _crypto->generateKeyMaterialExt(HSExt::SRT_CMD_KMREQ);
        req->ext_list.push_back(std::move(keyMaterial));
    }

    req->storeToData();
    _handleshake_req = req;
    sendControlPacket(req);

    return;
}

void SrtCaller::sendACKPacket() {
    uint32_t recv_rate = 0;

    SRT::ACKPacket::Ptr pkt = std::make_shared<SRT::ACKPacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
    pkt->ack_number = ++_ack_number_count;
    pkt->last_ack_pkt_seq_number = _recv_buf->getExpectedSeq();
    pkt->rtt = _rtt;
    pkt->rtt_variance = _rtt_variance;
    pkt->available_buf_size = _recv_buf->getAvailableBufferSize();
    pkt->pkt_recv_rate = _pkt_recv_rate_context->getPacketRecvRate(recv_rate);
    pkt->estimated_link_capacity = _estimated_link_capacity_context->getEstimatedLinkCapacity();
    pkt->recv_rate = recv_rate;
    if(0){
        TraceL<<pkt->pkt_recv_rate<<" pkt/s "<<recv_rate<<" byte/s "<<pkt->estimated_link_capacity<<" pkt/s (cap) "<<pkt->available_buf_size<<" available buf";
        //TraceL<<_pkt_recv_rate_context->dump();
        //TraceL<<"recv estimated:";
        //TraceL<< _pkt_recv_rate_context->dump();
        //TraceL<<"recv queue:";
        //TraceL<<_recv_buf->dump();
    }
    if (pkt->available_buf_size < 2) {
        pkt->available_buf_size = 2;
    }
    pkt->storeToData();
    _ack_send_timestamp[pkt->ack_number] = _now;
    _last_ack_pkt_seq = pkt->last_ack_pkt_seq_number;
    sendControlPacket(pkt, true);
    // TraceL<<"send  ack "<<pkt->dump();
    // TraceL<<_recv_buf->dump();
    return;
}

void SrtCaller::sendLightACKPacket() {
    ACKPacket::Ptr pkt = std::make_shared<ACKPacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
    pkt->ack_number = 0;
    pkt->last_ack_pkt_seq_number = _recv_buf->getExpectedSeq();
    pkt->rtt = 0;
    pkt->rtt_variance = 0;
    pkt->available_buf_size = 0;
    pkt->pkt_recv_rate = 0;
    pkt->estimated_link_capacity = 0;
    pkt->recv_rate = 0;
    pkt->storeToData();
    _last_ack_pkt_seq = pkt->last_ack_pkt_seq_number;
    sendControlPacket(pkt, true);
    TraceL << "send  ack " << pkt->dump();
    return;
}

void SrtCaller::sendNAKPacket(std::list<SRT::PacketQueue::LostPair> &lost_list) {
    SRT::NAKPacket::Ptr pkt = std::make_shared<SRT::NAKPacket>();
    std::list<SRT::PacketQueue::LostPair> tmp;
    auto size = SRT::NAKPacket::getCIFSize(lost_list);
    size_t paylaod_size = getPayloadSize();
    if (size > paylaod_size) {
        WarnL << "loss report cif size " << size;
        size_t num = paylaod_size / 8;

        size_t msgNum = (lost_list.size() + num - 1) / num;
        decltype(lost_list.begin()) cur, next;
        for (size_t i = 0; i < msgNum; ++i) {
            cur = lost_list.begin();
            std::advance(cur, i * num);

            if (i == msgNum - 1) {
                next = lost_list.end();
            } else {
                next = lost_list.begin();
                std::advance(next, (i + 1) * num);
            }
            tmp.assign(cur, next);
            pkt->dst_socket_id = _peer_socket_id;
            pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
            pkt->lost_list = tmp;
            pkt->storeToData();
            sendControlPacket(pkt, true);
        }

    } else {
        pkt->dst_socket_id = _peer_socket_id;
        pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
        pkt->lost_list = lost_list;
        pkt->storeToData();
        sendControlPacket(pkt, true);
    }

    // TraceL<<"send NAK "<<pkt->dump();
    return;
}

void SrtCaller::sendMsgDropReq(uint32_t first, uint32_t last) {
    MsgDropReqPacket::Ptr pkt = std::make_shared<MsgDropReqPacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
    pkt->first_pkt_seq_num = first;
    pkt->last_pkt_seq_num = last;
    pkt->storeToData();
    sendControlPacket(pkt, true);
    return;
}

void SrtCaller::sendKeepLivePacket() {
    auto now = SteadyClock::now();
    SRT::KeepLivePacket::Ptr req = std::make_shared<SRT::KeepLivePacket>();
    req->timestamp = SRT::DurationCountMicroseconds(now - _start_timestamp);
    req->dst_socket_id = _peer_socket_id;
    req->storeToData();
    sendControlPacket(req, true);
    return;
}

void SrtCaller::sendShutDown() {
    auto now = SteadyClock::now();
    ShutDownPacket::Ptr pkt = std::make_shared<ShutDownPacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = SRT::DurationCountMicroseconds(now - _start_timestamp);
    pkt->storeToData();
    sendControlPacket(pkt, true);
    return;
}

void SrtCaller::tryAnnounceKeyMaterial() {
    //TraceL;

    if (!_crypto) {
        return;
    }

    auto pkt = _crypto->takeAwayAnnouncePacket();
    if (!pkt) {
        return;
    }

    auto now = SteadyClock::now();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = SRT::DurationCountMicroseconds(now - _start_timestamp);
    pkt->storeToData();
    _announce_req = pkt;
    sendControlPacket(pkt, true);

    std::weak_ptr<SrtCaller> weak_self = std::static_pointer_cast<SrtCaller>(shared_from_this());
    _announce_timer = std::make_shared<Timer>(0.2, [weak_self]()->bool{
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        if (!strong_self->_announce_req) {
            return false;
        }

        strong_self->sendControlPacket(strong_self->_announce_req, true);
        return true;
    }, getPoller());

    return;
}

void SrtCaller::sendControlPacket(SRT::ControlPacket::Ptr pkt, bool flush) {
    //TraceL;
    sendPacket(pkt, flush);
    return;
}

void SrtCaller::sendDataPacket(SRT::DataPacket::Ptr pkt, char *buf, int len, bool flush) {
    auto data = buf;
    auto size = len;
    BufferLikeString::Ptr payload;
    if (_crypto) {
        payload = _crypto->encrypt(pkt, const_cast<char*>(buf), len);
        if (!payload) {
            WarnL << "encrypt pkt->packet_seq_number: " << pkt->packet_seq_number << ", timestamp: " << "pkt->timestamp " << " fail";
            return;
        }

        data = payload->data();
        size = payload->size();

        tryAnnounceKeyMaterial();
    }

    pkt->storeToData((uint8_t *)data, size);
    sendPacket(pkt, flush);
    _send_buf->inputPacket(pkt);
    return;
}

void SrtCaller::sendPacket(Buffer::Ptr pkt, bool flush) {
    //TraceL << pkt->size();
    auto tmp = _packet_pool.obtain2();
    tmp->assign(pkt->data(), pkt->size());
    _socket->send(std::move(tmp), nullptr, 0, flush);

    _send_ticker.resetTime();
    return;
}

void SrtCaller::handleHandshake(uint8_t *buf, int len, struct sockaddr *addr) {
    //DebugL;
    SRT::HandshakePacket pkt;
    if(!pkt.loadFromData(buf, len)){
        WarnL<< "is not vaild HandshakePacket";
        return;
    }

    if (pkt.handshake_type == SRT::HandshakePacket::HS_TYPE_INDUCTION) {
        handleHandshakeInduction(pkt, addr);
    } else if (pkt.handshake_type == SRT::HandshakePacket::HS_TYPE_CONCLUSION) {
        handleHandshakeConclusion(pkt, addr);
    } else if (pkt.isReject()){
        onResult(SockException(Err_other, StrPrinter << "handshake fail, reject resaon: " << pkt.handshake_type 
                               << ", " << SRT::getRejectReason((SRT_REJECT_REASON)pkt.handshake_type)));
        return;
    } else {
        WarnL << " not support handshake type = " << pkt.handshake_type;
        WarnL << pkt.dump();
    }
    _ack_ticker.resetTime(_now);
    _nak_ticker.resetTime(_now);
    return;
}

void SrtCaller::handleHandshakeInduction(SRT::HandshakePacket &pkt, struct sockaddr *addr) {
    DebugL;

    if (!_handleshake_req) {
        WarnL << "must Induction Phase for handleshake";
        return;
    }

    if (_handleshake_req->handshake_type == HandshakePacket::HS_TYPE_CONCLUSION) {
        WarnL << "should be Conclusion Phase for handleshake ";
        return;
    } else if (_handleshake_req->handshake_type != HandshakePacket::HS_TYPE_INDUCTION) {
        WarnL <<"not reach this";
        return;
    }

    // Induction Phase
    if (pkt.version != 5) {
        WarnL << "not support handleshake version: " << pkt.version;
        return;
    }

    if (pkt.extension_field != 0x4A17) {
        WarnL << "not match SRT MAGIC";
        return;
    }

    if (pkt.dst_socket_id != _handleshake_req->srt_socket_id) {
        WarnL << "not match _socket_id";
        return;
    }

    // TODO: encryption_field

    _sync_cookie = pkt.syn_cookie;

    _mtu = std::min<uint32_t>(pkt.mtu, _mtu);
    _max_flow_window_size = std::min<uint32_t>(pkt.max_flow_window_size, _max_flow_window_size);
    sendHandshakeConclusion();
    return;
}

void SrtCaller::handleHandshakeConclusion(SRT::HandshakePacket &pkt, struct sockaddr *addr) {
    DebugL;

    if (!_handleshake_req) {
        WarnL << "must Conclusion Phase for handleshake ";
        return;
    }

    if (_handleshake_req->handshake_type == HandshakePacket::HS_TYPE_INDUCTION) {
        WarnL << "should be Conclusion Phase for handleshake ";
        return;
    } else if (_handleshake_req->handshake_type != HandshakePacket::HS_TYPE_CONCLUSION) {
        WarnL <<"not reach this";
        return;
    }

    // Conclusion Phase
    if (pkt.version != 5) {
        WarnL << "not support handleshake version: " << pkt.version;
        return;
    }

    if (pkt.dst_socket_id != _handleshake_req->srt_socket_id) {
        WarnL << "not match _socket_id";
        return;
    }

    //  TODO: encryption_field

    _peer_socket_id = pkt.srt_socket_id;

    HSExtMessage::Ptr resp;
    HSExtKeyMaterial::Ptr keyMaterial;

    for (auto& ext : pkt.ext_list) {
        if (!resp) {
            resp = std::dynamic_pointer_cast<HSExtMessage>(ext);
        }
        if (!keyMaterial) {
            keyMaterial = std::dynamic_pointer_cast<HSExtKeyMaterial>(ext);
        }
    }

    if (resp) {
        _delay = std::max<uint16_t>(_delay, resp->recv_tsbpd_delay);
        //DebugL << "flag " << resp->srt_flag;
        //DebugL << "recv_tsbpd_delay " << resp->recv_tsbpd_delay;
        //DebugL << "send_tsbpd_delay " << resp->send_tsbpd_delay;
    }

    if (keyMaterial && _crypto) {
        _crypto->loadFromKeyMaterial(keyMaterial);
    }

    if (isPlayer()) {
        //The recommended threshold value is 1.25 times the SRT latency value.
        _recv_buf = std::make_shared<PacketRecvQueue>(getPktBufSize(), _init_seq_number, _delay * 1250, resp->srt_flag);
    } else {
        //The recommended threshold value is 1.25 times the SRT latency value.
        //Note that the SRT sender keeps packets for at least 1 second in case the latency is not high enough for a large RTT
        _send_buf = std::make_shared<PacketSendQueue>(getPktBufSize(), std::min<uint32_t>((uint32_t)_delay * 1250, 1000000), resp->srt_flag);
    }

    onHandShakeFinished();
    return;
}

void SrtCaller::handleACK(uint8_t *buf, int len, struct sockaddr *addr) {
    // TraceL;
    //Acknowledgement of Acknowledgement (ACKACK) control packets are sent to acknowledge the reception of a Full ACK
    ACKPacket ack;
    if (!ack.loadFromData(buf, len)) {
        return;
    }
    ACKACKPacket::Ptr pkt = std::make_shared<ACKACKPacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
    pkt->ack_number = ack.ack_number;
    pkt->storeToData();
    if (_send_buf) {
        _send_buf->drop(ack.last_ack_pkt_seq_number);
    }
    sendControlPacket(pkt, true);
    // TraceL<<"ack number "<<ack.ack_number;
    return;
}


void SrtCaller::handleACKACK(uint8_t *buf, int len, struct sockaddr *addr) {
    // TraceL;
    ACKACKPacket::Ptr pkt = std::make_shared<ACKACKPacket>();
    pkt->loadFromData(buf, len);

    if(_ack_send_timestamp.find(pkt->ack_number) != _ack_send_timestamp.end()){
        uint32_t rtt = DurationCountMicroseconds(_now - _ack_send_timestamp[pkt->ack_number]);
        _rtt_variance = (3 * _rtt_variance + abs((long)_rtt - (long)rtt)) / 4;
        _rtt = (7 * rtt + _rtt) / 8;
        // TraceL<<" rtt:"<<_rtt<<" rtt variance:"<<_rtt_variance;
        _ack_send_timestamp.erase(pkt->ack_number);

        if(_last_recv_ackack_seq_num < pkt->ack_number){
            _last_recv_ackack_seq_num = pkt->ack_number;
        }else{
            if((_last_recv_ackack_seq_num-pkt->ack_number)>(MAX_TS>>1)){
                _last_recv_ackack_seq_num = pkt->ack_number;
            }
        }

        if(_ack_send_timestamp.size()>1000){
            // clear data
            for(auto it = _ack_send_timestamp.begin(); it != _ack_send_timestamp.end();){
                if(DurationCountMicroseconds(_now-it->second)>5e6){
                    // 超过五秒没有ackack 丢弃
                    it = _ack_send_timestamp.erase(it);
                }else{
                    it++;
                }
            }
        }

    }
    return;
}

void SrtCaller::handleNAK(uint8_t *buf, int len, struct sockaddr *addr) {
    //TraceL;
    NAKPacket pkt;
    pkt.loadFromData(buf, len);
    bool empty = false;
    bool flush = false;

    for (auto& it : pkt.lost_list) {
        if (pkt.lost_list.back() == it) {
            flush = true;
        }
        empty = true;
        auto re_list = _send_buf->findPacketBySeq(it.first, it.second - 1);
        for (auto& pkt : re_list) {
            pkt->R = 1;
            pkt->storeToHeader();
            sendPacket(pkt, flush);
            empty = false;
        }
        if (empty) {
            sendMsgDropReq(it.first, it.second - 1);
        }
    }
    return;
}

void SrtCaller::handleDropReq(uint8_t *buf, int len, struct sockaddr *addr) {
    MsgDropReqPacket pkt;
    pkt.loadFromData(buf, len);
    std::list<DataPacket::Ptr> list;
    // TraceL<<"drop "<<pkt.first_pkt_seq_num<<" last "<<pkt.last_pkt_seq_num;
    _recv_buf->drop(pkt.first_pkt_seq_num, pkt.last_pkt_seq_num, list);
    //checkAndSendAckNak();
    if (list.empty()) {
        return;
    }
    // uint32_t max_seq = 0;
    for (auto& data : list) {
        // max_seq = data->packet_seq_number;
        if (_last_pkt_seq + 1 != data->packet_seq_number) {
            TraceL << "pkt lost " << _last_pkt_seq + 1 << "->" << data->packet_seq_number;
        }
        _last_pkt_seq = data->packet_seq_number;
        onSRTData(std::move(data));
    }
    return;
}

void SrtCaller::handleKeeplive(uint8_t *buf, int len, struct sockaddr *addr) {
    // TraceL;
    return;
}

void SrtCaller::handleShutDown(uint8_t *buf, int len, struct sockaddr *addr) {
    TraceL;
    onResult(SockException(Err_other, "peer close connection"));
    return;
}

void SrtCaller::handlePeerError(uint8_t *buf, int len, struct sockaddr *addr) {
    TraceL;
    return;
}

void SrtCaller::handleCongestionWarning(uint8_t *buf, int len, struct sockaddr *addr) {
    TraceL;
    return;
}

void SrtCaller::handleUserDefinedType(uint8_t *buf, int len, struct sockaddr *addr) {
    /* TraceL; */

    using srt_userd_defined_handler = void (SrtCaller::*)(uint8_t * buf, int len, struct sockaddr *addr);
    static std::unordered_map<uint16_t /*sub_type*/, srt_userd_defined_handler> s_userd_defined_functions;
    static onceToken token([]() {
        s_userd_defined_functions.emplace(SRT::HSExt::SRT_CMD_KMREQ, &SrtCaller::handleKeyMaterialReqPacket);
        s_userd_defined_functions.emplace(SRT::HSExt::SRT_CMD_KMRSP, &SrtCaller::handleKeyMaterialRspPacket);
    });

    uint16_t subtype = ControlPacket::getSubType(buf, len);
    auto it = s_userd_defined_functions.find(subtype);
    if (it == s_userd_defined_functions.end()) {
        WarnL << " not support subtype in user defined msg ignore: " << subtype;
        return;
    } else {
        (this->*(it->second))(buf, len, addr);
    }

    return;
}

void SrtCaller::handleKeyMaterialReqPacket(uint8_t *buf, int len, struct sockaddr *addr) {
    /* TraceL; */

    if (!_crypto) {
        WarnL << " not enable crypto, ignore";
        return;
    }

    KeyMaterialPacket::Ptr pkt = std::make_shared<KeyMaterialPacket>();
    pkt->loadFromData(buf, len);
    _crypto->loadFromKeyMaterial(pkt);

    //rsp
    pkt->sub_type = SRT::HSExt::SRT_CMD_KMRSP;
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
    pkt->storeToData();
    sendControlPacket(pkt, true);
    return;
}

void SrtCaller::handleKeyMaterialRspPacket(uint8_t *buf, int len, struct sockaddr *addr) {
    /* TraceL; */
    _announce_req = nullptr;
    return;
}

void SrtCaller::handleDataPacket(uint8_t *buf, int len, struct sockaddr *addr) {
    //TraceL;
    DataPacket::Ptr pkt = std::make_shared<DataPacket>();
    pkt->loadFromData(buf, len);

    if (_crypto) {
        auto payload = _crypto->decrypt(pkt, pkt->payloadData(), pkt->payloadSize());
        if (!payload) {
            WarnL << "decrypt pkt->packet_seq_number: " << pkt->packet_seq_number << ", timestamp: " << "pkt->timestamp " << " fail";
            return;
        }

        pkt->reloadPayload((uint8_t*)payload->data(), payload->size());
    }

    _estimated_link_capacity_context->inputPacket(_now, pkt);

    std::list<DataPacket::Ptr> list;
    _recv_buf->inputPacket(pkt, list);
    for (auto& data : list) {
        if (_last_pkt_seq + 1 != data->packet_seq_number) {
            TraceL << "pkt lost " << _last_pkt_seq + 1 << "->" << data->packet_seq_number;
        }
        _last_pkt_seq = data->packet_seq_number;
        onSRTData(std::move(data));
    }
    return;
}

void SrtCaller::checkAndSendAckNak() {
    //SRT Periodic NAK reports are sent with a period of (RTT + 4 * RTTVar) / 2 (so called NAKInterval), 
    //with a 20 milliseconds floor
    auto nak_interval = (_rtt + _rtt_variance * 4) / 2;
    if (nak_interval <= 20 * 1000) {
        nak_interval = 20 * 1000;
    }
    if (_nak_ticker.elapsedTime(_now) > nak_interval) {
        auto lost = _recv_buf->getLostSeq();
        if (!lost.empty()) {
            sendNAKPacket(lost);
        }
        _nak_ticker.resetTime(_now);
    }

    //A Full ACK control packet is sent every 10 ms
    if (_ack_ticker.elapsedTime(_now) > 10 * 1000) {
        _light_ack_pkt_count = 0;
        _ack_ticker.resetTime(_now);
        // send a ack per 10 ms for receiver
        if(_last_ack_pkt_seq != _recv_buf->getExpectedSeq()){
            //TraceL<<"send a ack packet";
            sendACKPacket();
        } else{
            //TraceL<<" ignore repeate ack packet";
        }
    } else {
        //The recommendation is to send a Light ACK for every 64 packets received.
        if (_light_ack_pkt_count >= 64) {
            // for high bitrate stream send light ack
            // TODO
            sendLightACKPacket();
            TraceL << "send light ack";
        }
        _light_ack_pkt_count = 0;
    }
    _light_ack_pkt_count++;
    return;
}

void SrtCaller::createTimerForCheckAlive(){
    std::weak_ptr<SrtCaller> weak_self = std::static_pointer_cast<SrtCaller>(shared_from_this());
    auto timeoutSec = getTimeOutSec();
    _alive_timer = std::make_shared<Timer>(
         timeoutSec /2,
        [weak_self,timeoutSec]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return false;
            }
            if (strong_self->_alive_ticker.elapsedTime() > timeoutSec * 1000) {
                strong_self->onResult(SockException(Err_timeout, "Receive srt socket data timeout"));
                return false;
            }
            return true;
        }, getPoller());

    return;
}

int SrtCaller::getLatencyMul() {
    GET_CONFIG(int, latencyMul, SRT::kLatencyMul);
    if (latencyMul < 0) {
        WarnL << "config srt " << kLatencyMul << " not vaild";
        return 4;
    }
    return latencyMul;
}

int SrtCaller::getPktBufSize() {
    GET_CONFIG(int, pktBufSize, SRT::kPktBufSize);
    if (pktBufSize <= 0) {
        WarnL << "config srt " << kPktBufSize << " not vaild";
        return 8912;
    }
    return pktBufSize;
}

float SrtCaller::getTimeOutSec() {
    GET_CONFIG(uint32_t, timeout, SRT::kTimeOutSec);
    if (timeout <= 0) {
        WarnL << "config srt " << kTimeOutSec << " not vaild";
        return 5 * 1000;
    }
    return (float)timeout * (float)1000;
};

std::string SrtCaller::generateStreamId() { 
    return _url._streamid;
};

uint32_t SrtCaller::generateSocketId() {
    // 生成一个 32 位的随机整数
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<uint32_t> dist(0, UINT32_MAX);
    uint32_t id = dist(mt);

    return id;
}

int32_t SrtCaller::generateInitSeq() {
    // 生成一个 32 位的随机整数
    std::random_device rd;
    std::mt19937 mt(rd());
    std::uniform_int_distribution<uint32_t> dist(0, MAX_SEQ);
    int32_t id = dist(mt);
    return id;
}

size_t SrtCaller::getPayloadSize() {
    size_t ret = (_mtu - 28 - 16) / 188 * 188;
    return ret;
}

size_t SrtCaller::getRecvSpeed() const {
    return _socket ? _socket->getRecvSpeed() : 0;
}

size_t SrtCaller::getRecvTotalBytes() const {
    return _socket ? _socket->getRecvTotalBytes() : 0;
}

size_t SrtCaller::getSendSpeed() const {
    return _socket ? _socket->getSendSpeed() : 0;
}

size_t SrtCaller::getSendTotalBytes() const {
    return _socket ? _socket->getSendTotalBytes() : 0;
}

} /* namespace mediakit */

