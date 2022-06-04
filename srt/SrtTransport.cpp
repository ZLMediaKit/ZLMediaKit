#include <stdlib.h>
#include "Util/onceToken.h"

#include "SrtTransport.hpp"
#include "Packet.hpp"
#include "Ack.hpp"
namespace SRT {
#define SRT_FIELD "srt."
//srt 超时时间
const std::string kTimeOutSec = SRT_FIELD"timeoutSec";
//srt 单端口udp服务器
const std::string kPort =  SRT_FIELD"port";

static std::atomic<uint32_t> s_srt_socket_id_generate{125};
////////////  SrtTransport //////////////////////////
SrtTransport::SrtTransport(const EventPoller::Ptr &poller)
    : _poller(poller) {
        _start_timestamp = SteadyClock::now();
        _socket_id = s_srt_socket_id_generate.fetch_add(1);\
        _pkt_recv_rate_context = std::make_shared<PacketRecvRateContext>(_start_timestamp);
        _recv_rate_context = std::make_shared<RecvRateContext>(_start_timestamp);
        _estimated_link_capacity_context = std::make_shared<EstimatedLinkCapacityContext>(_start_timestamp);
    }

SrtTransport::~SrtTransport(){
    TraceL<<" ";
}
const EventPoller::Ptr &SrtTransport::getPoller() const {
    return _poller;
}

void SrtTransport::setSession(Session::Ptr session) {
    _history_sessions.emplace(session.get(), session);
    if (_selected_session) {
        InfoL << "srt network changed: " << _selected_session->get_peer_ip() << ":"
              << _selected_session->get_peer_port() << " -> " << session->get_peer_ip() << ":"
              << session->get_peer_port() << ", id:" << _selected_session->getIdentifier();
    }
    _selected_session = session;
}
const Session::Ptr &SrtTransport::getSession() const {
    return _selected_session;
}

void SrtTransport::switchToOtherTransport(uint8_t *buf, int len,uint32_t socketid, struct sockaddr_storage *addr){
    BufferRaw::Ptr tmp = BufferRaw::create();
    struct sockaddr_storage tmp_addr = *addr;
    tmp->assign((char*)buf,len);
    auto trans = SrtTransportManager::Instance().getItem(std::to_string(socketid));
    if(trans){
        trans->getPoller()->async([tmp,tmp_addr,trans]{
            trans->inputSockData((uint8_t*)tmp->data(),tmp->size(),(struct sockaddr_storage*)&tmp_addr);
        });
    }
}

void SrtTransport::inputSockData(uint8_t *buf, int len, struct sockaddr_storage *addr) {
     using srt_control_handler = void (SrtTransport::*)(uint8_t* buf,int len,struct sockaddr_storage *addr);
    static std::unordered_map<uint16_t, srt_control_handler> s_control_functions;
    static onceToken token([]() {
        s_control_functions.emplace(ControlPacket::HANDSHAKE, &SrtTransport::handleHandshake);
        s_control_functions.emplace(ControlPacket::KEEPALIVE, &SrtTransport::handleKeeplive);
        s_control_functions.emplace(ControlPacket::ACK, &SrtTransport::handleACK);
        s_control_functions.emplace(ControlPacket::NAK, &SrtTransport::handleNAK);
        s_control_functions.emplace(ControlPacket::CONGESTIONWARNING, &SrtTransport::handleCongestionWarning);
        s_control_functions.emplace(ControlPacket::SHUTDOWN, &SrtTransport::handleShutDown);
        s_control_functions.emplace(ControlPacket::ACKACK, &SrtTransport::handleACKACK);
        s_control_functions.emplace(ControlPacket::DROPREQ, &SrtTransport::handleDropReq);
        s_control_functions.emplace(ControlPacket::PEERERROR, &SrtTransport::handlePeerError);
        s_control_functions.emplace(ControlPacket::USERDEFINEDTYPE, &SrtTransport::handleUserDefinedType);
    });
    _now = SteadyClock::now();
    // 处理srt数据
    if (DataPacket::isDataPacket(buf, len)) {
        uint32_t socketId = DataPacket::getSocketID(buf,len);
        if(socketId == _socket_id){
            _pkt_recv_rate_context->inputPacket(_now);
            _estimated_link_capacity_context->inputPacket(_now);
            _recv_rate_context->inputPacket(_now, len);

            handleDataPacket(buf, len, addr);
        }else{
            switchToOtherTransport(buf,len,socketId,addr);
        }
    } else {
        if (ControlPacket::isControlPacket(buf, len)) {
            uint32_t socketId = ControlPacket::getSocketID(buf,len);
             uint16_t type = ControlPacket::getControlType(buf,len);
            if(type != ControlPacket::HANDSHAKE && socketId != _socket_id && _socket_id != 0){
                // socket id not same
                switchToOtherTransport(buf,len,socketId,addr);
                return;
            }
            _pkt_recv_rate_context->inputPacket(_now);
            _estimated_link_capacity_context->inputPacket(_now);
            _recv_rate_context->inputPacket(_now, len);

            auto it = s_control_functions.find(type);
            if (it == s_control_functions.end()) {
                WarnL<<" not support type ignore" << ControlPacket::getControlType(buf,len);
                return;
            }else{
                (this->*(it->second))(buf,len,addr);
            }
        } else {
            // not reach
            WarnL << "not reach this";
        }
    }
}

void SrtTransport::handleHandshakeInduction(HandshakePacket &pkt, struct sockaddr_storage *addr) {
    // Induction Phase
    TraceL << getIdentifier() << " Induction Phase ";
    if (_handleshake_res) {
        TraceL << getIdentifier() << " Induction handle repeate ";
        sendControlPacket(_handleshake_res, true);
        return;
    }

    _init_seq_number = pkt.initial_packet_sequence_number;
    _max_window_size = pkt.max_flow_window_size;
    _mtu = pkt.mtu;

    _peer_socket_id = pkt.srt_socket_id;
    HandshakePacket::Ptr res = std::make_shared<HandshakePacket>();
    res->dst_socket_id = _peer_socket_id;
    res->timestamp = DurationCountMicroseconds(_start_timestamp.time_since_epoch());
    res->mtu = _mtu;
    res->max_flow_window_size = _max_window_size;
    res->initial_packet_sequence_number = _init_seq_number;
    res->version = 5;
    res->encryption_field = HandshakePacket::NO_ENCRYPTION;
    res->extension_field = 0x4A17;
    res->handshake_type = HandshakePacket::HS_TYPE_INDUCTION;
    res->srt_socket_id = _peer_socket_id;
    res->syn_cookie = HandshakePacket::generateSynCookie(addr, _start_timestamp);
    _sync_cookie = res->syn_cookie;
    memcpy(res->peer_ip_addr, pkt.peer_ip_addr, sizeof(pkt.peer_ip_addr) * sizeof(pkt.peer_ip_addr[0]));
    _handleshake_res = res;
    res->storeToData();

    registerSelfHandshake();
    sendControlPacket(res, true);
}
void SrtTransport::handleHandshakeConclusion(HandshakePacket &pkt, struct sockaddr_storage *addr) {
    if(!_handleshake_res){
        ErrorL<<"must Induction Phase for handleshake ";
        return;
    }

    if (_handleshake_res->handshake_type == HandshakePacket::HS_TYPE_INDUCTION) {
        // first
        HSExtMessage::Ptr req;
        HSExtStreamID::Ptr sid;
        uint32_t srt_flag = 0xbf;
        uint16_t delay = 120;

        for (auto ext : pkt.ext_list) {
            //TraceL << getIdentifier() << " ext " << ext->dump();
            if (!req) {
                req = std::dynamic_pointer_cast<HSExtMessage>(ext);
            }
            if(!sid){
                sid = std::dynamic_pointer_cast<HSExtStreamID>(ext);
            }
        }
        if(sid){
            _stream_id = sid->streamid;
        }
        if(req){
            srt_flag = req->srt_flag;
            delay = req->recv_tsbpd_delay;
        }
        TraceL << getIdentifier() << " CONCLUSION Phase ";
        HandshakePacket::Ptr res = std::make_shared<HandshakePacket>();
        res->dst_socket_id = _peer_socket_id;
        res->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
        res->mtu = _mtu;
        res->max_flow_window_size = _max_window_size;
        res->initial_packet_sequence_number = _init_seq_number;
        res->version = 5;
        res->encryption_field = HandshakePacket::NO_ENCRYPTION;
        res->extension_field = HandshakePacket::HS_EXT_FILED_HSREQ;
        res->handshake_type = HandshakePacket::HS_TYPE_CONCLUSION;
        res->srt_socket_id = _socket_id;
        res->syn_cookie = 0;
        res->assignPeerIP(addr);
        HSExtMessage::Ptr ext = std::make_shared<HSExtMessage>();
        ext->extension_type = HSExt::SRT_CMD_HSRSP;
        ext->srt_version = srtVersion(1, 5, 0);
        ext->srt_flag = srt_flag;
        ext->recv_tsbpd_delay = ext->send_tsbpd_delay = delay;
        res->ext_list.push_back(std::move(ext));
        res->storeToData();
        _handleshake_res = res;
        unregisterSelfHandshake();
        registerSelf();
        sendControlPacket(res, true);
        TraceL<<" buf size = "<<res->max_flow_window_size<<" init seq ="<<_init_seq_number<<" lantency="<<delay;
        _recv_buf = std::make_shared<PacketQueue>(res->max_flow_window_size,_init_seq_number, delay*1e6);
        _send_buf = std::make_shared<PacketQueue>(res->max_flow_window_size,_init_seq_number, delay*1e6);
        _send_packet_seq_number = _init_seq_number;
        _buf_delay = delay;
        onHandShakeFinished(_stream_id,addr);
    } else {
        TraceL << getIdentifier() << " CONCLUSION handle repeate ";
        sendControlPacket(_handleshake_res, true);
    }
    _last_ack_pkt_seq_num = _init_seq_number;
}
void SrtTransport::bufCheckInterval(){
}
void SrtTransport::handleHandshake(uint8_t *buf, int len, struct sockaddr_storage *addr){
    HandshakePacket pkt;
    assert(pkt.loadFromData(buf,len));

    if(pkt.handshake_type == HandshakePacket::HS_TYPE_INDUCTION){
        handleHandshakeInduction(pkt,addr);
    }else if(pkt.handshake_type == HandshakePacket::HS_TYPE_CONCLUSION){
        handleHandshakeConclusion(pkt,addr);
    }else{
        WarnL<<" not support handshake type = "<< pkt.handshake_type;
    }
    _ack_ticker.resetTime(_now);
    _nak_ticker.resetTime(_now);
}
void SrtTransport::handleKeeplive(uint8_t *buf, int len, struct sockaddr_storage *addr){
    //TraceL;
    sendKeepLivePacket();
}

