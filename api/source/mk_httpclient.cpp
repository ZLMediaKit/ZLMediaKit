﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_httpclient.h"

#include "Util/logger.h"
#include "Http/HttpDownloader.h"
#include "Http/HttpRequester.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

API_EXPORT mk_http_downloader API_CALL mk_http_downloader_create() {
    HttpDownloader::Ptr *obj(new HttpDownloader::Ptr(new HttpDownloader()));
    return (mk_http_downloader) obj;
}

API_EXPORT void API_CALL mk_http_downloader_release(mk_http_downloader ctx) {
    assert(ctx);
    HttpDownloader::Ptr *obj = (HttpDownloader::Ptr *) ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_http_downloader_start(mk_http_downloader ctx, const char *url, const char *file, on_mk_download_complete cb, void *user_data) {
    mk_http_downloader_start2(ctx, url, file, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_http_downloader_start2(mk_http_downloader ctx, const char *url, const char *file, on_mk_download_complete cb, void *user_data, on_user_data_free user_data_free) {
    assert(ctx && url && file);
    HttpDownloader::Ptr *obj = (HttpDownloader::Ptr *) ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    (*obj)->setOnResult([cb, ptr](const SockException &ex, const string &filePath) {
        if (cb) {
            cb(ptr.get(), ex.getErrCode(), ex.what(), filePath.data());
        }
    });
    (*obj)->startDownload(url, file, false);
}

///////////////////////////////////////////HttpRequester/////////////////////////////////////////////
API_EXPORT mk_http_requester API_CALL mk_http_requester_create(){
    HttpRequester::Ptr *ret = new HttpRequester::Ptr(new HttpRequester);
    return (mk_http_requester)ret;
}

API_EXPORT void API_CALL mk_http_requester_clear(mk_http_requester ctx){
    assert(ctx);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    (*obj)->clear();
}

API_EXPORT void API_CALL mk_http_requester_release(mk_http_requester ctx){
    assert(ctx);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    delete obj;
}

API_EXPORT void API_CALL mk_http_requester_set_method(mk_http_requester ctx,const char *method){
    assert(ctx);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    (*obj)->setMethod(method);
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

API_EXPORT void API_CALL mk_http_requester_set_body(mk_http_requester ctx, mk_http_body body){
    assert(ctx && body);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    HttpBody::Ptr *body_obj = (HttpBody::Ptr *)body;
    (*obj)->setBody(*body_obj);
}

API_EXPORT void API_CALL mk_http_requester_set_header(mk_http_requester ctx, const char *header[]){
    assert(ctx && header);
    auto header_obj = get_http_header(header);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    (*obj)->setHeader(header_obj);
}

API_EXPORT void API_CALL mk_http_requester_add_header(mk_http_requester ctx,const char *key,const char *value,int force){
    assert(ctx && key && value);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    (*obj)->addHeader(key,value,force);
}

API_EXPORT const char* API_CALL mk_http_requester_get_response_status(mk_http_requester ctx){
    assert(ctx);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    return (*obj)->response().status().c_str();
}

API_EXPORT const char* API_CALL mk_http_requester_get_response_header(mk_http_requester ctx,const char *key){
    assert(ctx);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    return (*obj)->response()[key].c_str();
}

API_EXPORT const char* API_CALL mk_http_requester_get_response_body(mk_http_requester ctx, size_t *length){
    assert(ctx);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    if(length){
       *length = (*obj)->response().content().size();
    }
    return (*obj)->response().content().c_str();
}

API_EXPORT mk_parser API_CALL mk_http_requester_get_response(mk_http_requester ctx){
    assert(ctx);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    return (mk_parser)&((*obj)->response());
}

API_EXPORT void API_CALL mk_http_requester_set_cb(mk_http_requester ctx,on_mk_http_requester_complete cb, void *user_data) {
    mk_http_requester_set_cb2(ctx, cb, user_data, nullptr);
}

API_EXPORT void API_CALL mk_http_requester_set_cb2(mk_http_requester ctx,on_mk_http_requester_complete cb, void *user_data, on_user_data_free user_data_free) {
    assert(ctx && cb);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    std::shared_ptr<void> ptr(user_data, user_data_free ? user_data_free : [](void *) {});
    (*obj)->setOnResult([cb, ptr](const SockException &ex, const Parser &res) {
        cb(ptr.get(), ex.getErrCode(), ex.what());
    });
}

API_EXPORT void API_CALL mk_http_requester_start(mk_http_requester ctx,const char *url, float timeout_second){
    assert(ctx && url && url[0] && timeout_second > 0);
    HttpRequester::Ptr *obj = (HttpRequester::Ptr *)ctx;
    (*obj)->setCompleteTimeout(timeout_second * 1000);
    (*obj)->sendRequest(url);
}

