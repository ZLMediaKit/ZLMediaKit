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


#ifndef ZLMEDIAKIT_HLSMANAGER_H
#define ZLMEDIAKIT_HLSMANAGER_H

#include <memory>
#include <string>
#include <mutex>
#include "Common/MediaSource.h"
using namespace std;

namespace mediakit{

class HlsManager;
class HlsCookieData{
public:
    HlsCookieData(const MediaInfo &info);
    ~HlsCookieData();
    void addByteUsage(uint64_t bytes);
private:
    uint64_t _bytes = 0;
    MediaInfo _info;
    std::shared_ptr<HlsManager> _manager;
};


class HlsManager : public std::enable_shared_from_this<HlsManager>{
public:
    friend class HlsCookieData;
    ~HlsManager();
    static HlsManager& Instance();

    /**
     * 获取hls播放器个数
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream 流id
     * @return 播放器个数
     */
    int hlsPlayerCount(const string &vhost,const string &app,const string &stream);
private:
    void onAddHlsPlayer(const MediaInfo &info);
    void onDelHlsPlayer(const MediaInfo &info);
    HlsManager();
private:
    class HlsPlayerCounter{
    private:
        friend class HlsManager;
        int _count = 0;
    };
private:
    recursive_mutex _mtx;
    unordered_map<string/*vhost*/,unordered_map<string/*app*/,
            unordered_map<string/*stream*/,HlsPlayerCounter> > > _player_counter;


};

}//namespace mediakit
#endif //ZLMEDIAKIT_HLSMANAGER_H