 void SrtTransport::sendKeepLivePacket(){
    KeepLivePacket::Ptr pkt = std::make_shared<KeepLivePacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now -_start_timestamp);
    pkt->storeToData();
    sendControlPacket(pkt,true);
 }
void SrtTransport::handleACK(uint8_t *buf, int len, struct sockaddr_storage *addr){
    //TraceL;
    ACKPacket ack;
    if(!ack.loadFromData(buf,len)){
        return;
    }

    ACKACKPacket::Ptr pkt = std::make_shared<ACKACKPacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now -_start_timestamp);
    pkt->ack_number = ack.ack_number;
    pkt->storeToData();
    _send_buf->dropForSend(ack.last_ack_pkt_seq_number);
    sendControlPacket(pkt,true);
    //TraceL<<"ack number "<<ack.ack_number;
}
void SrtTransport::sendMsgDropReq(uint32_t first ,uint32_t last){
    MsgDropReqPacket::Ptr pkt = std::make_shared<MsgDropReqPacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now -_start_timestamp);
    pkt->first_pkt_seq_num = first;
    pkt->last_pkt_seq_num = last;
    pkt->storeToData();
    sendControlPacket(pkt,true);
}
void SrtTransport::handleNAK(uint8_t *buf, int len, struct sockaddr_storage *addr){
    //TraceL;
    NAKPacket pkt;
    pkt.loadFromData(buf,len);
    bool empty = false;

    for(auto it : pkt.lost_list){
        empty = true;
        for(uint32_t i=it.first;i<it.second;++i){
            auto data = _send_buf->findPacketBySeq(i);
            if(data){
                data->R = 1;
                data->storeToHeader();
                sendPacket(data,true);
                empty = false;
            }
        }
        if(empty){
            sendMsgDropReq(it.first,it.second-1);
        }
    }
}
void SrtTransport::handleCongestionWarning(uint8_t *buf, int len, struct sockaddr_storage *addr){
    TraceL;
}
void SrtTransport::handleShutDown(uint8_t *buf, int len, struct sockaddr_storage *addr){
    TraceL;
    onShutdown(SockException(Err_shutdown, "peer close connection"));
}
void SrtTransport::handleDropReq(uint8_t *buf, int len, struct sockaddr_storage *addr){
    MsgDropReqPacket pkt;
    pkt.loadFromData(buf,len);
    //TraceL<<"drop "<<pkt.first_pkt_seq_num<<" last "<<pkt.last_pkt_seq_num;
    _recv_buf->dropForRecv(pkt.first_pkt_seq_num,pkt.last_pkt_seq_num);
}
void SrtTransport::handleUserDefinedType(uint8_t *buf, int len, struct sockaddr_storage *addr){
    TraceL;
}

