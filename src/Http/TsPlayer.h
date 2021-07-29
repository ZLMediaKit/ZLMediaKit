//
// Created by alex on 2021/4/6.
//

/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HTTP_TSPLAYER_H
#define HTTP_TSPLAYER_H

#include <unordered_set>
#include "Util/util.h"
#include "Poller/Timer.h"
#include "Http/HttpDownloader.h"
#include "Player/MediaPlayer.h"
#include "Rtp/Decoder.h"
#include "Rtp/TSDecoder.h"

using namespace toolkit;
namespace mediakit {

    class TsPlayer : public  HttpClientImp , public PlayerBase {
    public:
        TsPlayer(const EventPoller::Ptr &poller);
        ~TsPlayer() override;
        /**
         * 开始播放
         * @param strUrl
         */
        void play(const string &strUrl) override;

        /**
         * 停止播放
         */
        void teardown() override;

    protected:
        /**
         * 收到ts包
         * @param data ts数据负载
         * @param len ts包长度
         */
        virtual void onPacket(const char *data, size_t len) = 0;

    private:
        /**
         * 收到http回复头
         * @param status 状态码，譬如:200 OK
         * @param headers http头
         * @return 返回后续content的长度；-1:后续数据全是content；>=0:固定长度content
         *          需要指出的是，在http头中带有Content-Length字段时，该返回值无效
         */
        ssize_t onResponseHeader(const string &status,const HttpHeader &headers) override;
        /**
         * 收到http conten数据
         * @param buf 数据指针
         * @param size 数据大小
         * @param recvedSize 已收数据大小(包含本次数据大小),当其等于totalSize时将触发onResponseCompleted回调
         * @param totalSize 总数据大小
         */
        void onResponseBody(const char *buf,size_t size,size_t recvedSize, size_t totalSize) override;

        /**
         * 接收http回复完毕,
         */
        void onResponseCompleted() override;

        /**
         * http链接断开回调
         * @param ex 断开原因
         */
        void onDisconnect(const SockException &ex) override;

        /**
         * 重定向事件
         * @param url 重定向url
         * @param temporary 是否为临时重定向
         * @return 是否继续
         */
        bool onRedirectUrl(const string &url,bool temporary) override;

    private:
        void playTs(bool force = false);
        void teardown_l(const SockException &ex);

    private:
        struct UrlComp {
            //url忽略？后面的参数
            bool operator()(const string& __x, const string& __y) const {
                return split(__x,"?")[0] < split(__y,"?")[0];
            }
        };

    protected:
        bool isReconnect = false;
    private:
        bool _first = true;
        string _ts_url;
        int64_t _last_sequence = -1;
        Timer::Ptr _timer;
        TSSegment _segment;
        //是否为mpegts负载
        bool _is_ts_content = false;
        //第一个包是否为ts包
        bool _is_first_packet_ts = false;
        string _status;
    };
}//namespace mediakit
#endif //HTTP_TSPLAYER_H
