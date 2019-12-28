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

#include "HlsManager.h"
#include "Util/util.h"
using namespace toolkit;

namespace mediakit{

HlsCookieData::HlsCookieData(const MediaInfo &info) {
    _info = info;
    _manager = HlsManager::Instance().shared_from_this();
    _manager->onAddHlsPlayer(_info);
}

HlsCookieData::~HlsCookieData() {
    _manager->onAddHlsPlayer(_info);
}

void HlsCookieData::addByteUsage(uint64_t bytes) {
    _bytes += bytes;
}

////////////////////////////////////////////////////////
HlsManager::HlsManager() {}
HlsManager::~HlsManager() {}
INSTANCE_IMP(HlsManager);

void HlsManager::onAddHlsPlayer(const MediaInfo &info) {
    lock_guard<decltype(_mtx)> lck(_mtx);
    ++_player_counter[info._vhost][info._app][info._streamid]._count;
}

void HlsManager::onDelHlsPlayer(const MediaInfo &info) {
    lock_guard<decltype(_mtx)> lck(_mtx);
    auto it0 = _player_counter.find(info._vhost);
    if(it0 == _player_counter.end()){
        return;
    }
    auto it1 = it0->second.find(info._app);
    if(it1 == it0->second.end()){
        return;
    }
    auto it2 = it1->second.find(info._streamid);
    if(it2 == it1->second.end()){
        return;
    }
    if(--(it2->second._count) == 0){
        it1->second.erase(it2);
        if(it1->second.empty()){
            it0->second.erase(it1);
            if(it0->second.empty()){
                _player_counter.erase(it0);
            }
        }
    }
}

int HlsManager::hlsPlayerCount(const string &vhost, const string &app, const string &stream) {
    lock_guard<decltype(_mtx)> lck(_mtx);
    auto it0 = _player_counter.find(vhost);
    if(it0 == _player_counter.end()){
        return 0;
    }
    auto it1 = it0->second.find(app);
    if(it1 == it0->second.end()){
        return 0;
    }
    auto it2 = it1->second.find(stream);
    if(it2 == it1->second.end()){
        return 0;
    }
    return it2->second._count;
}


}//namespace mediakit

