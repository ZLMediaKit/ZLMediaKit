#include "Util/util.h"
#include <memory>

#include "SrtTransportImp.hpp"

namespace SRT {
SrtTransportImp::SrtTransportImp(const EventPoller::Ptr &poller)
    : SrtTransport(poller) {}

SrtTransportImp::~SrtTransportImp() {
    InfoP(this);
    uint64_t duration = _alive_ticker.createdTime() / 1000;
    WarnP(this) << (_is_pusher ? "srt 推流器(" : "srt 播放器(") << _media_info.shortUrl() << ")断开,耗时(s):" << duration;

    // 流量统计事件广播
    GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);
    if (_total_bytes >= iFlowThreshold * 1024) {
        NoticeCenter::Instance().emitEvent(
            Broadcast::kBroadcastFlowReport, _media_info, _total_bytes, duration, !_is_pusher,
            static_cast<SockInfo &>(*this));
    }
}

void SrtTransportImp::onHandShakeFinished(std::string &streamid, struct sockaddr_storage *addr) {
    SrtTransport::onHandShakeFinished(streamid,addr);
    // TODO parse stream id like this zlmediakit.com/live/test?token=1213444&type=push
    if (!_addr) {
        _addr.reset(new sockaddr_storage(*((sockaddr_storage *)addr)));
    }
    _is_pusher = false;
    TraceL << " stream id " << streamid;
    if (!parseStreamid(streamid)) {
        onShutdown(SockException(Err_shutdown, "stream id not vaild"));
        return;
    }

    auto params = Parser::parseArgs(_media_info._param_strs);
    if (params["m"] == "publish") {
        _is_pusher = true;
        _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, this);
        emitOnPublish();
    } else {
        _is_pusher = false;
        emitOnPlay();
    }
}

//
bool SrtTransportImp::parseStreamid(std::string &streamid) {

    if (!toolkit::start_with(streamid, "#!::")) {
        return false;
    }
    _media_info._schema = SRT_SCHEMA;

    std::string real_streamid = streamid.substr(4);
    std::string vhost, app, stream_name;

    auto params = Parser::parseArgs(real_streamid, ",", "=");

    for (auto it : params) {
        if (it.first == "h") {
            vhost = it.second;
        } else if (it.first == "r") {
            auto tmps = toolkit::split(it.second, "/");
            if (tmps.size() < 2) {
                continue;
            }
            app = tmps[0];
            stream_name = tmps[1];
        } else {
            if (_media_info._param_strs.empty()) {
                _media_info._param_strs = it.first + "=" + it.second;
            } else {
                _media_info._param_strs += "&" + it.first + "=" + it.second;
            }
        }
    }
    if (app == "" || stream_name == "") {
        return false;
    }

    if (vhost != "") {
        _media_info._vhost = vhost;
    } else {
        _media_info._vhost = DEFAULT_VHOST;
    }

    _media_info._app = app;
    _media_info._streamid = stream_name;

    TraceL << " mediainfo=" << _media_info.shortUrl() << " params=" << _media_info._param_strs;

    return true;
}

void SrtTransportImp::onSRTData(DataPacket::Ptr pkt) {
    if (!_is_pusher) {
        WarnP(this) << "this is a player data ignore";
        return;
    }
    if (_decoder) {
        _decoder->input(reinterpret_cast<const uint8_t *>(pkt->payloadData()), pkt->payloadSize());
        //TraceL<<" size "<<pkt->payloadSize();
    } else {
        WarnP(this) << " not reach this";
    }
}

void SrtTransportImp::onShutdown(const SockException &ex) {
    SrtTransport::onShutdown(ex);
}

bool SrtTransportImp::close(mediakit::MediaSource &sender) {
    std::string err = StrPrinter << "close media: " << sender.getUrl();
    weak_ptr<SrtTransportImp> weak_self = static_pointer_cast<SrtTransportImp>(shared_from_this());
    getPoller()->async([weak_self, err]() {
        auto strong_self = weak_self.lock();
        if (strong_self) {
            strong_self->onShutdown(SockException(Err_shutdown, err));
            // 主动关闭推流，那么不延时注销
            strong_self->_muxer = nullptr;
        }
    });
    return true;
}

