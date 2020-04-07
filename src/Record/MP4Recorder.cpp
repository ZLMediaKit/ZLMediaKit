/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifdef ENABLE_MP4
#include <ctime>
#include <sys/stat.h>
#include "Common/config.h"
#include "MP4Recorder.h"
#include "Thread/WorkThreadPool.h"

using namespace toolkit;

namespace mediakit {

MP4Recorder::MP4Recorder(const string& strPath,
                   const string &strVhost,
                   const string &strApp,
                   const string &strStreamId) {
    _strPath = strPath;
    /////record 业务逻辑//////
    _info.strAppName = strApp;
    _info.strStreamId = strStreamId;
    _info.strVhost = strVhost;
    _info.strFolder = strPath;
}
MP4Recorder::~MP4Recorder() {
    closeFile();
}

void MP4Recorder::createFile() {
    closeFile();
    auto strDate = getTimeStr("%Y-%m-%d");
    auto strTime = getTimeStr("%H-%M-%S");
    auto strFileTmp = _strPath + strDate + "/." + strTime + ".mp4";
    auto strFile =	_strPath + strDate + "/" + strTime + ".mp4";

    /////record 业务逻辑//////
    _info.ui64StartedTime = ::time(NULL);
    _info.strFileName = strTime + ".mp4";
    _info.strFilePath = strFile;
    GET_CONFIG(string,appName,Record::kAppName);
    _info.strUrl = appName + "/"
                   + _info.strAppName + "/"
                   + _info.strStreamId + "/"
                   + strDate + "/"
                   + strTime + ".mp4";

    try {
        _muxer = std::make_shared<MP4Muxer>(strFileTmp.data());
        for(auto &track :_tracks){
            //添加track
            _muxer->addTrack(track);
        }
        _strFileTmp = strFileTmp;
        _strFile = strFile;
        _createFileTicker.resetTime();
    }catch(std::exception &ex) {
        WarnL << ex.what();
    }
}

void MP4Recorder::asyncClose() {
    auto muxer = _muxer;
    auto strFileTmp = _strFileTmp;
    auto strFile = _strFile;
    auto info = _info;
    WorkThreadPool::Instance().getExecutor()->async([muxer,strFileTmp,strFile,info]() {
        //获取文件录制时间，放在关闭mp4之前是为了忽略关闭mp4执行时间
        const_cast<MP4Info&>(info).ui64TimeLen = ::time(NULL) - info.ui64StartedTime;
        //关闭mp4非常耗时，所以要放在后台线程执行
        const_cast<MP4Muxer::Ptr &>(muxer).reset();
        //临时文件名改成正式文件名，防止mp4未完成时被访问
        rename(strFileTmp.data(),strFile.data());
        //获取文件大小
        struct stat fileData;
        stat(strFile.data(), &fileData);
        const_cast<MP4Info&>(info).ui64FileSize = fileData.st_size;
        /////record 业务逻辑//////
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastRecordMP4,info);
    });
}

void MP4Recorder::closeFile() {
    if (_muxer) {
        asyncClose();
        _muxer = nullptr;
    }
}

void MP4Recorder::inputFrame(const Frame::Ptr &frame) {
    GET_CONFIG(uint32_t,recordSec,Record::kFileSecond);
    if(!_muxer || ((_createFileTicker.elapsedTime() > recordSec * 1000) &&
                  (!_haveVideo || (_haveVideo && frame->keyFrame()))) ){
        //成立条件
        //1、_muxer为空
        //2、到了切片时间，并且只有音频
        //3、到了切片时间，有视频并且遇到视频的关键帧
        createFile();
    }

    if(_muxer){
        //生成mp4文件
        _muxer->inputFrame(frame);
    }
}

void MP4Recorder::addTrack(const Track::Ptr & track){
    //保存所有的track，为创建MP4MuxerFile做准备
    _tracks.emplace_back(track);
    if(track->getTrackType() == TrackVideo){
        _haveVideo = true;
    }
}

void MP4Recorder::resetTracks() {
    closeFile();
    _tracks.clear();
    _haveVideo = false;
    _createFileTicker.resetTime();
}

} /* namespace mediakit */


#endif //ENABLE_MP4
