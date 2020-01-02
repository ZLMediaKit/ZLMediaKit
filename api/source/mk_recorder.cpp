/*
 * MIT License
 *
 * Copyright (c) 2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
API_EXPORT int API_CALL mk_recorder_status(int type, const char *vhost, const char *app, const char *stream){
    assert(vhost && app && stream);
    return Recorder::getRecordStatus((Recorder::type)type,vhost,app,stream);
}

API_EXPORT int API_CALL mk_recorder_start(int type, const char *vhost, const char *app, const char *stream,const char *customized_path,int wait_for_record, int continue_record){
    assert(vhost && app && stream);
    return Recorder::startRecord((Recorder::type)type,vhost,app,stream,customized_path ? customized_path : "",wait_for_record,continue_record);
}

API_EXPORT int API_CALL mk_recorder_stop(int type, const char *vhost, const char *app, const char *stream){
    assert(vhost && app && stream);
    return Recorder::stopRecord((Recorder::type)type,vhost,app,stream);
}

API_EXPORT void API_CALL mk_recorder_stop_all(){
    Recorder::stopAll();
}