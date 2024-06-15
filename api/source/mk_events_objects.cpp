﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <assert.h>
#include "mk_events_objects.h"
#include "Common/config.h"
#include "Record/MP4Recorder.h"
#include "Http/HttpSession.h"
#include "Http/HttpBody.h"

#include "Http/HttpClient.h"
#include "Rtsp/RtspSession.h"

#ifdef ENABLE_WEBRTC
#include "webrtc/WebRtcTransport.h"
#endif

using namespace toolkit;
using namespace mediakit;

///////////////////////////////////////////RecordInfo/////////////////////////////////////////////
API_EXPORT uint64_t API_CALL mk_record_info_get_start_time(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->start_time;
}

API_EXPORT float API_CALL mk_record_info_get_time_len(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->time_len;
}

API_EXPORT size_t API_CALL mk_record_info_get_file_size(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->file_size;
}

API_EXPORT const char *API_CALL mk_record_info_get_file_path(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->file_path.c_str();
}

API_EXPORT const char *API_CALL mk_record_info_get_file_name(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->file_name.c_str();
}

API_EXPORT const char *API_CALL mk_record_info_get_folder(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->folder.c_str();
}

API_EXPORT const char *API_CALL mk_record_info_get_url(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->url.c_str();
}

API_EXPORT const char *API_CALL mk_record_info_get_vhost(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->vhost.c_str();
}

API_EXPORT const char *API_CALL mk_record_info_get_app(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->app.c_str();
}

API_EXPORT const char *API_CALL mk_record_info_get_stream(const mk_record_info ctx) {
    assert(ctx);
    RecordInfo *info = (RecordInfo *)ctx;
    return info->stream.c_str();
}

///////////////////////////////////////////Parser/////////////////////////////////////////////
API_EXPORT const char* API_CALL mk_parser_get_method(const mk_parser ctx){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    return parser->method().c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_url(const mk_parser ctx){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    return parser->url().c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_url_params(const mk_parser ctx){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    return parser->params().c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_url_param(const mk_parser ctx,const char *key){
    assert(ctx && key);
    Parser *parser = (Parser *)ctx;
    return parser->getUrlArgs()[key].c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_tail(const mk_parser ctx){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    return parser->protocol().c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_header(const mk_parser ctx,const char *key){
    assert(ctx && key);
    Parser *parser = (Parser *)ctx;
    return parser->getHeader()[key].c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_content(const mk_parser ctx, size_t *length){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    if(length){
        *length = parser->content().size();
    }
    return parser->content().c_str();
}

///////////////////////////////////////////MediaInfo/////////////////////////////////////////////
API_EXPORT const char* API_CALL mk_media_info_get_params(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->params.c_str();
}

API_EXPORT const char* API_CALL mk_media_info_get_schema(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->schema.c_str();
}

API_EXPORT const char* API_CALL mk_media_info_get_vhost(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->vhost.c_str();
}

API_EXPORT const char* API_CALL mk_media_info_get_host(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->host.c_str();
}

API_EXPORT uint16_t API_CALL mk_media_info_get_port(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->port;
}

API_EXPORT const char* API_CALL mk_media_info_get_app(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->app.c_str();
}

API_EXPORT const char* API_CALL mk_media_info_get_stream(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->stream.c_str();
}

///////////////////////////////////////////MediaSource/////////////////////////////////////////////
API_EXPORT const char* API_CALL mk_media_source_get_schema(const mk_media_source ctx){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getSchema().c_str();
}
API_EXPORT const char* API_CALL mk_media_source_get_vhost(const mk_media_source ctx){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getMediaTuple().vhost.c_str();
}
API_EXPORT const char* API_CALL mk_media_source_get_app(const mk_media_source ctx){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getMediaTuple().app.c_str();
}
API_EXPORT const char* API_CALL mk_media_source_get_stream(const mk_media_source ctx){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getMediaTuple().stream.c_str();
}
API_EXPORT int API_CALL mk_media_source_get_reader_count(const mk_media_source ctx){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->readerCount();
}

API_EXPORT int API_CALL mk_media_source_get_total_reader_count(const mk_media_source ctx){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->totalReaderCount();
}

API_EXPORT int API_CALL mk_media_source_get_track_count(const mk_media_source ctx) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getTracks(false).size();
}

API_EXPORT mk_track API_CALL mk_media_source_get_track(const mk_media_source ctx, int index) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    auto tracks = src->getTracks(false);
    if (index < 0 && index >= tracks.size()) {
        return nullptr;
    }
    return (mk_track) new Track::Ptr(std::move(tracks[index]));
}

API_EXPORT float API_CALL mk_media_source_get_track_loss(const mk_media_source ctx, const mk_track track) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    // rtp推流只有一个统计器，但是可能有多个track，如果短时间多次获取间隔丢包率，第二次会获取为-1
    return src->getLossRate((*((Track::Ptr *)track))->getTrackType());
}

API_EXPORT int API_CALL mk_media_source_broadcast_msg(const mk_media_source ctx, const char *msg, size_t len) {
    assert(ctx && msg && len);
    MediaSource *src = (MediaSource *)ctx;

    Any any;
    Buffer::Ptr buffer = std::make_shared<BufferLikeString>(std::string(msg, len));
    any.set(std::move(buffer));
    return src->broadcastMessage(any);
}

API_EXPORT const char* API_CALL mk_media_source_get_origin_url(const mk_media_source ctx) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return _strdup(src->getOriginUrl().c_str());
}

API_EXPORT  int API_CALL mk_media_source_get_origin_type(const mk_media_source ctx) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return static_cast<int>(src->getOriginType());
}

API_EXPORT const char* API_CALL mk_media_source_get_origin_type_str(const mk_media_source ctx) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return _strdup(getOriginTypeString(src->getOriginType()).c_str());
}

