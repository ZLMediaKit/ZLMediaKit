/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "RtpProcess.h"
#include "Util/File.h"
#include "Http/HttpTSPlayer.h"
#define RTP_APP_NAME "rtp"

namespace mediakit{

string printSSRC(uint32_t ui32Ssrc) {
    char tmp[9] = { 0 };
    ui32Ssrc = htonl(ui32Ssrc);
    uint8_t *pSsrc = (uint8_t *) &ui32Ssrc;
    for (int i = 0; i < 4; i++) {
        sprintf(tmp + 2 * i, "%02X", pSsrc[i]);
    }
    return tmp;
}

static string printAddress(const struct sockaddr *addr){
    return StrPrinter << SockUtil::inet_ntoa(((struct sockaddr_in *) addr)->sin_addr) << ":" << ntohs(((struct sockaddr_in *) addr)->sin_port);
}

RtpProcess::RtpProcess(uint32_t ssrc) {
    _ssrc = ssrc;
    _track = std::make_shared<SdpTrack>();
    _track->_interleaved = 0;
    _track->_samplerate = 90000;
    _track->_type = TrackVideo;
    _track->_ssrc = _ssrc;

    _media_info._schema = RTP_APP_NAME;
    _media_info._vhost = DEFAULT_VHOST;
    _media_info._app = RTP_APP_NAME;
    _media_info._streamid = printSSRC(_ssrc);

    GET_CONFIG(string,dump_dir,RtpProxy::kDumpDir);
    {
        FILE *fp = !dump_dir.empty() ? File::create_file(File::absolutePath(_media_info._streamid + ".rtp", dump_dir).data(), "wb") : nullptr;
        if(fp){
            _save_file_rtp.reset(fp,[](FILE *fp){
                fclose(fp);
            });
        }
    }

    {
        FILE *fp = !dump_dir.empty() ? File::create_file(File::absolutePath(_media_info._streamid + ".mp2", dump_dir).data(), "wb") : nullptr;
        if(fp){
            _save_file_ps.reset(fp,[](FILE *fp){
                fclose(fp);
            });
        }
    }

    {
        FILE *fp = !dump_dir.empty() ? File::create_file(File::absolutePath(_media_info._streamid + ".video", dump_dir).data(), "wb") : nullptr;
        if(fp){
            _save_file_video.reset(fp,[](FILE *fp){
                fclose(fp);
            });
        }
    }
}

RtpProcess::~RtpProcess() {
    DebugP(this);
    if (_addr) {
        delete _addr;
    }

    uint64_t duration = (_last_rtp_time.createdTime() - _last_rtp_time.elapsedTime()) / 1000;
    WarnP(this) << "RTP推流器("
                << _media_info._vhost << "/"
                << _media_info._app << "/"
                << _media_info._streamid
                << ")断开,耗时(s):" << duration;

    //流量统计事件广播
    GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);
    if (_total_bytes > iFlowThreshold * 1024) {
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastFlowReport, _media_info, _total_bytes, duration, false, static_cast<SockInfo &>(*this));
    }
}

bool RtpProcess::inputRtp(const Socket::Ptr &sock, const char *data, int data_len,const struct sockaddr *addr,uint32_t *dts_out) {
    GET_CONFIG(bool,check_source,RtpProxy::kCheckSource);
    //检查源是否合法
    if(!_addr){
        _addr = new struct sockaddr;
        _sock = sock;
        memcpy(_addr,addr, sizeof(struct sockaddr));
        DebugP(this) << "bind to address:" << printAddress(_addr);
        //推流鉴权
        emitOnPublish();
    }

    if(!_muxer){
        //无权限推流
        return false;
    }

    if(check_source && memcmp(_addr,addr,sizeof(struct sockaddr)) != 0){
        DebugP(this) << "address dismatch:" << printAddress(addr) << " != " << printAddress(_addr);
        return false;
    }

    _total_bytes += data_len;
    _last_rtp_time.resetTime();
    bool ret = handleOneRtp(0,_track,(unsigned char *)data,data_len);
    if(dts_out){
        *dts_out = _dts;
    }
    return ret;
}

//判断是否为ts负载
static inline bool checkTS(const uint8_t *packet, int bytes){
    return bytes % TS_PACKET_SIZE == 0 && packet[0] == TS_SYNC_BYTE;
}

