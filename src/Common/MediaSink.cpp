/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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


namespace mediakit{

void MediaSink::addTrack(const Track::Ptr &track_in) {
    lock_guard<mutex> lck(_mtx);
//克隆Track，只拷贝其数据，不拷贝其数据转发关系
    auto track = track_in->clone();

    weak_ptr<MediaSink> weakSelf = shared_from_this();
    track->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([weakSelf](const Frame::Ptr &frame){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        if(strongSelf->_allTrackReady){
            strongSelf->onTrackFrame(frame);
        }
    }));
    auto codec_id = track->getCodecId();
    _track_map[codec_id] = track;
    auto lam = [this,track](){
        onTrackReady(track);
    };
    if(track->ready()){
        lam();
    }else{
        _allTrackReady = false;
        _trackReadyCallback[codec_id] = lam;
    }
}

void MediaSink::inputFrame(const Frame::Ptr &frame) {
    lock_guard<mutex> lck(_mtx);
    auto codec_id = frame->getCodecId();
    auto it = _track_map.find(codec_id);
    if (it == _track_map.end()) {
        return;
    }
    it->second->inputFrame(frame);
    if(!_allTrackReady && !_trackReadyCallback.empty() && it->second->ready()){
        //Track由未就绪状态装换成就绪状态，我们就生成sdp以及rtp编码器
        auto it_callback = _trackReadyCallback.find(codec_id);
        if(it_callback != _trackReadyCallback.end()){
            it_callback->second();
            _trackReadyCallback.erase(it_callback);
        }
    }

    if(!_allTrackReady && _trackReadyCallback.empty()){
        _allTrackReady = true;
        onAllTrackReady();
    }
}

bool MediaSink::isAllTrackReady() const {
    return _allTrackReady;
}


}//namespace mediakit