void SrtTransport::handleACKACK(uint8_t *buf, int len, struct sockaddr_storage *addr){
    //TraceL;
    ACKACKPacket::Ptr pkt = std::make_shared<ACKACKPacket>();
    pkt->loadFromData(buf,len);

    uint32_t rtt = DurationCountMicroseconds(_now - _ack_send_timestamp[pkt->ack_number]);
    _rtt_variance  = (3*_rtt_variance+abs((long)(_rtt - rtt)))/4;
    _rtt = (7*rtt+_rtt)/8;

    _ack_send_timestamp.erase(pkt->ack_number);
}

void SrtTransport::handlePeerError(uint8_t *buf, int len, struct sockaddr_storage *addr){
    TraceL;
}

void SrtTransport::sendACKPacket() {
    ACKPacket::Ptr pkt=std::make_shared<ACKPacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
    pkt->ack_number = ++_ack_number_count;
    pkt->last_ack_pkt_seq_number = _recv_buf->getExpectedSeq();
    pkt->rtt = _rtt;
    pkt->rtt_variance = _rtt_variance;
    pkt->available_buf_size = _recv_buf->getAvailableBufferSize();
    pkt->pkt_recv_rate = _pkt_recv_rate_context->getPacketRecvRate();
    pkt->estimated_link_capacity = _estimated_link_capacity_context->getEstimatedLinkCapacity();
    pkt->recv_rate = _recv_rate_context->getRecvRate();
    pkt->storeToData();
    _ack_send_timestamp[pkt->ack_number] = _now;
     _last_ack_pkt_seq_num = pkt->last_ack_pkt_seq_number;
    sendControlPacket(pkt,true);
    //TraceL<<"send  ack "<<pkt->dump();
}
void SrtTransport::sendLightACKPacket() {
    ACKPacket::Ptr pkt=std::make_shared<ACKPacket>();
   
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
    _last_ack_pkt_seq_num = pkt->last_ack_pkt_seq_number;
    sendControlPacket(pkt,true);
    TraceL<<"send  ack "<<pkt->dump();
}