// 获取媒体源类型
mediakit::MediaOriginType SrtTransportImp::getOriginType(mediakit::MediaSource &sender) const {
    return MediaOriginType::srt_push;
}

// 获取媒体源url或者文件路径
std::string SrtTransportImp::getOriginUrl(mediakit::MediaSource &sender) const {
    return _media_info._full_url;
}

// 获取媒体源客户端相关信息
std::shared_ptr<SockInfo> SrtTransportImp::getOriginSock(mediakit::MediaSource &sender) const {
    return static_pointer_cast<SockInfo>(getSession());
}

void SrtTransportImp::emitOnPublish() {
    std::weak_ptr<SrtTransportImp> weak_self = static_pointer_cast<SrtTransportImp>(shared_from_this());
    Broadcast::PublishAuthInvoker invoker = [weak_self](const std::string &err, const ProtocolOption &option) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->getPoller()->async([weak_self, err, option](){
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return;
            }
            if (err.empty()) {
                strong_self->_muxer = std::make_shared<MultiMediaSourceMuxer>(strong_self->_media_info._vhost,
                                                                              strong_self->_media_info._app,
                                                                              strong_self->_media_info._streamid,0.0f,
                                                                              option);
                strong_self->_muxer->setMediaListener(strong_self);
                strong_self->doCachedFunc();
                InfoP(strong_self) << "允许 srt 推流";
            } else {
                WarnP(strong_self) << "禁止 srt 推流:" << err;
                strong_self->onShutdown(SockException(Err_refused, err));
            }
        });
    };

    // 触发推流鉴权事件
    auto flag = NoticeCenter::Instance().emitEvent(
        Broadcast::kBroadcastMediaPublish, MediaOriginType::srt_push, _media_info, invoker,
        static_cast<SockInfo &>(*this));
    if (!flag) {
        // 该事件无人监听,默认不鉴权
        invoker("", ProtocolOption());
    }
}

void SrtTransportImp::emitOnPlay() {
    std::weak_ptr<SrtTransportImp> weak_self = static_pointer_cast<SrtTransportImp>(shared_from_this());
    Broadcast::AuthInvoker invoker = [weak_self](const string &err) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return;
        }
        strong_self->getPoller()->async([strong_self, err] {
            if (err != "") {
                strong_self->onShutdown(SockException(Err_refused, err));
            } else {
                strong_self->doPlay();
            }
        });
    };

    auto flag = NoticeCenter::Instance().emitEvent(
        Broadcast::kBroadcastMediaPlayed, _media_info, invoker, static_cast<SockInfo &>(*this));
    if (!flag) {
        doPlay();
    }
}

void SrtTransportImp::doPlay() {
    // 异步查找直播流
    MediaInfo info = _media_info;
    info._schema = TS_SCHEMA;
    std::weak_ptr<SrtTransportImp> weak_self = static_pointer_cast<SrtTransportImp>(shared_from_this());
    MediaSource::findAsync(info, getSession(), [weak_self](const MediaSource::Ptr &src) {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            // 本对象已经销毁
            TraceL << "本对象已经销毁";
            return;
        }
        if (!src) {
            // 未找到该流
            TraceL << "未找到该流";
            strong_self->onShutdown(SockException(Err_shutdown));
        } else {
            TraceL << "找到该流";
            auto ts_src = dynamic_pointer_cast<TSMediaSource>(src);
            assert(ts_src);
            ts_src->pause(false);
            strong_self->_ts_reader = ts_src->getRing()->attach(strong_self->getPoller());
            weak_ptr<Session> weak_session = strong_self->getSession();
            strong_self->_ts_reader->setGetInfoCB([weak_session]() { return weak_session.lock(); });
            strong_self->_ts_reader->setDetachCB([weak_self]() {
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    // 本对象已经销毁
                    return;
                }
                strong_self->onShutdown(SockException(Err_shutdown));
            });
            strong_self->_ts_reader->setReadCB([weak_self](const TSMediaSource::RingDataType &ts_list) {
                auto strong_self = weak_self.lock();
                if (!strong_self) {
                    // 本对象已经销毁
                    return;
                }
                size_t i = 0;
                auto size = ts_list->size();
                ts_list->for_each([&](const TSPacket::Ptr &ts) { strong_self->onSendTSData(ts, ++i == size); });
            });
        }
    });
}

