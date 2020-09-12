/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_HLSMEDIASOURCE_H
#define ZLMEDIAKIT_HLSMEDIASOURCE_H

#include <atomic>
#include "Util/TimeTicker.h"
#include "Common/MediaSource.h"
namespace mediakit{

class HlsMediaSource : public MediaSource {
public:
    friend class HlsCookieData;
    typedef RingBuffer<string> RingType;
    typedef std::shared_ptr<HlsMediaSource> Ptr;
    HlsMediaSource(const string &vhost, const string &app, const string &stream_id) : MediaSource(HLS_SCHEMA, vhost, app, stream_id){
        _reader_count = 0;
        _ring = std::make_shared<RingType>();
    }

    ~HlsMediaSource() override = default;

    /**
     * 	获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const {
        return _ring;
    }

    /**
     * 获取播放器个数
     * @return
     */
    int readerCount() override {
        return _reader_count.load();
    }

    /**
     * 生成m3u8文件时触发
     */
    void registHls(){
        if (!_is_regist) {
            _is_regist = true;
            onReaderChanged(0);
            regist();
        }

        //m3u8文件生成，发送给播放器
        decltype(_list_cb) copy;
        {
            lock_guard<mutex> lck(_mtx_cb);
            copy.swap(_list_cb);
        }
        copy.for_each([](const function<void()> &cb) {
            cb();
        });
    }

    void waitForHls(function<void()> cb){
        //等待生成m3u8文件
        lock_guard<mutex> lck(_mtx_cb);
        _list_cb.emplace_back(std::move(cb));
    }

private:
    /**
     * 修改观看者个数
     * @param add 添加海思删除
     */
    void modifyReaderCount(bool add) {
        if (add) {
            ++_reader_count;
        } else {
            --_reader_count;
        }
        onReaderChanged(_reader_count);
    }

private:
    bool _is_regist = false;
    atomic_int _reader_count;
    RingType::Ptr _ring;
    mutex _mtx_cb;
    List<function<void()> > _list_cb;
};

class HlsCookieData{
public:
    typedef std::shared_ptr<HlsCookieData> Ptr;
    HlsCookieData(const MediaInfo &info, const std::shared_ptr<SockInfo> &sock_info);
    ~HlsCookieData();
    void addByteUsage(uint64_t bytes);
private:
    void addReaderCount();
private:
    uint64_t _bytes = 0;
    MediaInfo _info;
    std::shared_ptr<bool> _added;
    weak_ptr<HlsMediaSource> _src;
    Ticker _ticker;
    std::shared_ptr<SockInfo> _sock_info;
    HlsMediaSource::RingType::RingReader::Ptr _ring_reader;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_HLSMEDIASOURCE_H
