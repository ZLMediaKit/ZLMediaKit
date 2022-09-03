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

#include "Common/MediaSource.h"
#include "Util/TimeTicker.h"
#include <atomic>

namespace mediakit {

class HlsMediaSource : public MediaSource {
public:
    friend class HlsCookieData;

    using RingType = toolkit::RingBuffer<std::string>;
    using Ptr = std::shared_ptr<HlsMediaSource>;

    HlsMediaSource(const std::string &vhost, const std::string &app, const std::string &stream_id)
        : MediaSource(HLS_SCHEMA, vhost, app, stream_id) {}
    ~HlsMediaSource() override = default;

    /**
     * 	获取媒体源的环形缓冲
     */
    const RingType::Ptr &getRing() const { return _ring; }

    /**
     * 获取播放器个数
     */
    int readerCount() override { return _ring ? _ring->readerCount() : 0; }

    /**
     * 设置或清空m3u8索引文件内容
     */
    void setIndexFile(std::string index_file) {
        if (!_ring) {
            std::weak_ptr<HlsMediaSource> weakSelf = std::dynamic_pointer_cast<HlsMediaSource>(shared_from_this());
            auto lam = [weakSelf](int size) {
                auto strongSelf = weakSelf.lock();
                if (!strongSelf) {
                    return;
                }
                strongSelf->onReaderChanged(size);
            };
            _ring = std::make_shared<RingType>(0, std::move(lam));
            regist();
        }

        //赋值m3u8索引文件内容
        std::lock_guard<std::mutex> lck(_mtx_index);
        _index_file = std::move(index_file);

        if (!_index_file.empty()) {
            _list_cb.for_each([&](const std::function<void(const std::string &str)> &cb) { cb(_index_file); });
            _list_cb.clear();
        }
    }

    /**
     * 异步获取m3u8文件
     */
    void getIndexFile(std::function<void(const std::string &str)> cb) {
        std::lock_guard<std::mutex> lck(_mtx_index);
        if (!_index_file.empty()) {
            cb(_index_file);
            return;
        }
        //等待生成m3u8文件
        _list_cb.emplace_back(std::move(cb));
    }

    /**
     * 同步获取m3u8文件
     */
    std::string getIndexFile() const {
        std::lock_guard<std::mutex> lck(_mtx_index);
        return _index_file;
    }

    void onSegmentSize(size_t bytes) { _speed[TrackVideo] += bytes; }

private:
    RingType::Ptr _ring;
    std::string _index_file;
    mutable std::mutex _mtx_index;
    toolkit::List<std::function<void(const std::string &)>> _list_cb;
};

class HlsCookieData {
public:
    using Ptr = std::shared_ptr<HlsCookieData>;

    HlsCookieData(const MediaInfo &info, const std::shared_ptr<toolkit::SockInfo> &sock_info);
    ~HlsCookieData();

    void addByteUsage(size_t bytes);
    void setMediaSource(const HlsMediaSource::Ptr &src);
    HlsMediaSource::Ptr getMediaSource() const;

private:
    void addReaderCount();

private:
    std::atomic<uint64_t> _bytes { 0 };
    MediaInfo _info;
    std::shared_ptr<bool> _added;
    toolkit::Ticker _ticker;
    std::weak_ptr<HlsMediaSource> _src;
    std::shared_ptr<toolkit::SockInfo> _sock_info;
    HlsMediaSource::RingType::RingReader::Ptr _ring_reader;
};

} // namespace mediakit
#endif // ZLMEDIAKIT_HLSMEDIASOURCE_H
