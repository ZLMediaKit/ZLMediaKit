/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdarg>
#include <cassert>

#include "mk_util.h"
#include "Util/util.h"
#include "Util/mini.h"
#include "Util/logger.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

#ifndef _WIN32
#define _strdup strdup
#endif

API_EXPORT void API_CALL mk_free(void *ptr) {
    free(ptr);
}

API_EXPORT char* API_CALL mk_util_get_exe_path(){
    return _strdup(exePath().data());
}

API_EXPORT char* API_CALL mk_util_get_exe_dir(const char *relative_path){
    if(relative_path){
        return _strdup((exeDir() + relative_path).data());
    }
    return _strdup(exeDir().data());
}

API_EXPORT uint64_t API_CALL mk_util_get_current_millisecond(){
    return getCurrentMillisecond();
}

API_EXPORT char* API_CALL mk_util_get_current_time_string(const char *fmt){
    assert(fmt);
    return _strdup(getTimeStr(fmt).data());
}

API_EXPORT char* API_CALL mk_util_hex_dump(const void *buf, int len){
    assert(buf && len > 0);
    return _strdup(hexdump(buf,len).data());
}

API_EXPORT mk_ini API_CALL mk_ini_create() {
    return (mk_ini)new mINI;
}

API_EXPORT mk_ini API_CALL mk_ini_default() {
    return (mk_ini)&(mINI::Instance());
}

static void emit_ini_file_reload(mk_ini ini) {
    if (ini == mk_ini_default()) {
        // 广播配置文件热加载
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastReloadConfig);
    }
}

API_EXPORT void API_CALL mk_ini_load_string(mk_ini ini, const char *str) {
    assert(str);
    auto ptr = (mINI *)ini;
    ptr->parse(str);
    emit_ini_file_reload(ini);
}

API_EXPORT void API_CALL mk_ini_load_file(mk_ini ini, const char *file) {
    assert(file);
    auto ptr = (mINI *)ini;
    ptr->parseFile(file);
    emit_ini_file_reload(ini);
}

API_EXPORT void API_CALL mk_ini_release(mk_ini ini) {
    assert(ini);
    delete (mINI *)ini;
}

API_EXPORT void API_CALL mk_ini_set_option(mk_ini ini, const char *key, const char *value) {
    assert(ini && key && value);
    auto ptr = (mINI *)ini;
    (*ptr)[key] = value;
    emit_ini_file_reload(ini);
}

API_EXPORT void API_CALL mk_ini_set_option_int(mk_ini ini, const char *key, int value) {
    assert(ini && key);
    auto ptr = (mINI *)ini;
    (*ptr)[key] = value;
    emit_ini_file_reload(ini);
}

API_EXPORT const char *API_CALL mk_ini_get_option(mk_ini ini, const char *key) {
    assert(ini && key);
    auto ptr = (mINI *)ini;
    auto it = ptr->find(key);
    if (it == ptr->end()) {
        return nullptr;
    }
    return it->second.data();
}

API_EXPORT int API_CALL mk_ini_del_option(mk_ini ini, const char *key) {
    assert(ini && key);
    auto ptr = (mINI *)ini;
    auto it = ptr->find(key);
    if (it == ptr->end()) {
        return false;
    }
    ptr->erase(it);
    emit_ini_file_reload(ini);
    return true;
}

API_EXPORT char *API_CALL mk_ini_dump_string(mk_ini ini) {
    assert(ini);
    auto ptr = (mINI *)ini;
    return _strdup(ptr->dump().data());
}

API_EXPORT void API_CALL mk_ini_dump_file(mk_ini ini, const char *file) {
    assert(ini && file);
    auto ptr = (mINI *)ini;
    ptr->dumpFile(file);
}

API_EXPORT void API_CALL mk_log_printf(int level, const char *file, const char *function, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    toolkit::LoggerWrapper::printLogV(getLogger(), level, file, function, line, fmt, ap);
    va_end(ap);
}

