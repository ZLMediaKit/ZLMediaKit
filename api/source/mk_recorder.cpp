/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "mk_recorder.h"
#include "Rtmp/FlvMuxer.h"
#include "Record/Recorder.h"
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
API_EXPORT int API_CALL mk_recorder_is_recording(int type, const char *vhost, const char *app, const char *stream){
    assert(vhost && app && stream);
    return Recorder::isRecording((Recorder::type)type,vhost,app,stream);
}

API_EXPORT int API_CALL mk_recorder_start(int type, const char *vhost, const char *app, const char *stream,const char *customized_path){
    assert(vhost && app && stream);
    return Recorder::startRecord((Recorder::type)type,vhost,app,stream,customized_path ? customized_path : "");
}

API_EXPORT int API_CALL mk_recorder_stop(int type, const char *vhost, const char *app, const char *stream){
    assert(vhost && app && stream);
    return Recorder::stopRecord((Recorder::type)type,vhost,app,stream);
}
