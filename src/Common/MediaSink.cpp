/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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
    _allTrackReady = false;
    _trackReadyCallback[codec_id] = [this, track]() {
        onTrackReady(track);
    };
    _ticker.resetTime();

    track->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([this](const Frame::Ptr &frame) {
        if (_allTrackReady) {
            onTrackFrame(frame);
        }
    }));
}

void MediaSink::resetTracks() {
    lock_guard<recursive_mutex> lck(_mtx);
    _allTrackReady = false;
    _track_map.clear();
    _trackReadyCallback.clear();
    _ticker.resetTime();
    _max_track_size = 2;
}

void MediaSink::inputFrame(const Frame::Ptr &frame) {
    lock_guard<recursive_mutex> lck(_mtx);
    auto it = _track_map.find(frame->getCodecId());
    if (it == _track_map.end()) {
        return;
    }
    it->second->inputFrame(frame);
    checkTrackIfReady(it->second);
}

void MediaSink::checkTrackIfReady_l(const Track::Ptr &track){
    //Track由未就绪状态转换成就绪状态，我们就触发onTrackReady回调
    auto it_callback = _trackReadyCallback.find(track->getCodecId());
    if (it_callback != _trackReadyCallback.end() && track->ready()) {
        it_callback->second();
        _trackReadyCallback.erase(it_callback);
    }
}

void MediaSink::checkTrackIfReady(const Track::Ptr &track){
    if (!_allTrackReady && !_trackReadyCallback.empty()) {
        if (track) {
            checkTrackIfReady_l(track);
        } else {
            for (auto &pr : _track_map) {
                checkTrackIfReady_l(pr.second);
            }
        }
    }

    if(!_allTrackReady){
        if(_ticker.elapsedTime() > MAX_WAIT_MS_READY){
            //如果超过规定时间，那么不再等待并忽略未准备好的Track
            emitAllTrackReady();
            return;
        }

        if(!_trackReadyCallback.empty()){
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
    {
        lock_guard<recursive_mutex> lck(_mtx);
        _max_track_size = _track_map.size();
    }
    checkTrackIfReady(nullptr);
}

void MediaSink::emitAllTrackReady() {
    if (_allTrackReady) {
        return;
    }

    if (!_trackReadyCallback.empty()) {
        //这是超时强制忽略未准备好的Track
        _trackReadyCallback.clear();
        //移除未准备好的Track
        for (auto it = _track_map.begin(); it != _track_map.end();) {
            if (!it->second->ready()) {
                WarnL << "该track长时间未被初始化,已忽略:" << it->second->getCodecName();
                it = _track_map.erase(it);
                continue;
            }
            ++it;
        }
    }

    if (!_track_map.empty()) {
        //最少有一个有效的Track
        _allTrackReady = true;
        onAllTrackReady();
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