void RtpProcess::onRtpSorted(const RtpPacket::Ptr &rtp, int) {
    if(rtp->sequence != _sequence + 1 && rtp->sequence != 0){
        WarnP(this) << "rtp丢包:" << rtp->sequence << " != " << _sequence << "+1" << ",公网环境下请使用tcp方式推流";
    }
    _sequence = rtp->sequence;
    if(_save_file_rtp){
        uint16_t  size = rtp->size() - 4;
        size = htons(size);
        fwrite((uint8_t *) &size, 2, 1, _save_file_rtp.get());
        fwrite((uint8_t *) rtp->data() + 4, rtp->size() - 4, 1, _save_file_rtp.get());
    }
    decodeRtp(rtp->data() + 4 ,rtp->size() - 4);
}

void RtpProcess::onRtpDecode(const uint8_t *packet, int bytes, uint32_t timestamp, int flags) {
    if(_save_file_ps){
        fwrite((uint8_t *)packet,bytes, 1, _save_file_ps.get());
    }

    if (!_decoder) {
        //创建解码器
        if (checkTS(packet, bytes)) {
            //猜测是ts负载
            InfoP(this) << "judged to be TS";
            _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, this);
        } else {
            //猜测是ps负载
            InfoP(this) << "judged to be PS";
            _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ps, this);
        }
    }

    if (_decoder) {
        auto ret = _decoder->input((uint8_t *) packet, bytes);
        if (ret != bytes) {
            WarnP(this) << ret << " != " << bytes << " " << flags;
        }
    }
}

void  RtpProcess::inputFrame(const Frame::Ptr &frame){
    _dts = frame->dts();
    if (_save_file_video && frame->getTrackType() == TrackVideo) {
        fwrite((uint8_t *) frame->data(), frame->size(), 1, _save_file_video.get());
    }
    _muxer->inputFrame(frame);
}

void  RtpProcess::addTrack(const Track::Ptr & track){
    _muxer->addTrack(track);
}

bool RtpProcess::alive() {
    GET_CONFIG(int,timeoutSec,RtpProxy::kTimeoutSec)
    if(_last_rtp_time.elapsedTime() / 1000 < timeoutSec){
        return true;
    }
    return false;
}

string RtpProcess::get_peer_ip() {
    if(_addr){
        return SockUtil::inet_ntoa(((struct sockaddr_in *) _addr)->sin_addr);
    }
    return "0.0.0.0";
}

uint16_t RtpProcess::get_peer_port() {
    if(!_addr){
        return 0;
    }
    return ntohs(((struct sockaddr_in *) _addr)->sin_port);
}

string RtpProcess::get_local_ip() {
    if(_sock){
        return _sock->get_local_ip();
    }
    return "0.0.0.0";
}

uint16_t RtpProcess::get_local_port() {
    if(_sock){
       return _sock->get_local_port();
    }
    return 0;
}

string RtpProcess::getIdentifier() const{
    return _media_info._streamid;
}

int RtpProcess::totalReaderCount(){
    return _muxer ? _muxer->totalReaderCount() : 0;
}

void RtpProcess::setListener(const std::weak_ptr<MediaSourceEvent> &listener){
    if(_muxer){
        _muxer->setMediaListener(listener);
    }else{
        _listener = listener;
    }
}

void RtpProcess::emitOnPublish() {
    weak_ptr<RtpProcess> weak_self = shared_from_this();
    Broadcast::PublishAuthInvoker invoker = [weak_self](const string &err, bool enableRtxp, bool enableHls, bool enableMP4) {
        auto strongSelf = weak_self.lock();
        if (!strongSelf) {
            return;
        }
        if (err.empty()) {
            strongSelf->_muxer = std::make_shared<MultiMediaSourceMuxer>(strongSelf->_media_info._vhost,
                                                                         strongSelf->_media_info._app,
                                                                         strongSelf->_media_info._streamid, 0,
                                                                         enableRtxp, enableRtxp, enableHls, enableMP4);
            strongSelf->_muxer->setMediaListener(strongSelf->_listener);
            InfoP(strongSelf) << "允许RTP推流";
        } else {
            WarnP(strongSelf) << "禁止RTP推流:" << err;
        }
    };

    //触发推流鉴权事件
    auto flag = NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastMediaPublish, _media_info, invoker, static_cast<SockInfo &>(*this));
    if(!flag){
        //该事件无人监听,默认不鉴权
        GET_CONFIG(bool, toRtxp, General::kPublishToRtxp);
        GET_CONFIG(bool, toHls, General::kPublishToHls);
        GET_CONFIG(bool, toMP4, General::kPublishToMP4);
        invoker("", toRtxp, toHls, toMP4);
    }
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)