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
#define MAX_WAIT_MS 10000

namespace mediakit{

void MediaSink::addTrack(const Track::Ptr &track_in) {
    lock_guard<recursive_mutex> lck(_mtx);
    //克隆Track，只拷贝其数据，不拷贝其数据转发关系
    auto track = track_in->clone();

    auto codec_id = track->getCodecId();
    _track_map[codec_id] = track;
    auto lam = [this,track](){
        onTrackReady(track);
    };
    if(track->ready()){
        lam();
    }else{
        _anyTrackUnReady = true;
        _allTrackReady = false;
        _trackReadyCallback[codec_id] = lam;
        _ticker.resetTime();
    }

    weak_ptr<MediaSink> weakSelf = shared_from_this();
    track->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([weakSelf](const Frame::Ptr &frame){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        if(!strongSelf->_anyTrackUnReady){
            strongSelf->onTrackFrame(frame);
        }
    }));
}

void MediaSink::resetTracks() {
    lock_guard<recursive_mutex> lck(_mtx);
    _anyTrackUnReady = false;
    _allTrackReady = false;
    _track_map.clear();
    _trackReadyCallback.clear();
    _ticker.resetTime();
}

void MediaSink::inputFrame(const Frame::Ptr &frame) {
    lock_guard<recursive_mutex> lck(_mtx);
    auto codec_id = frame->getCodecId();
    auto it = _track_map.find(codec_id);
    if (it == _track_map.end()) {
        return;
    }
    it->second->inputFrame(frame);

    if(!_allTrackReady && !_trackReadyCallback.empty() && it->second->ready()){
        //Track由未就绪状态转换成就绪状态，我们就触发onTrackReady回调
        auto it_callback = _trackReadyCallback.find(codec_id);
        if(it_callback != _trackReadyCallback.end()){
            it_callback->second();
            _trackReadyCallback.erase(it_callback);
        }
    }

    if(!_allTrackReady && (_trackReadyCallback.empty() || _ticker.elapsedTime() > MAX_WAIT_MS)){
        _allTrackReady = true;
        _anyTrackUnReady = false;
        if(!_trackReadyCallback.empty()){
            //这是超时强制忽略未准备好的Track
            _trackReadyCallback.clear();
            //移除未准备好的Track
            for(auto it = _track_map.begin() ; it != _track_map.end() ; ){
                if(!it->second->ready()){
                    it = _track_map.erase(it);
                    continue;
                }
                ++it;
            }
        }

        if(!_track_map.empty()){
            //最少有一个有效的Track
            onAllTrackReady();
        }
    }
}

bool MediaSink::isAllTrackReady() const {
    return _allTrackReady;
}

Track::Ptr MediaSink::getTrack(TrackType type,bool trackReady) const {
    lock_guard<recursive_mutex> lck(_mtx);
    for (auto &pr : _track_map){
        if(pr.second->getTrackType() == type){
            if(!trackReady){
                return pr.second;
            }
            return pr.second->ready() ? pr.second : nullptr;
        }
    }
    return nullptr;
}


}//namespace mediakit