std::string SrtTransportImp::get_peer_ip() {
    if (!_addr) {
        return "::";
    }
    return SockUtil::inet_ntoa((sockaddr *)_addr.get());
}

uint16_t SrtTransportImp::get_peer_port() {
    if (!_addr) {
        return 0;
    }
    return SockUtil::inet_port((sockaddr *)_addr.get());
}

std::string SrtTransportImp::get_local_ip() {
    auto s = getSession();
    if (s) {
        return s->get_local_ip();
    }
    return "::";
}

uint16_t SrtTransportImp::get_local_port() {
    auto s = getSession();
    if (s) {
        return s->get_local_port();
    }
    return 0;
}

std::string SrtTransportImp::getIdentifier() const {
    return _media_info._streamid;
}

bool SrtTransportImp::inputFrame(const Frame::Ptr &frame) {
    if (_muxer) {
        //TraceL<<"before type "<<frame->getCodecName()<<" dts "<<frame->dts()<<" pts "<<frame->pts();
        auto frame_tmp = std::make_shared<FrameStamp>(frame, _type_to_stamp[frame->getTrackType()],false);
        //TraceL<<"after type "<<frame_tmp->getCodecName()<<" dts "<<frame_tmp->dts()<<" pts "<<frame_tmp->pts();
        return _muxer->inputFrame(frame_tmp);
    }
    if (_cached_func.size() > 200) {
        WarnL << "cached frame of track(" << frame->getCodecName() << ") is too much, now dropped";
        return false;
    }
    auto frame_cached = Frame::getCacheAbleFrame(frame);
    lock_guard<recursive_mutex> lck(_func_mtx);
    _cached_func.emplace_back([this, frame_cached]() { 
        //TraceL<<"before type "<<frame_cached->getCodecName()<<" dts "<<frame_cached->dts()<<" pts "<<frame_cached->pts();
        auto frame_tmp = std::make_shared<FrameStamp>(frame_cached, _type_to_stamp[frame_cached->getTrackType()],false);
        //TraceL<<"after type "<<frame_tmp->getCodecName()<<" dts "<<frame_tmp->dts()<<" pts "<<frame_tmp->pts();
        _muxer->inputFrame(frame_tmp);
    });
    return true;
}

bool SrtTransportImp::addTrack(const Track::Ptr &track) {
    _type_to_stamp.emplace(track->getTrackType(),Stamp());
    if (_muxer) {
        return _muxer->addTrack(track);
    }

    lock_guard<recursive_mutex> lck(_func_mtx);
    _cached_func.emplace_back([this, track]() { _muxer->addTrack(track); });
    return true;
}

void SrtTransportImp::addTrackCompleted() {
    if (_muxer) {
        _muxer->addTrackCompleted();
    } else {
        lock_guard<recursive_mutex> lck(_func_mtx);
        _cached_func.emplace_back([this]() { _muxer->addTrackCompleted(); });
    }
    if(_type_to_stamp.size() >1){
        _type_to_stamp[TrackType::TrackAudio].syncTo(_type_to_stamp[TrackType::TrackVideo]);
    }
}

void SrtTransportImp::doCachedFunc() {
    lock_guard<recursive_mutex> lck(_func_mtx);
    for (auto &func : _cached_func) {
        func();
    }
    _cached_func.clear();
}

int SrtTransportImp::getLatencyMul() {
    GET_CONFIG(int, latencyMul, kLatencyMul);
    if (latencyMul <= 0) {
        WarnL << "config srt " << kLatencyMul << " not vaild";
        return 4;
    }
    return latencyMul;
}

float SrtTransportImp::getTimeOutSec() {
    GET_CONFIG(float, timeOutSec, kTimeOutSec);
    if (timeOutSec <= 0) {
        WarnL << "config srt " << kTimeOutSec << " not vaild";
        return 5.0;
    }
    return timeOutSec;
}

int SrtTransportImp::getPktBufSize() {
    // kPktBufSize
    GET_CONFIG(int, pktBufSize, kPktBufSize);
    if (pktBufSize <= 0) {
        WarnL << "config srt " << kPktBufSize << " not vaild";
        return 8912;
    }
    return pktBufSize;
}

} // namespace SRT