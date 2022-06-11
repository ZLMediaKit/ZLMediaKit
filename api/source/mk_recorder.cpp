/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_recorder.h"
#include "Rtmp/FlvMuxer.h"
#include "Record/Recorder.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

API_EXPORT mk_flv_recorder API_CALL mk_flv_recorder_create(){
    FlvRecorder::Ptr *ret = new FlvRecorder::Ptr(new FlvRecorder);
    return ret;
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

///////////////////////////////////////////hls/mp4录制/////////////////////////////////////////////

static inline bool isRecording(Recorder::type type, const string &vhost, const string &app, const string &stream_id){
    auto src = MediaSource::find(vhost, app, stream_id);
    if(!src){
        return false;
    }
    return src->isRecording(type);
}

static inline  bool startRecord(Recorder::type type, const string &vhost, const string &app, const string &stream_id,const string &customized_path, size_t max_second){
    auto src = MediaSource::find(vhost, app, stream_id);
    if (!src) {
        WarnL << "未找到相关的MediaSource,startRecord失败:" << vhost << "/" << app << "/" << stream_id;
        return false;
    }
    return src->setupRecord(type, true, customized_path, max_second);
}

static inline bool stopRecord(Recorder::type type, const string &vhost, const string &app, const string &stream_id){
    auto src = MediaSource::find(vhost, app, stream_id);
    if(!src){
        return false;
    }
    return src->setupRecord(type, false, "", 0);
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
