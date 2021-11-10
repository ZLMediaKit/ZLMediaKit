/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
    _file_path = filePath;
    if(_file_path.empty()){
        GET_CONFIG(string,recordPath,Record::kFilePath);
        GET_CONFIG(bool,enableVhost,General::kEnableVhost);
        if(enableVhost){
            _file_path = strVhost + "/" + strApp + "/" + strId;
        }else{
            _file_path = strApp + "/" + strId;
        }
        _file_path = File::absolutePath(_file_path,recordPath);
    }

    _demuxer = std::make_shared<MP4Demuxer>();
    _demuxer->openMP4(_file_path);
    _muxer = std::make_shared<MultiMediaSourceMuxer>(strVhost, strApp, strId, _demuxer->getDurationMS() / 1000.0f, true, true, false, false);
    auto tracks = _demuxer->getTracks(false);
    if(tracks.empty()){
        throw std::runtime_error(StrPrinter << "该mp4文件没有有效的track:" << _file_path);
    }
    for(auto &track : tracks){
        _muxer->addTrack(track);
        if(track->getTrackType() == TrackVideo){
            _have_video = true;
        }
    }
    //添加完毕所有track，防止单track情况下最大等待3秒
    _muxer->addTrackCompleted();
}

bool MP4Reader::readSample() {
    if (_paused) {
        //确保暂停时，时间轴不走动
        _seek_ticker.resetTime();
        return true;
    }

    bool keyFrame = false;
    bool eof = false;
    while (!eof) {
        auto frame = _demuxer->readFrame(keyFrame, eof);
        if (!frame) {
            continue;
        }
        _muxer->inputFrame(frame);
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
    _muxer->setMediaListener(strongSelf);

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
    return (uint32_t)(_seek_to + !_paused * _speed * _seek_ticker.elapsedTime());
}

void MP4Reader::setCurrentStamp(uint32_t new_stamp){
    auto old_stamp = getCurrentStamp();
    _seek_to = new_stamp;
    _seek_ticker.resetTime();
    if (old_stamp != new_stamp) {
        //时间轴未拖动时不操作
        _muxer->setTimeStamp(new_stamp);
    }
}

bool MP4Reader::seekTo(MediaSource &sender, uint32_t stamp) {
    //拖动进度条后应该恢复播放
    pause(sender, false);
    TraceL << getOriginUrl(sender) << ",stamp:" << stamp;
    return seekTo(stamp);
}

bool MP4Reader::pause(MediaSource &sender, bool pause) {
    if (_paused == pause) {
        return true;
    }
    //_seek_ticker重新计时，不管是暂停还是seek都不影响总的播放进度
    setCurrentStamp(getCurrentStamp());
    _paused = pause;
    TraceL << getOriginUrl(sender) << ",pause:" << pause;
    return true;
}

bool MP4Reader::speed(MediaSource &sender, float speed) {
    if (speed < 0.1 && speed > 20) {
        WarnL << "播放速度取值范围非法:" << speed;
        return false;
    }
    //设置播放速度后应该恢复播放
    pause(sender, false);
    if (_speed == speed) {
        return true;
    }
    _speed = speed;
    TraceL << getOriginUrl(sender) << ",speed:" << speed;
    return true;
}

bool MP4Reader::seekTo(uint32_t ui32Stamp) {
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
        setCurrentStamp((uint32_t)stamp);
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
            _muxer->inputFrame(frame);
            //设置当前时间戳
            setCurrentStamp(frame->dts());
            return true;
        }
    }
    return false;
}

bool MP4Reader::close(MediaSource &sender,bool force){
    if(!_muxer || (!force && _muxer->totalReaderCount())){
        return false;
    }
    _timer.reset();
    WarnL << sender.getSchema() << "/" << sender.getVhost() << "/" << sender.getApp() << "/" << sender.getId() << " " << force;
    return true;
}

int MP4Reader::totalReaderCount(MediaSource &sender) {
    return _muxer ? _muxer->totalReaderCount() : sender.readerCount();
}

MediaOriginType MP4Reader::getOriginType(MediaSource &sender) const {
    return MediaOriginType::mp4_vod;
}

string MP4Reader::getOriginUrl(MediaSource &sender) const {
    return _file_path;
}

} /* namespace mediakit */
#endif //ENABLE_MP4