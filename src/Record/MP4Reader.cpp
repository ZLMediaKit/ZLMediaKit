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

#include <regex>
using namespace toolkit;
namespace mediakit {

static string regex_paser(const string &tex, const string &rule, uint8_t length) {
    smatch s;
    regex_search(tex, s, regex(rule));
    return s.str().substr(length, s.str().length() - length);
}

MP4Reader::MP4Reader(const string &strVhost,const string &strApp, const string &strId, const string& para, const string &filePath) {
    GET_CONFIG(string, recordPath, Record::kFilePath);
    GET_CONFIG(bool, enableVhost, General::kEnableVhost);
    _poller = WorkThreadPool::Instance().getPoller();
    _file_path = filePath;
    _str_vhost = strVhost;
    _str_app = strApp;
    _str_id = strId;
    if (para == "") {
        if (_file_path.empty()) {
            if (enableVhost) {
                _file_path = strVhost + "/" + strApp + "/" + strId;
            }
            else {
                _file_path = strApp + "/" + strId;
            }
            _file_path = File::absolutePath(_file_path, recordPath);
        }
        play_list.push_back(_file_path);
    }
    else {
        string &&record_vhost = regex_paser(para, "vhost=[_\\w]+", 6);
        auto pos = strId.rfind('/');
        string&& record_app = strId.substr(0, pos + 1);
        string&& record_stream = strId.substr(pos);
        string &&record_date = regex_paser(para, "date=[0-9-]+", 5);
        string &&record_time = regex_paser(para, "time=[0-9-]+", 5);
        auto record_path = Recorder::getRecordPath(Recorder::type_mp4, record_vhost, record_app, record_stream);
        record_path += record_date + "/";

        File::scanDir(record_path, [&](const string& path, bool isDir) {
            auto pos = path.rfind('/');
            if (pos != string::npos) {
                string file_name = path.substr(pos + 1, path.length() - pos - 1);
                    if (!isDir) {
                        //我们只收集mp4文件，对文件夹不感兴趣
                        record_time.erase(std::remove(record_time.begin(), record_time.end(), '-'), record_time.end());
                        file_name.erase(std::remove(file_name.begin(), file_name.end(), '-'), file_name.end());
                        if (atoi(file_name.c_str()) + 500 > atoi(record_time.c_str()))
                            play_list.push_back(path);
                    }
            }
            return true;
            }, false);
    }

    int i = 0;
    for (auto& play_file : play_list) {
        _demuxers.push_back(make_shared<MP4Demuxer>());
        _demuxers[i]->openMP4(play_list[i]);
        ++i;
    }
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
        auto frame = _demuxers[_file_num]->readFrame(keyFrame, eof);
        if (!frame) {
            continue;
        }
        _mediaMuxer->inputFrame(frame);
        if (frame->dts() > getCurrentStamp()) {
            break;
        }
    }

    GET_CONFIG(bool, fileRepeat, Record::kFileRepeat);
    if (eof) {
        if (fileRepeat) {
            //需要从头开始看
            seekTo(0);
            return true;
        }
        if (++_file_num >= _demuxers.size()) {
            return false;
        }
        //_demuxers[_file_num]->openMP4(play_list[_file_num]);
        seekTo(0);
    }

    return true;
}

void MP4Reader::startReadMP4() {
    float title_time = 0.0;
    for (auto& _demuxer : _demuxers) {
        title_time += _demuxer->getDurationMS() / 1000.0f;
    }
    _mediaMuxer.reset(new MultiMediaSourceMuxer(_str_vhost, _str_app, _str_id, title_time, true, true, false, false));
    auto tracks = _demuxers[_file_num]->getTracks(false);
    if (tracks.empty()) {
        throw std::runtime_error(StrPrinter << "该mp4文件没有有效的track:" << _file_path);
    }
    _have_video = false;
    for (auto& track : tracks) {
        _mediaMuxer->addTrack(track);
        if (track->getTrackType() == TrackVideo) {
            _have_video = true;
        }
    }
    //添加完毕所有track，防止单track情况下最大等待3秒
    _mediaMuxer->addTrackCompleted();
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
    return (uint32_t)(_seek_to + !_paused * _speed * _seek_ticker.elapsedTime());
}

void MP4Reader::setCurrentStamp(uint32_t new_stamp){
    auto old_stamp = getCurrentStamp();
    _seek_to = new_stamp;
    _seek_ticker.resetTime();
    if (old_stamp != new_stamp) {
        //时间轴未拖动时不操作
        _mediaMuxer->setTimeStamp(new_stamp);
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
    if (ui32Stamp != 0) {
        int i = 0;
        float title_time = ui32Stamp;
        for (auto& _demuxer : _demuxers) {
            title_time -= _demuxer->getDurationMS();
            if (title_time > 0)
                ++i;
            else {
                _file_num = i;
                break;
            }
        }
        if (i == _demuxers.size())
            return false;
        //_demuxers[_file_num]->openMP4(play_list[_file_num]);
        title_time += _demuxers[_file_num]->getDurationMS();
        ui32Stamp = title_time;
    }
    auto stamp = _demuxers[_file_num]->seekTo(ui32Stamp);
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
        auto frame = _demuxers[_file_num]->readFrame(keyFrame, eof);
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

MediaOriginType MP4Reader::getOriginType(MediaSource &sender) const {
    return MediaOriginType::mp4_vod;
}

string MP4Reader::getOriginUrl(MediaSource &sender) const {
    return _file_path;
}

} /* namespace mediakit */
#endif //ENABLE_MP4