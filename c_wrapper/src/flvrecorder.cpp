//
// Created by xzl on 2018/8/31.
//

#include "flvrecorder.h"
#include "Rtmp/FlvMuxer.h"

using namespace ZL::Rtmp;

API_EXPORT FlvRecorderContex API_CALL createFlvRecorder(){
    DebugL;
    FlvRecorder::Ptr *ret = new FlvRecorder::Ptr(new FlvRecorder);
    return ret;
}
API_EXPORT void API_CALL releaseFlvRecorder(FlvRecorderContex ctx){
    DebugL;
    FlvRecorder::Ptr *record = (FlvRecorder::Ptr *)(ctx);
    delete record;
}
API_EXPORT int API_CALL flvRecorder_start(FlvRecorderContex ctx,const char *appName,const char *streamName, const char *file_path){
    DebugL << appName << " " << streamName << " " << file_path;
    FlvRecorder::Ptr *record = (FlvRecorder::Ptr *)(ctx);
    try {
        (*record)->startRecord(DEFAULT_VHOST,appName,streamName,file_path);
        return 0;
    }catch (std::exception &ex){
        WarnL << ex.what();
        return -1;
    }
}