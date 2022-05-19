/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "RtmpPlayer.h"
#include "Rtmp/utils.h"
#include "Util/util.h"
#include "Util/onceToken.h"
#include "Thread/ThreadPool.h"
using namespace toolkit;
using namespace std;

namespace mediakit {

RtmpPlayer::RtmpPlayer(const EventPoller::Ptr &poller) : TcpClient(poller) {}

RtmpPlayer::~RtmpPlayer() {
    DebugL << endl;
}

void RtmpPlayer::teardown() {
    if (alive()) {
        shutdown(SockException(Err_shutdown,"teardown"));
    }
    _app.clear();
    _stream_id.clear();
    _tc_url.clear();
    _beat_timer.reset();
    _play_timer.reset();
    _rtmp_recv_timer.reset();
    _seek_ms = 0;
    RtmpProtocol::reset();

    CLEAR_ARR(_fist_stamp);
    CLEAR_ARR(_now_stamp);

    _map_on_result.clear();
    _deque_on_status.clear();
}

void RtmpPlayer::play(const string &strUrl)  {
    teardown();
    string host_url = FindField(strUrl.data(), "://", "/");
    _app = FindField(strUrl.data(), (host_url + "/").data(), "/");
    _stream_id = FindField(strUrl.data(), (host_url + "/" + _app + "/").data(), NULL);
    _tc_url = string("rtmp://") + host_url + "/" + _app;

    if (!_app.size() || !_stream_id.size()) {
        onPlayResult_l(SockException(Err_other, "rtmp url非法"), false);
        return;
    }
    DebugL << host_url << " " << _app << " " << _stream_id;

    uint16_t port = 1935;
    splitUrl(host_url, host_url, port);

    if (!(*this)[Client::kNetAdapter].empty()) {
        setNetAdapter((*this)[Client::kNetAdapter]);
    }

    weak_ptr<RtmpPlayer> weak_self = dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
    float play_timeout_sec = (*this)[Client::kTimeoutMS].as<int>() / 1000.0f;
    _play_timer.reset(new Timer(play_timeout_sec, [weak_self]() {
        auto strong_self = weak_self.lock();
        if (!strong_self) {
            return false;
        }
        strong_self->onPlayResult_l(SockException(Err_timeout, "play rtmp timeout"), false);
        return false;
    }, getPoller()));

    _metadata_got = false;
    startConnect(host_url, port, play_timeout_sec);
}

void RtmpPlayer::onErr(const SockException &ex){
    //定时器_pPlayTimer为空后表明握手结束了
    onPlayResult_l(ex, !_play_timer);
}

void RtmpPlayer::onPlayResult_l(const SockException &ex, bool handshake_done) {
    if (ex.getErrCode() == Err_shutdown) {
        //主动shutdown的，不触发回调
        return;
    }

    WarnL << ex.getErrCode() << " " << ex.what();
    if (!handshake_done) {
        //开始播放阶段
        _play_timer.reset();
        //是否为性能测试模式
        _benchmark_mode = (*this)[Client::kBenchmarkMode].as<int>();
        onPlayResult(ex);
    } else if (ex) {
        //播放成功后异常断开回调
        onShutdown(ex);
    } else {
        //恢复播放
        onResume();
    }

    if (!ex) {
        //播放成功，恢复rtmp接收超时定时器
        _rtmp_recv_ticker.resetTime();
        auto timeout_ms = (*this)[Client::kMediaTimeoutMS].as<uint64_t>();
        weak_ptr<RtmpPlayer> weakSelf = dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
        auto lam = [weakSelf, timeout_ms]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            if (strongSelf->_rtmp_recv_ticker.elapsedTime() > timeout_ms) {
                //接收rtmp媒体数据超时
                SockException ex(Err_timeout, "receive rtmp timeout");
                strongSelf->onPlayResult_l(ex, true);
                return false;
            }
            return true;
        };
        //创建rtmp数据接收超时检测定时器
        _rtmp_recv_timer = std::make_shared<Timer>(timeout_ms / 2000.0f, lam, getPoller());
    } else {
        shutdown(SockException(Err_shutdown,"teardown"));
    }
}

void RtmpPlayer::onConnect(const SockException &err){
    if (err.getErrCode() != Err_success) {
        onPlayResult_l(err, false);
        return;
    }
    weak_ptr<RtmpPlayer> weakSelf = dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
    startClientSession([weakSelf]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->send_connect();
    });
}

