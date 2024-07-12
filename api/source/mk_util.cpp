/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdarg>
#include <cassert>

#include "mk_util.h"
#include "Util/util.h"
#include "Util/mini.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Poller/EventPoller.h"
#include "Thread/WorkThreadPool.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

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
        NOTICE_EMIT(BroadcastReloadConfigArgs, Broadcast::kBroadcastReloadConfig);
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

extern uint64_t getTotalMemUsage();
extern uint64_t getTotalMemBlock();
extern uint64_t getThisThreadMemUsage();
extern uint64_t getThisThreadMemBlock();
extern std::vector<size_t> getBlockTypeSize();
extern uint64_t getTotalMemBlockByType(int type);
extern uint64_t getThisThreadMemBlockByType(int type);

namespace mediakit {
class MediaSource;
class MultiMediaSourceMuxer;
class FrameImp;
class Frame;
class RtpPacket;
class RtmpPacket;
} // namespace mediakit

namespace toolkit {
class TcpServer;
class TcpSession;
class UdpServer;
class UdpSession;
class TcpClient;
class Socket;
class Buffer;
class BufferRaw;
class BufferLikeString;
class BufferList;
} // namespace toolkit

API_EXPORT void API_CALL mk_get_statistic(on_mk_get_statistic_cb func, void *user_data, on_user_data_free free_cb) {
    assert(func);
    std::shared_ptr<void> data(user_data, free_cb);
    auto cb = [func, data](const toolkit::mINI &ini) { func(data.get(), (mk_ini)&ini); };
    auto obj = std::make_shared<toolkit::mINI>();
    auto &val = *obj;

    val["object.MediaSource"] = ObjectStatistic<MediaSource>::count();
    val["object.MultiMediaSourceMuxer"] = ObjectStatistic<MultiMediaSourceMuxer>::count();

    val["object.TcpServer"] = ObjectStatistic<TcpServer>::count();
    val["object.TcpSession"] = ObjectStatistic<TcpSession>::count();
    val["object.UdpServer"] = ObjectStatistic<UdpServer>::count();
    val["object.UdpSession"] = ObjectStatistic<UdpSession>::count();
    val["object.TcpClient"] = ObjectStatistic<TcpClient>::count();
    val["object.Socket"] = ObjectStatistic<Socket>::count();

    val["object.FrameImp"] = ObjectStatistic<FrameImp>::count();
    val["object.Frame"] = ObjectStatistic<Frame>::count();

    val["object.Buffer"] = ObjectStatistic<Buffer>::count();
    val["object.BufferRaw"] = ObjectStatistic<BufferRaw>::count();
    val["object.BufferLikeString"] = ObjectStatistic<BufferLikeString>::count();
    val["object.BufferList"] = ObjectStatistic<BufferList>::count();

    val["object.RtpPacket"] = ObjectStatistic<RtpPacket>::count();
    val["object.RtmpPacket"] = ObjectStatistic<RtmpPacket>::count();
#ifdef ENABLE_MEM_DEBUG
    auto bytes = getTotalMemUsage();
    val["memory.memUsage"] = bytes;
    val["memory.memUsageMB"] = (int)(bytes / 1024 / 1024);
    val["memory.memBlock"] = getTotalMemBlock();
    static auto block_type_size = getBlockTypeSize();
    {
        int i = 0;
        string str;
        size_t last = 0;
        for (auto sz : block_type_size) {
            str.append(to_string(last) + "~" + to_string(sz) + ":" + to_string(getTotalMemBlockByType(i++)) + ";");
            last = sz;
        }
        str.pop_back();
        val["memory.memBlockTypeCount"] = str;
    }
#endif

    auto thread_size = EventPollerPool::Instance().getExecutorSize() + WorkThreadPool::Instance().getExecutorSize();
    std::shared_ptr<vector<toolkit::mINI>> thread_mem_info = std::make_shared<vector<toolkit::mINI>>(thread_size);

    shared_ptr<void> finished(nullptr, [thread_mem_info, cb, obj](void *) {
        for (auto &val : *thread_mem_info) {
            auto thread_name = val["name"];
            replace(thread_name, "...", "~~~");
            auto prefix = "thread-" + thread_name + ".";
            for (auto &pr : val) {
                (*obj).emplace(prefix + pr.first, std::move(pr.second));
            }
        }
        // 触发回调
        cb(*obj);
    });

    auto pos = 0;
    auto lambda = [&](const TaskExecutor::Ptr &executor) {
        auto &val = (*thread_mem_info)[pos++];
        val["load"] = executor->load();
        Ticker ticker;
        executor->async([finished, &val, ticker]() {
            val["name"] = getThreadName();
            val["delay"] = ticker.elapsedTime();
#ifdef ENABLE_MEM_DEBUG
            auto bytes = getThisThreadMemUsage();
            val["memUsage"] = bytes;
            val["memUsageMB"] = bytes / 1024 / 1024;
            val["memBlock"] = getThisThreadMemBlock();
            {
                int i = 0;
                string str;
                size_t last = 0;
                for (auto sz : block_type_size) {
                    str.append(to_string(last) + "~" + to_string(sz) + ":" + to_string(getThisThreadMemBlockByType(i++)) + ";");
                    last = sz;
                }
                str.pop_back();
                val["memBlockTypeCount"] = str;
            }
#endif
        });
    };
    EventPollerPool::Instance().for_each(lambda);
    WorkThreadPool::Instance().for_each(lambda);
}

API_EXPORT void API_CALL mk_log_printf(int level, const char *file, const char *function, int line, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    toolkit::LoggerWrapper::printLogV(getLogger(), level, file, function, line, fmt, ap);
    va_end(ap);
}

