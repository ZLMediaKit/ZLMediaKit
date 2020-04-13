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
#include "MP4Reader.h"
#include "Common/config.h"
#include "Thread/WorkThreadPool.h"
using namespace toolkit;
namespace mediakit {

MP4Reader::MP4Reader(const string &strVhost,const string &strApp, const string &strId,const string &filePath ) {
    _poller = WorkThreadPool::Instance().getPoller();
    auto strFileName = filePath;
    if(strFileName.empty()){
        GET_CONFIG(string,recordPath,Record::kFilePath);
        GET_CONFIG(bool,enableVhost,General::kEnableVhost);
        if(enableVhost){
            strFileName = strVhost + "/" + strApp + "/" + strId;
        }else{
            strFileName = strApp + "/" + strId;
        }
        strFileName = File::absolutePath(strFileName,recordPath);
    }

    _demuxer = std::make_shared<MP4Demuxer>(strFileName.data());
    _mediaMuxer.reset(new MultiMediaSourceMuxer(strVhost, strApp, strId, _demuxer->getDurationMS() / 1000.0, true, true, false, false));
    auto tracks = _demuxer->getTracks(false);
    if(tracks.empty()){
        throw std::runtime_error(StrPrinter << "该mp4文件没有有效的track:" << strFileName);
    }
    for(auto &track : tracks){
        _mediaMuxer->addTrack(track);
        if(track->getTrackType() == TrackVideo){
            _have_video = true;
        }
    }
    //添加完毕所有track，防止单track情况下最大等待3秒
    _mediaMuxer->addTrackCompleted();
}

bool MP4Reader::readSample() {
    bool keyFrame = false;
    bool eof = false;
    while (!eof) {
        auto frame = _demuxer->readFrame(keyFrame, eof);
        if (!frame) {
            continue;
        }
        _mediaMuxer->inputFrame(frame);
        if (frame->dts() > getCurrentStamp()) {
            break;
        }
    }

    GET_CONFIG(bool, fileRepeat, Record::kFileRepeat);
    if (eof && fileRepeat) {
        //需要从头开始看
        seekTo(0);
        return true;
    }

    return !eof;
}

void MP4Reader::startReadMP4() {
    GET_CONFIG(uint32_t, sampleMS, Record::kSampleMS);
    auto strongSelf = shared_from_this();
    _mediaMuxer->setMediaListener(strongSelf);

    //先获取关键帧
    seekTo(0);
    //读sampleMS毫秒的数据用于产生MediaSource
    setCurrentStamp(getCurrentStamp() + sampleMS);
    readSample();

    //启动定时器
    _timer = std::make_shared<Timer>(sampleMS / 1000.0f, [strongSelf]() {
        lock_guard<recursive_mutex> lck(strongSelf->_mtx);
        return strongSelf->readSample();
    }, _poller);
}

uint32_t MP4Reader::getCurrentStamp() {
    return _seek_to + _seek_ticker.elapsedTime();
}

void MP4Reader::setCurrentStamp(uint32_t ui32Stamp){
    _seek_to = ui32Stamp;
    _seek_ticker.resetTime();
    _mediaMuxer->setTimeStamp(ui32Stamp);
}

bool MP4Reader::seekTo(MediaSource &sender,uint32_t ui32Stamp){
    return seekTo(ui32Stamp);
}

bool MP4Reader::seekTo(uint32_t ui32Stamp){
    lock_guard<recursive_mutex> lck(_mtx);
    if (ui32Stamp > _demuxer->getDurationMS()) {
        //超过文件长度
        return false;
    }
    auto stamp = _demuxer->seekTo(ui32Stamp);
    if(stamp == -1){
        //seek失败
        return false;
    }

    if(!_have_video){
        //没有视频，不需要搜索关键帧
        //设置当前时间戳
        setCurrentStamp(stamp);
        return true;
    }
    //搜索到下一帧关键帧
    bool keyFrame = false;
    bool eof = false;
    while (!eof) {
        auto frame = _demuxer->readFrame(keyFrame, eof);
        if(!frame){
            //文件读完了都未找到下一帧关键帧
            continue;
        }
        if(keyFrame || frame->keyFrame() || frame->configFrame()){
            //定位到key帧
            _mediaMuxer->inputFrame(frame);
            //设置当前时间戳
            setCurrentStamp(frame->dts());
            return true;
        }
    }
    return false;
}

bool MP4Reader::close(MediaSource &sender,bool force){
    if(!_mediaMuxer || (!force && _mediaMuxer->totalReaderCount())){
        return false;
    }
    _timer.reset();
    WarnL << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    return true;
}

int MP4Reader::totalReaderCount(MediaSource &sender) {
    return _mediaMuxer ? _mediaMuxer->totalReaderCount() : sender.readerCount();
}

} /* namespace mediakit */
#endif //ENABLE_MP4