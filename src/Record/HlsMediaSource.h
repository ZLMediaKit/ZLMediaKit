/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
    typedef RingBuffer<std::string> RingType;
    typedef std::shared_ptr<HlsMediaSource> Ptr;
    HlsMediaSource(const std::string &vhost, const std::string &app, const std::string &stream_id) : MediaSource(HLS_SCHEMA, vhost, app, stream_id){}
    ~HlsMediaSource() override = default;

    /**
     * 	获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const {
        return _ring;
    }

    /**
     * 获取播放器个数
     */
    int readerCount() override {
        return _ring ? _ring->readerCount() : 0;
    }

    /**
     * 生成m3u8文件时触发
     * @param file_created 是否产生了hls文件
     */
    void registHls(bool file_created){
        if (!_is_regist) {
            _is_regist = true;
            std::weak_ptr<HlsMediaSource> weakSelf = std::dynamic_pointer_cast<HlsMediaSource>(shared_from_this());
            auto lam = [weakSelf](int size) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                strongSelf->onReaderChanged(size);
            };
            _ring = std::make_shared<RingType>(0, std::move(lam));
            onReaderChanged(0);
            regist();
        }

        if (!file_created) {
            //没产生文件
            return;
        }
        //m3u8文件生成，发送给播放器
        decltype(_list_cb) copy;
        {
            std::lock_guard<std::mutex> lck(_mtx_cb);
            copy.swap(_list_cb);
        }
        copy.for_each([](const std::function<void()> &cb) {
            cb();
        });
    }

    void waitForFile(std::function<void()> cb) {
        //等待生成m3u8文件
        std::lock_guard<std::mutex> lck(_mtx_cb);
        _list_cb.emplace_back(std::move(cb));
    }

    void onSegmentSize(size_t bytes) {
        _speed[TrackVideo] += bytes;
    }

private:
    bool _is_regist = false;
    RingType::Ptr _ring;
    std::mutex _mtx_cb;
    List<std::function<void()> > _list_cb;
};

class HlsCookieData{
public:
    typedef std::shared_ptr<HlsCookieData> Ptr;
    HlsCookieData(const MediaInfo &info, const std::shared_ptr<SockInfo> &sock_info);
    ~HlsCookieData();
    void addByteUsage(size_t bytes);

private:
    void addReaderCount();

private:
    std::atomic<uint64_t> _bytes {0};
    MediaInfo _info;
    std::shared_ptr<bool> _added;
    Ticker _ticker;
    std::shared_ptr<SockInfo> _sock_info;
    HlsMediaSource::RingType::RingReader::Ptr _ring_reader;
};


}//namespace mediakit
#endif //ZLMEDIAKIT_HLSMEDIASOURCE_H
