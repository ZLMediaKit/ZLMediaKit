//
// Created by alex on 2021/4/6.
//

#include "TsPlayer.h"

namespace mediakit {

    TsPlayer::TsPlayer(const EventPoller::Ptr &poller) {
        _segment.setOnSegment([this](const char *data, size_t len) { onPacket(data, len); });
        setPoller(poller ? poller : EventPollerPool::Instance().getPoller());
    }

    TsPlayer::~TsPlayer() {}

    void TsPlayer::play(const string &strUrl) {
        _ts_url.append(strUrl);
        playTs();
    }

    void TsPlayer::teardown_l(const SockException &ex) {
        _timer.reset();
        shutdown(ex);
    }

    void TsPlayer::teardown() {
        teardown_l(SockException(Err_shutdown, "teardown"));
    }

    void TsPlayer::playTs(bool force) {
        if (!force && alive()) {
            //播放器目前还存活，正在下载中
            return;
        }
        WarnL << "fetch:" << _ts_url << " is reconnect:" << isReconnect;
        setOnCreateSocket([this](const EventPoller::Ptr &poller) {
            return Socket::createSocket(poller, true);
        });
        if (!(*this)[kNetAdapter].empty()) {
            setNetAdapter((*this)[kNetAdapter]);
        }
        if (force) {
            HttpClient::clear();
        }
        setMethod("GET");
        sendRequest(_ts_url, 600, 60);
    }

    void TsPlayer::onPacket(const char *data, size_t len) {
        _segment.input(data, len);
    }

    ssize_t TsPlayer::onResponseHeader(const string &status, const HttpClient::HttpHeader &headers) {
        _status = status;
        if (status != "200" && status != "206") {
            //http状态码不符合预期
            shutdown(SockException(Err_other, StrPrinter << "bad http status code:" + status));
            return 0;
        }
        auto content_type = const_cast< HttpClient::HttpHeader &>(headers)["Content-Type"];
        if (content_type.find("video/mp2t") == 0 || content_type.find("video/mpeg") == 0) {
            _is_ts_content = true;
        }
        if (_first) {
            _first = false;
        }
        if(isReconnect){
            onPlayResult(SockException(Err_success, "play reconnect"));
        }
        onPlayResult(SockException(Err_success, "play success"));
        //后续是不定长content
        return -1;
    }

    void TsPlayer::onResponseBody(const char *buf, size_t size, size_t recvedSize, size_t totalSize) {
        if (_status != "200" && _status != "206") {
            return;
        }
        if (recvedSize == size) {
            //开始接收数据
            if (buf[0] == TS_SYNC_BYTE) {
                //这是ts头
                _is_first_packet_ts = true;
            } else {
                WarnL << "可能不是http-ts流";
            }
        }
        TsPlayer::onPacket(buf, size);
    }

    void TsPlayer::onResponseCompleted() {
        //接收完毕
        teardown_l(SockException(Err_success, "play completed"));
    }

    void TsPlayer::onDisconnect(const SockException &ex) {
        WarnL << ex.getErrCode() << " " << ex.what();
        if (_first) {
            //第一次失败，则播放失败
            _first = false;
            onPlayResult(ex);
            return;
        }
        if (ex.getErrCode() == Err_shutdown) {
            //主动shutdown的，不触发回调
            onShutdown(ex);
        } else {
            onShutdown(ex);
        }
    }

    bool TsPlayer::onRedirectUrl(const string &url, bool temporary) {
        return HttpClient::onRedirectUrl(url, temporary);
    }
}//namespace mediakit