void RtmpPlayer::onRecv(const Buffer::Ptr &buf){
    try {
        if (_benchmark_mode && !_play_timer) {
            //在性能测试模式下，如果rtmp握手完毕后，不再解析rtmp包
            _rtmp_recv_ticker.resetTime();
            return;
        }
        onParseRtmp(buf->data(), buf->size());
    } catch (exception &e) {
        SockException ex(Err_other, e.what());
        //定时器_pPlayTimer为空后表明握手结束了
        onPlayResult_l(ex, !_play_timer);
    }
}

void RtmpPlayer::pause(bool bPause) {
    send_pause(bPause);
}

void RtmpPlayer::speed(float speed) {
    //todo
}

inline void RtmpPlayer::send_connect() {
    AMFValue obj(AMF_OBJECT);
    obj.set("app", _app);
    obj.set("tcUrl", _tc_url);
    //未使用代理
    obj.set("fpad", false);
    //参考librtmp,什么作用?
    obj.set("capabilities", 15);
    //SUPPORT_VID_CLIENT_SEEK 支持seek
    obj.set("videoFunction", 1);
    //只支持aac
    obj.set("audioCodecs", (double) (0x0400));
    //只支持H264
    obj.set("videoCodecs", (double) (0x0080));
    sendInvoke("connect", obj);
    addOnResultCB([this](AMFDecoder &dec) {
        //TraceL << "connect result";
        dec.load<AMFValue>();
        auto val = dec.load<AMFValue>();
        auto level = val["level"].as_string();
        auto code = val["code"].as_string();
        if (level != "status") {
            throw std::runtime_error(StrPrinter << "connect 失败:" << level << " " << code << endl);
        }
        send_createStream();
    });
}

inline void RtmpPlayer::send_createStream() {
    AMFValue obj(AMF_NULL);
    sendInvoke("createStream", obj);
    addOnResultCB([this](AMFDecoder &dec) {
        //TraceL << "createStream result";
        dec.load<AMFValue>();
        _stream_index = dec.load<int>();
        send_play();
    });
}

inline void RtmpPlayer::send_play() {
    AMFEncoder enc;
    enc << "play" << ++_send_req_id << nullptr << _stream_id << -2000;
    sendRequest(MSG_CMD, enc.data());
    auto fun = [](AMFValue &val) {
        //TraceL << "play onStatus";
        auto level = val["level"].as_string();
        auto code = val["code"].as_string();
        if (level != "status") {
            throw std::runtime_error(StrPrinter << "play 失败:" << level << " " << code << endl);
        }
    };
    addOnStatusCB(fun);
    addOnStatusCB(fun);
}

inline void RtmpPlayer::send_pause(bool pause) {
    AMFEncoder enc;
    enc << "pause" << ++_send_req_id << nullptr << pause;
    sendRequest(MSG_CMD, enc.data());
    auto fun = [this, pause](AMFValue &val) {
        //TraceL << "pause onStatus";
        auto level = val["level"].as_string();
        auto code = val["code"].as_string();
        if (level != "status") {
            if (!pause) {
                throw std::runtime_error(StrPrinter << "pause 恢复播放失败:" << level << " " << code << endl);
            }
        } else {
            _paused = pause;
            if (!pause) {
                onPlayResult_l(SockException(Err_success, "resum rtmp success"), true);
            } else {
                //暂停播放
                _rtmp_recv_timer.reset();
            }
        }
    };
    addOnStatusCB(fun);

    _beat_timer.reset();
    if (pause) {
        weak_ptr<RtmpPlayer> weakSelf = dynamic_pointer_cast<RtmpPlayer>(shared_from_this());
        _beat_timer.reset(new Timer((*this)[Client::kBeatIntervalMS].as<int>() / 1000.0f, [weakSelf]() {
            auto strongSelf = weakSelf.lock();
            if (!strongSelf) {
                return false;
            }
            uint32_t timeStamp = (uint32_t)::time(NULL);
            strongSelf->sendUserControl(CONTROL_PING_REQUEST, timeStamp);
            return true;
        }, getPoller()));
    }
}

void RtmpPlayer::onCmd_result(AMFDecoder &dec){
    auto req_id = dec.load<int>();
    auto it = _map_on_result.find(req_id);
    if (it != _map_on_result.end()) {
        it->second(dec);
        _map_on_result.erase(it);
    } else {
        WarnL << "unhandled _result";
    }
}

void RtmpPlayer::onCmd_onStatus(AMFDecoder &dec) {
    AMFValue val;
    while (true) {
        val = dec.load<AMFValue>();
        if (val.type() == AMF_OBJECT) {
            break;
        }
    }
    if (val.type() != AMF_OBJECT) {
        throw std::runtime_error("onStatus:the result object was not found");
    }

    if (_deque_on_status.size()) {
        _deque_on_status.front()(val);
        _deque_on_status.pop_front();
    } else {
        auto level = val["level"];
        auto code = val["code"].as_string();
        if (level.type() == AMF_STRING) {
            // warning 不应该断开
            if (level.as_string() != "status" && level.as_string() != "warning") {
                throw std::runtime_error(StrPrinter << "onStatus 失败:" << level.as_string() << " " << code << endl);
            }
        }
        //WarnL << "unhandled onStatus:" << code;
    }
}

