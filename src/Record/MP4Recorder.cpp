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
#include <ctime>
#include <sys/stat.h>
#include "Util/File.h"
#include "Common/config.h"
#include "MP4Recorder.h"
#include "Thread/WorkThreadPool.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

MP4Recorder::MP4Recorder(const string &path, const string &vhost, const string &app, const string &stream_id, size_t max_second) {
    _folder_path = path;
    /////record 业务逻辑//////
    _info.app = app;
    _info.stream = stream_id;
    _info.vhost = vhost;
    _info.folder = path;
    GET_CONFIG(size_t ,recordSec,Record::kFileSecond);
    _max_second = max_second ? max_second : recordSec;
}

MP4Recorder::~MP4Recorder() {
    closeFile();
}

void MP4Recorder::createFile() {
    closeFile();
    auto date = getTimeStr("%Y-%m-%d");
    auto time = getTimeStr("%H-%M-%S");
    auto full_path_tmp = _folder_path + date + "/." + time + ".mp4";
    auto full_path = _folder_path + date + "/" + time + ".mp4";

    /////record 业务逻辑//////
    _info.start_time = ::time(NULL);
    _info.file_name = time + ".mp4";
    _info.file_path = full_path;
    GET_CONFIG(string, appName, Record::kAppName);
    _info.url = appName + "/" + _info.app + "/" + _info.stream + "/" + date + "/" + time + ".mp4";

    try {
        _muxer = std::make_shared<MP4Muxer>();
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
    WorkThreadPool::Instance().getExecutor()->async([muxer, full_path_tmp, full_path, info]() mutable {
        //获取文件录制时间，放在关闭mp4之前是为了忽略关闭mp4执行时间
        info.time_len = (float) (::time(NULL) - info.start_time);
        //关闭mp4非常耗时，所以要放在后台线程执行
        muxer->closeMP4();
        
        if(!full_path_tmp.empty()) {
            //获取文件大小
            info.file_size = File::fileSize(full_path_tmp.data());
            if (info.file_size < 1024) {
                //录像文件太小，删除之
                File::delete_file(full_path_tmp.data());
                return;
            }
            //临时文件名改成正式文件名，防止mp4未完成时被访问
            rename(full_path_tmp.data(), full_path.data());
        }

        /////record 业务逻辑//////
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastRecordMP4, info);
    });
}

void MP4Recorder::closeFile() {
    if (_muxer) {
        asyncClose();
        _muxer = nullptr;
    }
}

bool MP4Recorder::inputFrame(const Frame::Ptr &frame) {
    if (!(_have_video && frame->getTrackType() == TrackAudio)) {
        //如果有视频且输入的是音频，那么应该忽略切片逻辑
        if (_last_dts == 0 || _last_dts > frame->dts()) {
            //极少情况下dts时间戳可能回退
            _last_dts = frame->dts();
        }

        auto duration = frame->dts() - _last_dts;
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
