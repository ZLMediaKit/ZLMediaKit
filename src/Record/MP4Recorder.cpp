﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifdef ENABLE_MP4
#include <ctime>
#include <sys/stat.h>
#include "Util/File.h"
#include "Common/config.h"
#include "MP4Recorder.h"
#include "Thread/WorkThreadPool.h"
#include "MP4Muxer.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

MP4Recorder::MP4Recorder(const MediaTuple &tuple, const string &path, size_t max_second) {
    _folder_path = path;
    /////record 业务逻辑//////
    static_cast<MediaTuple &>(_info) = tuple;
    _info.folder = path;
    GET_CONFIG(uint32_t, s_max_second, Protocol::kMP4MaxSecond);
    _max_second = max_second ? max_second : s_max_second;
}

MP4Recorder::~MP4Recorder() {
    try {
        flush();
        closeFile();
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }
}

void MP4Recorder::createFile() {
    closeFile();
    auto date = getTimeStr("%Y-%m-%d");
    auto file_name = getTimeStr("%H-%M-%S") + "-" + std::to_string(_file_index++) + ".mp4";
    auto full_path = _folder_path + date + "/" + file_name;
    auto full_path_tmp = _folder_path + date + "/." + file_name;

    /////record 业务逻辑//////
    _info.start_time = ::time(NULL);
    _info.file_name = file_name;
    _info.file_path = full_path;
    GET_CONFIG(string, appName, Record::kAppName);
    _info.url = appName + "/" + _info.app + "/" + _info.stream + "/" + date + "/" + file_name;

    try {
        _muxer = std::make_shared<MP4Muxer>();
        TraceL << "Open tmp mp4 file: " << full_path_tmp;
        _muxer->openMP4(full_path_tmp);
        for (auto &track :_tracks) {
            //添加track
            _muxer->addTrack(track);
        }
        _full_path_tmp = full_path_tmp;
        _full_path = full_path;
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }
}

void MP4Recorder::asyncClose() {
    auto muxer = _muxer;
    auto full_path_tmp = _full_path_tmp;
    auto full_path = _full_path;
    auto info = _info;
    TraceL << "Start close tmp mp4 file: " << full_path_tmp;
    WorkThreadPool::Instance().getExecutor()->async([muxer, full_path_tmp, full_path, info]() mutable {
        info.time_len = muxer->getDuration() / 1000.0f;
        // 关闭mp4可能非常耗时，所以要放在后台线程执行
        TraceL << "Closing tmp mp4 file: " << full_path_tmp;
        muxer->closeMP4();
        TraceL << "Closed tmp mp4 file: " << full_path_tmp;
        if (!full_path_tmp.empty()) {
            // 获取文件大小
            info.file_size = File::fileSize(full_path_tmp);
            if (info.file_size < 1024) {
                // 录像文件太小，删除之
                File::delete_file(full_path_tmp);
                return;
            }
            // 临时文件名改成正式文件名，防止mp4未完成时被访问
            rename(full_path_tmp.data(), full_path.data());
        }
        TraceL << "Emit mp4 record event: " << full_path;
        //触发mp4录制切片生成事件
        NOTICE_EMIT(BroadcastRecordMP4Args, Broadcast::kBroadcastRecordMP4, info);
    });
}

void MP4Recorder::closeFile() {
    if (_muxer) {
        asyncClose();
        _muxer = nullptr;
    }
}

void MP4Recorder::flush() {
    if (_muxer) {
        _muxer->flush();
    }
}

bool MP4Recorder::inputFrame(const Frame::Ptr &frame) {
    if (!(_have_video && frame->getTrackType() == TrackAudio)) {
        //如果有视频且输入的是音频，那么应该忽略切片逻辑
        if (_last_dts == 0 || _last_dts > frame->dts()) {
            //b帧情况下dts时间戳可能回退
            _last_dts = MAX(frame->dts(), _last_dts);
        }
        auto duration = 5u; // 默认至少一帧5ms
        if (frame->dts() > 0 && frame->dts() > _last_dts) {
            duration = MAX(duration, frame->dts() - _last_dts);
        }
        if (!_muxer || ((duration > _max_second * 1000) && (!_have_video || (_have_video && frame->keyFrame())))) {
            //成立条件
            // 1、_muxer为空
            // 2、到了切片时间，并且只有音频
            // 3、到了切片时间，有视频并且遇到视频的关键帧
            _last_dts = 0;
            createFile();
        }
    }

    if (_muxer) {
        //生成mp4文件
        return _muxer->inputFrame(frame);
    }
    return false;
}

bool MP4Recorder::addTrack(const Track::Ptr &track) {
    //保存所有的track，为创建MP4MuxerFile做准备
    _tracks.emplace_back(track);
    if (track->getTrackType() == TrackVideo) {
        _have_video = true;
    }
    return true;
}

void MP4Recorder::resetTracks() {
    closeFile();
    _tracks.clear();
    _have_video = false;
}

} /* namespace mediakit */

#endif //ENABLE_MP4