void SrtTransport::sendNAKPacket(std::list<PacketQueue::LostPair>& lost_list){
    NAKPacket::Ptr pkt = std::make_shared<NAKPacket>();

    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
    pkt->lost_list = lost_list;

    pkt->storeToData();

    //TraceL<<"send NAK "<<pkt->dump();
    sendControlPacket(pkt,true);
}

void SrtTransport::sendShutDown(){
    ShutDownPacket::Ptr pkt = std::make_shared<ShutDownPacket>();
    pkt->dst_socket_id = _peer_socket_id;
    pkt->timestamp = DurationCountMicroseconds(_now - _start_timestamp);
    pkt->storeToData();
    sendControlPacket(pkt,true);
}
void SrtTransport::handleDataPacket(uint8_t *buf, int len, struct sockaddr_storage *addr){
    DataPacket::Ptr pkt = std::make_shared<DataPacket>();
    pkt->loadFromData(buf,len);

    pkt->get_ts = _now;
   
    //TraceL<<" seq="<< pkt->packet_seq_number<<" ts="<<pkt->timestamp<<" size="<<pkt->payloadSize()<<\
    //" PP="<<(int)pkt->PP<<" O="<<(int)pkt->O<<" kK="<<(int)pkt->KK<<" R="<<(int)pkt->R;
#if 1
    _recv_buf->inputPacket(pkt);
#else
    if(pkt->packet_seq_number%100 == 0){
        // drop
        TraceL<<"drop packet";
        TraceL<<"expected size "<<_recv_buf->getExpectedSize()<<" real size="<<_recv_buf->getSize();
    }else{
        _recv_buf->inputPacket(pkt);
    }
#endif
    //TraceL<<" data number size "<<list.size();
    auto list = _recv_buf->tryGetPacket();

    for(auto data : list){
        onSRTData(std::move(data));
    }

    auto nak_interval = (_rtt+_rtt_variance*4)/2;
    if(nak_interval >= 20*1000){
        nak_interval = 20*1000;
    }
    if(_nak_ticker.elapsedTime(_now)>nak_interval || list.empty()){
        auto lost = _recv_buf->getLostSeq();
        if(!lost.empty()){
             sendNAKPacket(lost);
             //TraceL<<"send NAK";
        }
        _nak_ticker.resetTime(_now);
    }

     if(_ack_ticker.elapsedTime(_now)>10*1000){
        _light_ack_pkt_count = 0;
        _ack_ticker.resetTime(_now);
        // send a ack per 10 ms for receiver 
        sendACKPacket();
    }else{
        if(_light_ack_pkt_count >= 64){
            // for high bitrate stream send light ack
            // TODO 
            sendLightACKPacket();
            TraceL<<"send light ack";
        }
        _light_ack_pkt_count = 0;
    }
    _light_ack_pkt_count++;

    //bufCheckInterval();
}

