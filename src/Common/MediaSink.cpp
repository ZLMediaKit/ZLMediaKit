/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "MediaSink.h"

//最多等待未初始化的Track 10秒，超时之后会忽略未初始化的Track
#define MAX_WAIT_MS_READY 10000

//如果添加Track，最多等待3秒
#define MAX_WAIT_MS_ADD_TRACK 3000


namespace mediakit{

void MediaSink::addTrack(const Track::Ptr &track_in) {
    lock_guard<recursive_mutex> lck(_mtx);
    //克隆Track，只拷贝其数据，不拷贝其数据转发关系
    auto track = track_in->clone();
    auto codec_id = track->getCodecId();
    _track_map[codec_id] = track;
    _all_track_ready = false;
    _track_ready_callback[codec_id] = [this, track]() {
        onTrackReady(track);
    };
    _ticker.resetTime();

    track->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([this](const Frame::Ptr &frame) {
        if (_all_track_ready) {
            onTrackFrame(frame);
        } else {
            //还有Track未就绪，先缓存之
            _frame_unread[frame->getCodecId()].emplace_back(Frame::getCacheAbleFrame(frame));
        }
    }));
}

void MediaSink::resetTracks() {
    lock_guard<recursive_mutex> lck(_mtx);
    _all_track_ready = false;
    _track_map.clear();
    _track_ready_callback.clear();
    _ticker.resetTime();
    _max_track_size = 2;
    _frame_unread.clear();
}

void MediaSink::inputFrame(const Frame::Ptr &frame) {
    lock_guard<recursive_mutex> lck(_mtx);
    auto it = _track_map.find(frame->getCodecId());
    if (it == _track_map.end()) {
        return;
    }
    it->second->inputFrame(frame);
    checkTrackIfReady(nullptr);
}

void MediaSink::checkTrackIfReady_l(const Track::Ptr &track){
    //Track由未就绪状态转换成就绪状态，我们就触发onTrackReady回调
    auto it_callback = _track_ready_callback.find(track->getCodecId());
    if (it_callback != _track_ready_callback.end() && track->ready()) {
        it_callback->second();
        _track_ready_callback.erase(it_callback);
    }
}

void MediaSink::checkTrackIfReady(const Track::Ptr &track){
    if (!_all_track_ready && !_track_ready_callback.empty()) {
        if (track) {
            checkTrackIfReady_l(track);
        } else {
            for (auto &pr : _track_map) {
                checkTrackIfReady_l(pr.second);
            }
        }
    }

    if(!_all_track_ready){
        if(_ticker.elapsedTime() > MAX_WAIT_MS_READY){
            //如果超过规定时间，那么不再等待并忽略未准备好的Track
            emitAllTrackReady();
            return;
        }

        if(!_track_ready_callback.empty()){
            //在超时时间内，如果存在未准备好的Track，那么继续等待
            return;
        }

        if(_track_map.size() == _max_track_size){
            //如果已经添加了音视频Track，并且不存在未准备好的Track，那么说明所有Track都准备好了
            emitAllTrackReady();
            return;
        }

        if(_track_map.size() == 1 && _ticker.elapsedTime() > MAX_WAIT_MS_ADD_TRACK){
            //如果只有一个Track，那么在该Track添加后，我们最多还等待若干时间(可能后面还会添加Track)
            emitAllTrackReady();
            return;
        }
    }
}

void MediaSink::addTrackCompleted(){
    lock_guard<recursive_mutex> lck(_mtx);
    _max_track_size = _track_map.size();
    checkTrackIfReady(nullptr);
}

void MediaSink::emitAllTrackReady() {
    if (_all_track_ready) {
        return;
    }

    DebugL << "all track ready use " << _ticker.elapsedTime() << "ms";
    if (!_track_ready_callback.empty()) {
        //这是超时强制忽略未准备好的Track
        _track_ready_callback.clear();
        //移除未准备好的Track
        for (auto it = _track_map.begin(); it != _track_map.end();) {
            if (!it->second->ready()) {
                WarnL << "track not ready for a long time, ignored: " << it->second->getCodecName();
                it = _track_map.erase(it);
                continue;
            }
            ++it;
        }
    }

    if (!_track_map.empty()) {
        //最少有一个有效的Track
        _all_track_ready = true;
        onAllTrackReady();

        //全部Track就绪，我们一次性把之前的帧输出
        for(auto &pr : _frame_unread){
            if (_track_map.find(pr.first) == _track_map.end()) {
                //该Track已经被移除
                continue;
            }
            pr.second.for_each([&](const Frame::Ptr &frame) {
                onTrackFrame(frame);
            });
        }
        _frame_unread.clear();
    }
}

vector<Track::Ptr> MediaSink::getTracks(bool trackReady) const{
    vector<Track::Ptr> ret;
    lock_guard<recursive_mutex> lck(_mtx);
    for (auto &pr : _track_map){
        if(trackReady && !pr.second->ready()){
            continue;
        }
        ret.emplace_back(pr.second);
    }
    return std::move(ret);
}


}//namespace mediakit
