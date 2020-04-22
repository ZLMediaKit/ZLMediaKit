/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
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
using namespace mediakit;

///////////////////////////////////////////MP4Info/////////////////////////////////////////////
API_EXPORT uint64_t API_CALL mk_mp4_info_get_start_time(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->ui64StartedTime;
}

API_EXPORT uint64_t API_CALL mk_mp4_info_get_time_len(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->ui64TimeLen;
}

API_EXPORT uint64_t API_CALL mk_mp4_info_get_file_size(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->ui64FileSize;
}

API_EXPORT const char* API_CALL mk_mp4_info_get_file_path(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->strFilePath.c_str();
}

API_EXPORT const char* API_CALL mk_mp4_info_get_file_name(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->strFileName.c_str();
}

API_EXPORT const char* API_CALL mk_mp4_info_get_folder(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->strFolder.c_str();
}

API_EXPORT const char* API_CALL mk_mp4_info_get_url(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->strUrl.c_str();
}

API_EXPORT const char* API_CALL mk_mp4_info_get_vhost(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->strVhost.c_str();
}

API_EXPORT const char* API_CALL mk_mp4_info_get_app(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->strAppName.c_str();
}

API_EXPORT const char* API_CALL mk_mp4_info_get_stream(const mk_mp4_info ctx){
    assert(ctx);
    MP4Info *info = (MP4Info *)ctx;
    return info->strStreamId.c_str();
}

///////////////////////////////////////////Parser/////////////////////////////////////////////
API_EXPORT const char* API_CALL mk_parser_get_method(const mk_parser ctx){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    return parser->Method().c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_url(const mk_parser ctx){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    return parser->Url().c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_full_url(const mk_parser ctx){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    return parser->FullUrl().c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_url_params(const mk_parser ctx){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    return parser->Params().c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_url_param(const mk_parser ctx,const char *key){
    assert(ctx && key);
    Parser *parser = (Parser *)ctx;
    return parser->getUrlArgs()[key].c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_tail(const mk_parser ctx){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    return parser->Tail().c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_header(const mk_parser ctx,const char *key){
    assert(ctx && key);
    Parser *parser = (Parser *)ctx;
    return parser->getHeader()[key].c_str();
}
API_EXPORT const char* API_CALL mk_parser_get_content(const mk_parser ctx, int *length){
    assert(ctx);
    Parser *parser = (Parser *)ctx;
    if(length){
        *length = parser->Content().size();
    }
    return parser->Content().c_str();
}

///////////////////////////////////////////MediaInfo/////////////////////////////////////////////
API_EXPORT const char* API_CALL mk_media_info_get_params(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->_param_strs.c_str();
}

API_EXPORT const char* API_CALL mk_media_info_get_schema(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->_schema.c_str();
}

API_EXPORT const char* API_CALL mk_media_info_get_vhost(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->_vhost.c_str();
}

API_EXPORT const char* API_CALL mk_media_info_get_host(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->_host.c_str();
}

API_EXPORT uint16_t API_CALL mk_media_info_get_port(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return std::stoi(info->_port);
}

API_EXPORT const char* API_CALL mk_media_info_get_app(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->_app.c_str();
}

API_EXPORT const char* API_CALL mk_media_info_get_stream(const mk_media_info ctx){
    assert(ctx);
    MediaInfo *info = (MediaInfo *)ctx;
    return info->_streamid.c_str();
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
    return src->getVhost().c_str();
}
API_EXPORT const char* API_CALL mk_media_source_get_app(const mk_media_source ctx){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getApp().c_str();
}
API_EXPORT const char* API_CALL mk_media_source_get_stream(const mk_media_source ctx){
    assert(ctx);
    MediaSource *src = (MediaSource *)ctx;
    return src->getId().c_str();
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

API_EXPORT void API_CALL mk_media_source_find(const char *schema,
                                              const char *vhost,
                                              const char *app,
                                              const char *stream,
                                              void *user_data,
                                              on_mk_media_source_find_cb cb) {
    assert(schema && vhost && app && stream && cb);
    auto src = MediaSource::find(schema, vhost, app, stream);
    cb(user_data, src.get());
}

API_EXPORT void API_CALL mk_media_source_for_each(void *user_data, on_mk_media_source_find_cb cb){
    assert(cb);
    MediaSource::for_each_media([&](const MediaSource::Ptr &src){
        cb(user_data,src.get());
    });
}

///////////////////////////////////////////HttpBody/////////////////////////////////////////////
API_EXPORT mk_http_body API_CALL mk_http_body_from_string(const char *str,int len){
    assert(str);
    if(!len){
        len = strlen(str);
    }
    return new HttpBody::Ptr(new HttpStringBody(string(str,len)));
}

API_EXPORT mk_http_body API_CALL mk_http_body_from_file(const char *file_path){
    assert(file_path);
    return new HttpBody::Ptr(new HttpFileBody(file_path));
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
    return std::move(header);
}

API_EXPORT mk_http_body API_CALL mk_http_body_from_multi_form(const char *key_val[],const char *file_path){
    assert(key_val && file_path);
    return new HttpBody::Ptr(new HttpMultiFormBody(get_http_header<HttpArgs>(key_val),file_path));
}

API_EXPORT void API_CALL mk_http_body_release(mk_http_body ctx){
    assert(ctx);
    HttpBody::Ptr *ptr = (HttpBody::Ptr *)ctx;
    delete ptr;
}

///////////////////////////////////////////HttpResponseInvoker/////////////////////////////////////////////
API_EXPORT void API_CALL mk_http_response_invoker_do_string(const mk_http_response_invoker ctx,
                                                            const char *response_code,
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
                                                     const char *response_code,
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
    return new  HttpSession::HttpResponseInvoker (*invoker);
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
    return new HttpSession::HttpAccessPathInvoker(*invoker);
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
    return new RtspSession::onGetRealm (*invoker);
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
    return new RtspSession::onAuth(*invoker);
}

API_EXPORT void API_CALL mk_rtsp_auth_invoker_clone_release(const mk_rtsp_auth_invoker ctx){
    assert(ctx);
    RtspSession::onAuth *invoker = (RtspSession::onAuth *)ctx;
    delete invoker;
}

///////////////////////////////////////////Broadcast::PublishAuthInvoker/////////////////////////////////////////////
API_EXPORT void API_CALL mk_publish_auth_invoker_do(const mk_publish_auth_invoker ctx,
                                                    const char *err_msg,
                                                    int enable_rtxp,
                                                    int enable_hls,
                                                    int enable_mp4){
    assert(ctx);
    Broadcast::PublishAuthInvoker *invoker = (Broadcast::PublishAuthInvoker *)ctx;
    (*invoker)(err_msg ? err_msg : "", enable_rtxp, enable_hls, enable_mp4);
}

API_EXPORT mk_publish_auth_invoker API_CALL mk_publish_auth_invoker_clone(const mk_publish_auth_invoker ctx){
    assert(ctx);
    Broadcast::PublishAuthInvoker *invoker = (Broadcast::PublishAuthInvoker *)ctx;
    return new Broadcast::PublishAuthInvoker(*invoker);
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
    return new Broadcast::AuthInvoker(*invoker);
}

API_EXPORT void API_CALL mk_auth_invoker_clone_release(const mk_auth_invoker ctx){
    assert(ctx);
    Broadcast::AuthInvoker *invoker = (Broadcast::AuthInvoker *)ctx;
    delete invoker;
}