void SrtTransport::sendDataPacket(DataPacket::Ptr pkt,char* buf,int len, bool flush) { 
    pkt->storeToData((uint8_t*)buf,len);
    sendPacket(pkt,flush);
    _send_buf->inputPacket(pkt);
}
void SrtTransport::sendControlPacket(ControlPacket::Ptr pkt, bool flush) { 
    sendPacket(pkt,flush);
}
void SrtTransport::sendPacket(Buffer::Ptr pkt,bool flush){
    if(_selected_session){
         auto tmp = _packet_pool.obtain2();
         tmp->assign(pkt->data(),pkt->size());
         _selected_session->setSendFlushFlag(flush);
         _selected_session->send(std::move(tmp));
    }else{
        WarnL<<"not reach this";
    }
}
std::string SrtTransport::getIdentifier(){
    return _selected_session ? _selected_session->getIdentifier() : "";
}

void SrtTransport::registerSelfHandshake() { 
    SrtTransportManager::Instance().addHandshakeItem(std::to_string(_sync_cookie),shared_from_this());
}
void SrtTransport::unregisterSelfHandshake() { 
    if(_sync_cookie == 0){
        return;
    }
    SrtTransportManager::Instance().removeHandshakeItem(std::to_string(_sync_cookie));
}

void SrtTransport::registerSelf() {
    if(_socket_id == 0){
        return;
    }
    SrtTransportManager::Instance().addItem(std::to_string(_socket_id),shared_from_this());

}
void SrtTransport::unregisterSelf() { 
    SrtTransportManager::Instance().removeItem(std::to_string(_socket_id));
}

