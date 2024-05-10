/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_events.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Http/HttpSession.h"
#include "Rtsp/RtspSession.h"
#include "Record/MP4Recorder.h"

#ifdef ENABLE_WEBRTC
#include "webrtc/WebRtcTransport.h"
#endif

using namespace toolkit;
using namespace mediakit;

static void* s_tag;
static mk_events s_events = {0};

API_EXPORT void API_CALL mk_events_listen(const mk_events *events){
    if (events) {
        memcpy(&s_events, events, sizeof(s_events));
    } else {
        memset(&s_events, 0, sizeof(s_events));
    }

    static onceToken token([]{
        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastMediaChanged,[](BroadcastMediaChangedArgs){
            if(s_events.on_mk_media_changed){
                s_events.on_mk_media_changed(bRegist,
                                             (mk_media_source)&sender);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastRecordMP4,[](BroadcastRecordMP4Args){
            if(s_events.on_mk_record_mp4){
                s_events.on_mk_record_mp4((mk_record_info)&info);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag, Broadcast::kBroadcastRecordTs, [](BroadcastRecordTsArgs) {
            if (s_events.on_mk_record_ts) {
                s_events.on_mk_record_ts((mk_record_info)&info);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastHttpRequest,[](BroadcastHttpRequestArgs){
            if(s_events.on_mk_http_request){
                int consumed_int = consumed;
                s_events.on_mk_http_request((mk_parser)&parser,
                                            (mk_http_response_invoker)&invoker,
                                            &consumed_int,
                                            (mk_sock_info)&sender);
                consumed = consumed_int;
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastHttpAccess,[](BroadcastHttpAccessArgs){
            if(s_events.on_mk_http_access){
                s_events.on_mk_http_access((mk_parser)&parser,
                                           path.c_str(),
                                           is_dir,
                                           (mk_http_access_path_invoker)&invoker,
                                           (mk_sock_info)&sender);
            } else{
                invoker("","",0);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastHttpBeforeAccess,[](BroadcastHttpBeforeAccessArgs){
            if(s_events.on_mk_http_before_access){
                char path_c[4 * 1024] = {0};
                strcpy(path_c,path.c_str());
                s_events.on_mk_http_before_access((mk_parser) &parser,
                                                  path_c,
                                                  (mk_sock_info) &sender);
                path = path_c;
            }
        });


        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastOnGetRtspRealm,[](BroadcastOnGetRtspRealmArgs){
            if (s_events.on_mk_rtsp_get_realm) {
                s_events.on_mk_rtsp_get_realm((mk_media_info) &args,
                                              (mk_rtsp_get_realm_invoker) &invoker,
                                              (mk_sock_info) &sender);
            }else{
                invoker("");
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastOnRtspAuth,[](BroadcastOnRtspAuthArgs){
            if (s_events.on_mk_rtsp_auth) {
                s_events.on_mk_rtsp_auth((mk_media_info) &args,
                                         realm.c_str(),
                                         user_name.c_str(),
                                         must_no_encrypt,
                                         (mk_rtsp_auth_invoker) &invoker,
                                         (mk_sock_info) &sender);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastMediaPublish,[](BroadcastMediaPublishArgs){
            if (s_events.on_mk_media_publish) {
                s_events.on_mk_media_publish((mk_media_info) &args,
                                             (mk_publish_auth_invoker) &invoker,
                                             (mk_sock_info) &sender);
            } else {
                invoker("", ProtocolOption());
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastMediaPlayed,[](BroadcastMediaPlayedArgs){
            if (s_events.on_mk_media_play) {
                s_events.on_mk_media_play((mk_media_info) &args,
                                          (mk_auth_invoker) &invoker,
                                          (mk_sock_info) &sender);
            }else{
                invoker("");
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastShellLogin,[](BroadcastShellLoginArgs){
            if (s_events.on_mk_shell_login) {
                s_events.on_mk_shell_login(user_name.c_str(),
                                           passwd.c_str(),
                                           (mk_auth_invoker) &invoker,
                                           (mk_sock_info) &sender);
            }else{
                invoker("");
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastFlowReport,[](BroadcastFlowReportArgs){
            if (s_events.on_mk_flow_report) {
                s_events.on_mk_flow_report((mk_media_info) &args,
                                           totalBytes,
                                           totalDuration,
                                           isPlayer,
                                           (mk_sock_info)&sender);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastNotFoundStream,[](BroadcastNotFoundStreamArgs){
            if (s_events.on_mk_media_not_found) {
                if (s_events.on_mk_media_not_found((mk_media_info) &args,
                                                   (mk_sock_info) &sender)) {
                    closePlayer();
                }
            }
        });

        NoticeCenter::Instance().addListener(&s_tag,Broadcast::kBroadcastStreamNoneReader,[](BroadcastStreamNoneReaderArgs){
            if (s_events.on_mk_media_no_reader) {
                s_events.on_mk_media_no_reader((mk_media_source) &sender);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag, EventChannel::kBroadcastLogEvent,[](BroadcastLogEventArgs){
            if (s_events.on_mk_log) {
                auto log = ctx->str();
                s_events.on_mk_log((int) ctx->_level, ctx->_file.data(), ctx->_line, ctx->_function.data(), log.data());
            }
        });

        NoticeCenter::Instance().addListener(&s_tag, Broadcast::kBroadcastSendRtpStopped,[](BroadcastSendRtpStoppedArgs){
            if (s_events.on_mk_media_send_rtp_stop) {
                s_events.on_mk_media_send_rtp_stop(sender.getMediaTuple().vhost.c_str(), sender.getMediaTuple().app.c_str(),
                                                   sender.getMediaTuple().stream.c_str(), ssrc.c_str(), ex.getErrCode(), ex.what());
            }
        });
#ifdef ENABLE_WEBRTC
        NoticeCenter::Instance().addListener(&s_tag, Broadcast::kBroadcastRtcSctpConnecting,[](BroadcastRtcSctpConnectArgs){
            if (s_events.on_mk_rtc_sctp_connecting) {
                s_events.on_mk_rtc_sctp_connecting((mk_rtc_transport)&sender);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag, Broadcast::kBroadcastRtcSctpConnected,[](BroadcastRtcSctpConnectArgs){
            if (s_events.on_mk_rtc_sctp_connected) {
                s_events.on_mk_rtc_sctp_connected((mk_rtc_transport)&sender);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag, Broadcast::kBroadcastRtcSctpFailed,[](BroadcastRtcSctpConnectArgs){
            if (s_events.on_mk_rtc_sctp_failed) {
                s_events.on_mk_rtc_sctp_failed((mk_rtc_transport)&sender);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag, Broadcast::kBroadcastRtcSctpClosed,[](BroadcastRtcSctpConnectArgs){
            if (s_events.on_mk_rtc_sctp_closed) {
                s_events.on_mk_rtc_sctp_closed((mk_rtc_transport)&sender);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag, Broadcast::kBroadcastRtcSctpSend,[](BroadcastRtcSctpSendArgs){
            if (s_events.on_mk_rtc_sctp_send) {
                s_events.on_mk_rtc_sctp_send((mk_rtc_transport)&sender, data, len);
            }
        });

        NoticeCenter::Instance().addListener(&s_tag, Broadcast::kBroadcastRtcSctpReceived,[](BroadcastRtcSctpReceivedArgs){
            if (s_events.on_mk_rtc_sctp_received) {
                s_events.on_mk_rtc_sctp_received((mk_rtc_transport)&sender, streamId, ppid, msg, len);
            }
        });
#endif
    });

}