void RtmpPlayer::onCmd_onMetaData(AMFDecoder &dec) {
    //TraceL;
    auto val = dec.load<AMFValue>();
    if (!onCheckMeta(val)) {
        throw std::runtime_error("onCheckMeta failed");
    }
    _metadata_got = true;
}

void RtmpPlayer::onStreamDry(uint32_t stream_index) {
    //TraceL << stream_index;
    onPlayResult_l(SockException(Err_other, "rtmp stream dry"), true);
}

void RtmpPlayer::onMediaData_l(RtmpPacket::Ptr chunk_data) {
    _rtmp_recv_ticker.resetTime();
    if (!_play_timer) {
        //已经触发了onPlayResult事件，直接触发onMediaData事件
        onMediaData(chunk_data);
        return;
    }

    if (chunk_data->isCfgFrame()) {
        //输入配置帧以便初始化完成各个track
        onMediaData(chunk_data);
    } else {
        //先触发onPlayResult事件，这个时候解码器才能初始化完毕
        onPlayResult_l(SockException(Err_success, "play rtmp success"), false);
        //触发onPlayResult事件后，再把帧数据输入到解码器
        onMediaData(chunk_data);
    }
}

void RtmpPlayer::onRtmpChunk(RtmpPacket::Ptr packet) {
    auto &chunk_data = *packet;
    typedef void (RtmpPlayer::*rtmp_func_ptr)(AMFDecoder &dec);
    static unordered_map<string, rtmp_func_ptr> s_func_map;
    static onceToken token([]() {
        s_func_map.emplace("_error", &RtmpPlayer::onCmd_result);
        s_func_map.emplace("_result", &RtmpPlayer::onCmd_result);
        s_func_map.emplace("onStatus", &RtmpPlayer::onCmd_onStatus);
        s_func_map.emplace("onMetaData", &RtmpPlayer::onCmd_onMetaData);
    });

    switch (chunk_data.type_id) {
        case MSG_CMD:
        case MSG_CMD3:
        case MSG_DATA:
        case MSG_DATA3: {
            AMFDecoder dec(chunk_data.buffer, 0, (chunk_data.type_id == MSG_DATA3 || chunk_data.type_id == MSG_CMD3) ? 3 : 0);
            std::string type = dec.load<std::string>();
            auto it = s_func_map.find(type);
            if (it != s_func_map.end()) {
                auto fun = it->second;
                (this->*fun)(dec);
            } else {
                WarnL << "can not support cmd:" << type;
            }
            break;
        }

        case MSG_AUDIO:
        case MSG_VIDEO: {
            auto idx = chunk_data.type_id % 2;
            if (_now_stamp_ticker[idx].elapsedTime() > 500) {
                //计算播放进度时间轴用
                _now_stamp[idx] = chunk_data.time_stamp;
            }
            if (!_metadata_got) {
                if (!onCheckMeta(TitleMeta().getMetadata())) {
                    throw std::runtime_error("onCheckMeta failed");
                }
                _metadata_got = true;
            }
            onMediaData_l(std::move(packet));
            break;
        }

        default: break;
    }
}

uint32_t RtmpPlayer::getProgressMilliSecond() const{
    uint32_t stamp[2] = {0, 0};
    for (auto i = 0; i < 2; i++) {
        stamp[i] = _now_stamp[i] - _fist_stamp[i];
    }
    return _seek_ms + MAX(stamp[0], stamp[1]);
}

void RtmpPlayer::seekToMilliSecond(uint32_t seekMS){
    if (_paused) {
        pause(false);
    }
    AMFEncoder enc;
    enc << "seek" << ++_send_req_id << nullptr << seekMS * 1.0;
    sendRequest(MSG_CMD, enc.data());
    addOnStatusCB([this, seekMS](AMFValue &val) {
        //TraceL << "seek result";
        _now_stamp_ticker[0].resetTime();
        _now_stamp_ticker[1].resetTime();
        int iTimeInc = seekMS - getProgressMilliSecond();
        for (auto i = 0; i < 2; i++) {
            _fist_stamp[i] = _now_stamp[i] + iTimeInc;
            _now_stamp[i] = _fist_stamp[i];
        }
        _seek_ms = seekMS;
    });
}

} /* namespace mediakit */
