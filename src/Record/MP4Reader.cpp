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

using namespace std;
using namespace toolkit;

namespace mediakit {

MP4Reader::MP4Reader(const string &vhost, const string &app, const string &stream_id, const string &file_path) {
    //读写文件建议放在后台线程
    _poller = WorkThreadPool::Instance().getPoller();
    _file_path = file_path;
    if (_file_path.empty()) {
        GET_CONFIG(string, recordPath, Record::kFilePath);
        GET_CONFIG(bool, enableVhost, General::kEnableVhost);
        if (enableVhost) {
            _file_path = vhost + "/" + app + "/" + stream_id;
        } else {
            _file_path = app + "/" + stream_id;
        }
        _file_path = File::absolutePath(_file_path, recordPath);
    }

    _demuxer = std::make_shared<MP4Demuxer>();
    _demuxer->openMP4(_file_path);

    if (stream_id.empty()) {
        return;
    }
    ProtocolOption option;
    //读取mp4文件并流化时，不重复生成mp4/hls文件
    option.enable_mp4 = false;
    option.enable_hls = false;
    _muxer = std::make_shared<MultiMediaSourceMuxer>(vhost, app, stream_id, _demuxer->getDurationMS() / 1000.0f, option);
    auto tracks = _demuxer->getTracks(false);
    if (tracks.empty()) {
        throw std::runtime_error(StrPrinter << "该mp4文件没有有效的track:" << _file_path);
    }
    for (auto &track : tracks) {
        _muxer->addTrack(track);
        if (track->getTrackType() == TrackVideo) {
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
    while (!eof && _last_dts < getCurrentStamp()) {
        auto frame = _demuxer->readFrame(keyFrame, eof);
        if (!frame) {
            continue;
        }
        _last_dts = frame->dts();
        if (_muxer) {
            _muxer->inputFrame(frame);
        }
    }

    GET_CONFIG(bool, file_repeat, Record::kFileRepeat);
    if (eof && (file_repeat || _file_repeat)) {
        //需要从头开始看
        seekTo(0);
        return true;
    }

    return !eof;
}

bool MP4Reader::readNextSample() {
    bool keyFrame = false;
    bool eof = false;
    auto frame = _demuxer->readFrame(keyFrame, eof);
    if (!frame) {
        return false;
    }
    if (_muxer) {
        _muxer->inputFrame(frame);
    }
    setCurrentStamp(frame->dts());
    return true;
}

void MP4Reader::stopReadMP4() {
    _timer = nullptr;
}

void MP4Reader::startReadMP4(uint64_t sample_ms, bool ref_self, bool file_repeat) {
    GET_CONFIG(uint32_t, sampleMS, Record::kSampleMS);
    auto strong_self = shared_from_this();
    if (_muxer) {
        _muxer->setMediaListener(strong_self);
        //一直读到所有track就绪为止
        while (!_muxer->isAllTrackReady() && readNextSample()) {}
    }

    auto timer_sec = (sample_ms ? sample_ms : sampleMS) / 1000.0f;

    //启动定时器
    if (ref_self) {
        _timer = std::make_shared<Timer>(timer_sec, [strong_self]() {
            lock_guard<recursive_mutex> lck(strong_self->_mtx);
            return strong_self->readSample();
        }, _poller);
    } else {
        weak_ptr<MP4Reader> weak_self = strong_self;
        _timer = std::make_shared<Timer>(timer_sec, [weak_self]() {
            auto strong_self = weak_self.lock();
            if (!strong_self) {
                return false;
            }
            lock_guard<recursive_mutex> lck(strong_self->_mtx);
            return strong_self->readSample();
        }, _poller);
    }

    _file_repeat = file_repeat;
}

const MP4Demuxer::Ptr &MP4Reader::getDemuxer() const {
    return _demuxer;
}

uint32_t MP4Reader::getCurrentStamp() {
    return (uint32_t) (_seek_to + !_paused * _speed * _seek_ticker.elapsedTime());
}

void MP4Reader::setCurrentStamp(uint32_t new_stamp) {
    auto old_stamp = getCurrentStamp();
    _seek_to = new_stamp;
    _last_dts = new_stamp;
    _seek_ticker.resetTime();
    if (old_stamp != new_stamp && _muxer) {
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

bool MP4Reader::seekTo(uint32_t stamp_seek) {
    lock_guard<recursive_mutex> lck(_mtx);
    if (stamp_seek > _demuxer->getDurationMS()) {
        //超过文件长度
        return false;
    }
    auto stamp = _demuxer->seekTo(stamp_seek);
    if (stamp == -1) {
        //seek失败
        return false;
    }

    if (!_have_video) {
        //没有视频，不需要搜索关键帧；设置当前时间戳
        setCurrentStamp((uint32_t) stamp);
        return true;
    }
    //搜索到下一帧关键帧
    bool keyFrame = false;
    bool eof = false;
    while (!eof) {
        auto frame = _demuxer->readFrame(keyFrame, eof);
        if (!frame) {
            //文件读完了都未找到下一帧关键帧
            continue;
        }
        if (keyFrame || frame->keyFrame() || frame->configFrame()) {
            //定位到key帧
            if (_muxer) {
                _muxer->inputFrame(frame);
            }
            //设置当前时间戳
            setCurrentStamp(frame->dts());
            return true;
        }
    }
    return false;
}

bool MP4Reader::close(MediaSource &sender) {
    _timer = nullptr;
    WarnL << "close media: " << sender.getUrl();
    return true;
}

MediaOriginType MP4Reader::getOriginType(MediaSource &sender) const {
    return MediaOriginType::mp4_vod;
}

string MP4Reader::getOriginUrl(MediaSource &sender) const {
    return _file_path;
}

toolkit::EventPoller::Ptr MP4Reader::getOwnerPoller(MediaSource &sender) {
    return _poller;
}

} /* namespace mediakit */
#endif //ENABLE_MP4