API_EXPORT uint64_t API_CALL mk_media_source_get_create_stamp(const mk_media_source ctx) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getCreateStamp();
}

API_EXPORT int API_CALL mk_media_source_is_recording(const mk_media_source ctx,int type) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->isRecording((Recorder::type)type);
}

API_EXPORT int API_CALL mk_media_source_get_bytes_speed(const mk_media_source ctx) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getBytesSpeed();
}

API_EXPORT uint64_t API_CALL mk_media_source_get_alive_second(const mk_media_source ctx) {
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getAliveSecond();
}


API_EXPORT int API_CALL mk_media_source_close(const mk_media_source ctx,int force){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->close(force);
}
API_EXPORT int API_CALL mk_media_source_seek_to(const mk_media_source ctx,uint32_t stamp){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->seekTo(stamp);
}
API_EXPORT void API_CALL mk_media_source_start_send_rtp(const mk_media_source ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int is_udp, on_mk_media_source_send_rtp_result cb, void *user_data) {
    mk_media_source_start_send_rtp2(ctx, dst_url, dst_port, ssrc, is_udp, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_media_source_start_send_rtp2(const mk_media_source ctx, const char *dst_url, uint16_t dst_port, const char *ssrc, int is_udp, on_mk_media_source_send_rtp_result cb, void *user_data, on_user_data_free user_data_free){
    assert(ctx && dst_url && ssrc);
    MediaSource *src = (MediaSource *)ctx;

    MediaSourceEvent::SendRtpArgs args;
    args.dst_url = dst_url;
    args.dst_port = dst_port;
    args.ssrc = ssrc;
    args.is_udp = is_udp;

    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    src->startSendRtp(args, [cb, ptr](uint16_t local_port, const SockException &ex){
        if (cb) {
            cb(ptr.get(), local_port, ex.getErrCode(), ex.what());
        }
    });
}

API_EXPORT int API_CALL mk_media_source_stop_send_rtp(const mk_media_source ctx){
    assert(ctx);
    MediaSource *src = (MediaSource *) ctx;
    return src->stopSendRtp("");
}

API_EXPORT void API_CALL mk_media_source_find(const char *schema,
                                              const char *vhost,
                                              const char *app,
                                              const char *stream,
                                              int from_mp4,
                                              void *user_data,
                                              on_mk_media_source_find_cb cb) {
    assert(schema && vhost && app && stream && cb);
    auto src = MediaSource::find(schema, vhost, app, stream, from_mp4);
    cb(user_data, (mk_media_source)src.get());
}

API_EXPORT mk_media_source API_CALL mk_media_source_find2(const char *schema,
                                                          const char *vhost,
                                                          const char *app,
                                                          const char *stream,
                                                          int from_mp4) {
    assert(schema && vhost && app && stream);
    auto src = MediaSource::find(schema, vhost, app, stream, from_mp4);
    return (mk_media_source)src.get();
}

API_EXPORT void API_CALL mk_media_source_for_each(void *user_data, on_mk_media_source_find_cb cb, const char *schema,
                                                  const char *vhost, const char *app, const char *stream) {
    assert(cb);
    MediaSource::for_each_media([&](const MediaSource::Ptr &src) {
        cb(user_data, (mk_media_source)src.get());
    }, schema ? schema : "", vhost ? vhost : "", app ? app : "", stream ? stream : "");
}

///////////////////////////////////////////HttpBody/////////////////////////////////////////////

API_EXPORT mk_http_body API_CALL mk_http_body_from_string(const char *str, size_t len){
    assert(str);
    if(!len){
        len = strlen(str);
    }
    return (mk_http_body)new HttpBody::Ptr(new HttpStringBody(std::string(str, len)));
}

API_EXPORT mk_http_body API_CALL mk_http_body_from_buffer(mk_buffer buffer) {
    assert(buffer);
    return (mk_http_body)new HttpBody::Ptr(new HttpBufferBody(*((Buffer::Ptr *) buffer)));
}

API_EXPORT mk_http_body API_CALL mk_http_body_from_file(const char *file_path){
    assert(file_path);
    return (mk_http_body)new HttpBody::Ptr(new HttpFileBody(file_path));
}

template <typename C = StrCaseMap>
static C get_http_header( const char *response_header[]){
    C header;
    for (int i = 0; response_header[i] != NULL;) {
        auto key = response_header[i];
        auto value = response_header[i + 1];
        if (key && value) {
            i += 2;
            header.emplace(key,value);
            continue;
        }
        break;
    }
    return header;
}

API_EXPORT mk_http_body API_CALL mk_http_body_from_multi_form(const char *key_val[],const char *file_path){
    assert(key_val && file_path);
    return (mk_http_body)new HttpBody::Ptr(new HttpMultiFormBody(get_http_header<HttpArgs>(key_val),file_path));
}

API_EXPORT void API_CALL mk_http_body_release(mk_http_body ctx){
    assert(ctx);
    HttpBody::Ptr *ptr = (HttpBody::Ptr *)ctx;
    delete ptr;
}

///////////////////////////////////////////HttpResponseInvoker/////////////////////////////////////////////
API_EXPORT void API_CALL mk_http_response_invoker_do_string(const mk_http_response_invoker ctx,
                                                            int response_code,
                                                            const char **response_header,
                                                            const char *response_content){
    assert(ctx && response_code && response_header && response_content);
    auto header = get_http_header(response_header);
    HttpSession::HttpResponseInvoker *invoker = (HttpSession::HttpResponseInvoker *)ctx;
    (*invoker)(response_code,header,response_content);
}

API_EXPORT void API_CALL mk_http_response_invoker_do_file(const mk_http_response_invoker ctx,
                                                          const mk_parser request_parser,
                                                          const char *response_header[],
                                                          const char *response_file_path){
    assert(ctx && request_parser && response_header && response_file_path);
    auto header = get_http_header(response_header);
    HttpSession::HttpResponseInvoker *invoker = (HttpSession::HttpResponseInvoker *)ctx;
    (*invoker).responseFile(((Parser *) (request_parser))->getHeader(), header, response_file_path);
}

API_EXPORT void API_CALL mk_http_response_invoker_do(const mk_http_response_invoker ctx,
                                                     int response_code,
                                                     const char **response_header,
                                                     const mk_http_body response_body){
    assert(ctx && response_code && response_header && response_body);
    auto header = get_http_header(response_header);
    HttpSession::HttpResponseInvoker *invoker = (HttpSession::HttpResponseInvoker *)ctx;
    HttpBody::Ptr *body = (HttpBody::Ptr*) response_body;
    (*invoker)(response_code,header,*body);
}

API_EXPORT mk_http_response_invoker API_CALL mk_http_response_invoker_clone(const mk_http_response_invoker ctx){
    assert(ctx);
    HttpSession::HttpResponseInvoker *invoker = (HttpSession::HttpResponseInvoker *)ctx;
    return (mk_http_response_invoker)new HttpSession::HttpResponseInvoker (*invoker);
}

API_EXPORT void API_CALL mk_http_response_invoker_clone_release(const mk_http_response_invoker ctx){
    assert(ctx);
    HttpSession::HttpResponseInvoker *invoker = (HttpSession::HttpResponseInvoker *)ctx;
    delete invoker;
}

///////////////////////////////////////////HttpAccessPathInvoker/////////////////////////////////////////////
API_EXPORT void API_CALL mk_http_access_path_invoker_do(const mk_http_access_path_invoker ctx,
                                                        const char *err_msg,
                                                        const char *access_path,
                                                        int cookie_life_second){
    assert(ctx);
    HttpSession::HttpAccessPathInvoker *invoker = (HttpSession::HttpAccessPathInvoker *)ctx;
    (*invoker)(err_msg ? err_msg : "",
              access_path? access_path : "",
              cookie_life_second);
}

API_EXPORT mk_http_access_path_invoker API_CALL mk_http_access_path_invoker_clone(const mk_http_access_path_invoker ctx){
    assert(ctx);
    HttpSession::HttpAccessPathInvoker *invoker = (HttpSession::HttpAccessPathInvoker *)ctx;
    return (mk_http_access_path_invoker)new HttpSession::HttpAccessPathInvoker(*invoker);
}

API_EXPORT void API_CALL mk_http_access_path_invoker_clone_release(const mk_http_access_path_invoker ctx){
    assert(ctx);
    HttpSession::HttpAccessPathInvoker *invoker = (HttpSession::HttpAccessPathInvoker *)ctx;
    delete invoker;
}

///////////////////////////////////////////RtspSession::onGetRealm/////////////////////////////////////////////
API_EXPORT void API_CALL mk_rtsp_get_realm_invoker_do(const mk_rtsp_get_realm_invoker ctx,
                                                      const char *realm){
    assert(ctx);
    RtspSession::onGetRealm *invoker = (RtspSession::onGetRealm *)ctx;
    (*invoker)(realm ? realm : "");
}

API_EXPORT mk_rtsp_get_realm_invoker API_CALL mk_rtsp_get_realm_invoker_clone(const mk_rtsp_get_realm_invoker ctx){
    assert(ctx);
    RtspSession::onGetRealm *invoker = (RtspSession::onGetRealm *)ctx;
    return (mk_rtsp_get_realm_invoker)new RtspSession::onGetRealm (*invoker);
}

API_EXPORT void API_CALL mk_rtsp_get_realm_invoker_clone_release(const mk_rtsp_get_realm_invoker ctx){
    assert(ctx);
    RtspSession::onGetRealm *invoker = (RtspSession::onGetRealm *)ctx;
    delete invoker;
}

///////////////////////////////////////////RtspSession::onAuth/////////////////////////////////////////////
API_EXPORT void API_CALL mk_rtsp_auth_invoker_do(const mk_rtsp_auth_invoker ctx,
                                                 int encrypted,
                                                 const char *pwd_or_md5){
    assert(ctx);
    RtspSession::onAuth *invoker = (RtspSession::onAuth *)ctx;
    (*invoker)(encrypted, pwd_or_md5 ? pwd_or_md5 : "");
}

API_EXPORT mk_rtsp_auth_invoker API_CALL mk_rtsp_auth_invoker_clone(const mk_rtsp_auth_invoker ctx){
    assert(ctx);
    RtspSession::onAuth *invoker = (RtspSession::onAuth *)ctx;
    return (mk_rtsp_auth_invoker)new RtspSession::onAuth(*invoker);
}

API_EXPORT void API_CALL mk_rtsp_auth_invoker_clone_release(const mk_rtsp_auth_invoker ctx){
    assert(ctx);
    RtspSession::onAuth *invoker = (RtspSession::onAuth *)ctx;
    delete invoker;
}

///////////////////////////////////////////Broadcast::PublishAuthInvoker/////////////////////////////////////////////
API_EXPORT void API_CALL mk_publish_auth_invoker_do(const mk_publish_auth_invoker ctx,
                                                    const char *err_msg,
                                                    int enable_hls,
                                                    int enable_mp4){
    assert(ctx);
    Broadcast::PublishAuthInvoker *invoker = (Broadcast::PublishAuthInvoker *)ctx;
    ProtocolOption option;
    option.enable_hls = enable_hls;
    option.enable_mp4 = enable_mp4;
    (*invoker)(err_msg ? err_msg : "", option);
}

API_EXPORT void API_CALL mk_publish_auth_invoker_do2(const mk_publish_auth_invoker ctx, const char *err_msg, mk_ini ini) {
    assert(ctx);
    Broadcast::PublishAuthInvoker *invoker = (Broadcast::PublishAuthInvoker *)ctx;
    ProtocolOption option(ini ? *((mINI *)ini) : mINI{} );
    (*invoker)(err_msg ? err_msg : "", option);
}

API_EXPORT mk_publish_auth_invoker API_CALL mk_publish_auth_invoker_clone(const mk_publish_auth_invoker ctx){
    assert(ctx);
    Broadcast::PublishAuthInvoker *invoker = (Broadcast::PublishAuthInvoker *)ctx;
    return (mk_publish_auth_invoker)new Broadcast::PublishAuthInvoker(*invoker);
}

API_EXPORT void API_CALL mk_publish_auth_invoker_clone_release(const mk_publish_auth_invoker ctx){
    assert(ctx);
    Broadcast::PublishAuthInvoker *invoker = (Broadcast::PublishAuthInvoker *)ctx;
    delete invoker;
}

///////////////////////////////////////////Broadcast::AuthInvoker/////////////////////////////////////////////
API_EXPORT void API_CALL mk_auth_invoker_do(const mk_auth_invoker ctx, const char *err_msg){
    assert(ctx);
    Broadcast::AuthInvoker *invoker = (Broadcast::AuthInvoker *)ctx;
    (*invoker)(err_msg ? err_msg : "");
}

API_EXPORT mk_auth_invoker API_CALL mk_auth_invoker_clone(const mk_auth_invoker ctx){
    assert(ctx);
    Broadcast::AuthInvoker *invoker = (Broadcast::AuthInvoker *)ctx;
    return (mk_auth_invoker)new Broadcast::AuthInvoker(*invoker);
}

API_EXPORT void API_CALL mk_auth_invoker_clone_release(const mk_auth_invoker ctx){
    assert(ctx);
    Broadcast::AuthInvoker *invoker = (Broadcast::AuthInvoker *)ctx;
    delete invoker;
}

///////////////////////////////////////////WebRtcTransport/////////////////////////////////////////////
API_EXPORT void API_CALL mk_rtc_send_datachannel(const mk_rtc_transport ctx, uint16_t streamId, uint32_t ppid, const char *msg, size_t len) {
#ifdef ENABLE_WEBRTC
    assert(ctx && msg);
    WebRtcTransport *transport = (WebRtcTransport *)ctx;
    std::string msg_str(msg, len);
    std::weak_ptr<WebRtcTransport> weak_trans = transport->shared_from_this();
    transport->getPoller()->async([streamId, ppid, msg_str, weak_trans]() {
        // 切换线程后再操作
        if (auto trans = weak_trans.lock()) {
            trans->sendDatachannel(streamId, ppid, msg_str.c_str(), msg_str.size());
        }
    });
#else
    WarnL << "未启用webrtc功能, 编译时请开启ENABLE_WEBRTC";
#endif
}