void SrtTransport::onShutdown(const SockException &ex){
    sendShutDown();
    WarnL << ex.what();
    unregisterSelfHandshake();
    unregisterSelf();
    for (auto &pr : _history_sessions) {
        auto session = pr.second.lock();
        if (session) {
            session->shutdown(ex);
        }
    }
}
size_t SrtTransport::getPayloadSize(){
    size_t ret = (_mtu - 28 -16)/188*188;
    return ret;
}
void SrtTransport::onSendTSData(const Buffer::Ptr &buffer, bool flush){
    //TraceL;
    DataPacket::Ptr pkt;
    size_t payloadSize = getPayloadSize();
    size_t  size = buffer->size();
    char* ptr = buffer->data();
    char* end = buffer->data()+size;

    while(ptr < end && size >=payloadSize){
        pkt = std::make_shared<DataPacket>();
        pkt->f = 0;
        pkt->packet_seq_number = _send_packet_seq_number++;
        pkt->PP = 3;
        pkt->O = 0;
        pkt->KK = 0;
        pkt->R = 0;
        pkt->msg_number = _send_msg_number++;
        pkt->dst_socket_id = _peer_socket_id;
        pkt->timestamp = DurationCountMicroseconds(SteadyClock::now() - _start_timestamp);
        sendDataPacket(pkt,ptr,(int)payloadSize,flush);
        ptr += payloadSize;
        size -= payloadSize;
    }

    if(size >0 && ptr <end){
        pkt = std::make_shared<DataPacket>();
        pkt->f = 0;
        pkt->packet_seq_number = _send_packet_seq_number++;
        pkt->PP = 3;
        pkt->O = 0;
        pkt->KK = 0;
        pkt->R = 0;
        pkt->msg_number = _send_msg_number++;
        pkt->dst_socket_id = _peer_socket_id;
        pkt->timestamp = DurationCountMicroseconds(SteadyClock::now() - _start_timestamp);
        sendDataPacket(pkt,ptr,(int)size,flush);
    }

}
////////////  SrtTransportManager //////////////////////////
SrtTransportManager &SrtTransportManager::Instance() {
    static SrtTransportManager s_instance;
    return s_instance;
}

void SrtTransportManager::addItem(const std::string &key, const SrtTransport::Ptr &ptr) {
    std::lock_guard<std::mutex> lck(_mtx);
    _map[key] = ptr;
}

SrtTransport::Ptr SrtTransportManager::getItem(const std::string &key) {
    if (key.empty()) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lck(_mtx);
    auto it = _map.find(key);
    if (it == _map.end()) {
        return nullptr;
    }
    return it->second.lock();
}

void SrtTransportManager::removeItem(const std::string &key) {
    std::lock_guard<std::mutex> lck(_mtx);
    _map.erase(key);
}

void SrtTransportManager::addHandshakeItem(const std::string &key, const SrtTransport::Ptr &ptr) {
    std::lock_guard<std::mutex> lck(_handshake_mtx);
    _handshake_map[key] = ptr;
}
void SrtTransportManager::removeHandshakeItem(const std::string &key) {
     std::lock_guard<std::mutex> lck(_handshake_mtx);
    _handshake_map.erase(key);
}
SrtTransport::Ptr SrtTransportManager::getHandshakeItem(const std::string &key) {
    if (key.empty()) {
        return nullptr;
    }
    std::lock_guard<std::mutex> lck(_handshake_mtx);
    auto it = _handshake_map.find(key);
    if (it == _handshake_map.end()) {
        return nullptr;
    }
    return it->second.lock();
}


} // namespace SRT