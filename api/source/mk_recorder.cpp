/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_recorder.h"
#include "Rtmp/FlvMuxer.h"
#include "Record/Recorder.h"
#include "Record/MP4Reader.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

API_EXPORT mk_flv_recorder API_CALL mk_flv_recorder_create(){
    FlvRecorder::Ptr *ret = new FlvRecorder::Ptr(new FlvRecorder);
    return (mk_flv_recorder)ret;
}
API_EXPORT void API_CALL mk_flv_recorder_release(mk_flv_recorder ctx){
    assert(ctx);
    FlvRecorder::Ptr *record = (FlvRecorder::Ptr *)(ctx);
    delete record;
}
API_EXPORT int API_CALL mk_flv_recorder_start(mk_flv_recorder ctx, const char *vhost, const char *app, const char *stream, const char *file_path){
    assert(ctx && vhost && app && stream && file_path);
    try {
        FlvRecorder::Ptr *record = (FlvRecorder::Ptr *)(ctx);
        (*record)->startRecord(EventPollerPool::Instance().getPoller(),vhost,app,stream,file_path);
        return 0;
    }catch (std::exception &ex){
        WarnL << ex.what();
        return -1;
    }
}

// /////////////////////////////////////////hls/mp4录制/////////////////////////////////////////////  [AUTO-TRANSLATED:99c61c68]
// /////////////////////////////////////////hls/mp4 recording/////////////////////////////////////////////

static inline bool isRecording(Recorder::type type, const string &vhost, const string &app, const string &stream_id){
    auto src = MediaSource::find(vhost, app, stream_id);
    if(!src){
        return false;
    }
    return src->isRecording(type);
}

static inline bool startRecord(Recorder::type type, const string &vhost, const string &app, const string &stream_id, const string &customized_path, size_t max_second) {
    auto src = MediaSource::find(vhost, app, stream_id);
    if (!src) {
        WarnL << "未找到相关的MediaSource,startRecord失败:" << vhost << "/" << app << "/" << stream_id;
        return false;
    }
    bool ret;
    src->getOwnerPoller()->sync([&]() { ret = src->setupRecord(type, true, customized_path, max_second); });
    return ret;
}

static inline bool stopRecord(Recorder::type type, const string &vhost, const string &app, const string &stream_id) {
    auto src = MediaSource::find(vhost, app, stream_id);
    if (!src) {
        return false;
    }
    bool ret;
    src->getOwnerPoller()->sync([&]() { ret = src->setupRecord(type, false, "", 0); });
    return ret;
}

API_EXPORT int API_CALL mk_recorder_is_recording(int type, const char *vhost, const char *app, const char *stream){
    assert(vhost && app && stream);
    return isRecording((Recorder::type)type,vhost,app,stream);
}

API_EXPORT int API_CALL mk_recorder_start(int type, const char *vhost, const char *app, const char *stream,const char *customized_path, size_t max_second){
    assert(vhost && app && stream);
    return startRecord((Recorder::type)type,vhost,app,stream,customized_path ? customized_path : "", max_second);
}

API_EXPORT int API_CALL mk_recorder_stop(int type, const char *vhost, const char *app, const char *stream){
    assert(vhost && app && stream);
    return stopRecord((Recorder::type)type,vhost,app,stream);
}

API_EXPORT void API_CALL mk_load_mp4_file(const char *vhost, const char *app, const char *stream, const char *file_path, int file_repeat) {
    mINI ini;
    mk_load_mp4_file2(vhost, app, stream, file_path, file_repeat, (mk_ini)&ini);
}

API_EXPORT void API_CALL mk_load_mp4_file2(const char *vhost, const char *app, const char *stream, const char *file_path, int file_repeat, mk_ini ini) {
#if ENABLE_MP4
    assert(vhost && app && stream && file_path && ini);
    ProtocolOption option(*((mINI *)ini));
    // mp4支持多track  [AUTO-TRANSLATED:b9688762]
    // mp4 supports multiple tracks
    option.max_track = 16;
    // 默认解复用mp4不生成mp4  [AUTO-TRANSLATED:11f2dcee]
    // By default, demultiplexing mp4 does not generate mp4
    option.enable_mp4 = false;
    // 但是如果参数明确指定开启mp4, 那么也允许之  [AUTO-TRANSLATED:b143a9e3]
    // But if the parameter explicitly specifies to enable mp4, then it is also allowed

    // 强制无人观看时自动关闭  [AUTO-TRANSLATED:f7c85948]
    // Force automatic shutdown when no one is watching
    option.auto_close = true;
    MediaTuple tuple = { vhost, app, stream, "" };
    auto reader = std::make_shared<MP4Reader>(tuple, file_path, option);
    // sample_ms设置为0，从配置文件加载；file_repeat可以指定，如果配置文件也指定循环解复用，那么强制开启  [AUTO-TRANSLATED:23e826b4]
    // sample_ms is set to 0, loaded from the configuration file; file_repeat can be specified, if the configuration file also specifies loop demultiplexing,
    // then force it to be enabled
    reader->startReadMP4(0, true, file_repeat);
#else
    WarnL << "MP4-related features are disabled. Please enable the ENABLE_MP4 macro and recompile.";
#endif
}

