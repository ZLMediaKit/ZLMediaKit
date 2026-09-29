/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_HLSMEDIASOURCE_H
#define ZLMEDIAKIT_HLSMEDIASOURCE_H

#include "Common/MediaSource.h"
#include "Util/TimeTicker.h"
#include "Util/RingBuffer.h"
#include "Network/Session.h"
#include <atomic>

namespace mediakit {

class HlsMediaSource : public MediaSource {
public:
    friend class HlsCookieData;

    using RingType = toolkit::RingBuffer<std::string>;
    using Ptr = std::shared_ptr<HlsMediaSource>;

    HlsMediaSource(const std::string &schema, const MediaTuple &tuple) : MediaSource(schema, tuple) {}

    /**
     * 	获取媒体源的环形缓冲
     * 	Get the circular buffer of the media source
     
     * [AUTO-TRANSLATED:75ac76b6]
     */
    const RingType::Ptr &getRing() const { return _ring; }

    /**
     * 获取播放器个数
     * Get the number of players
     
     * [AUTO-TRANSLATED:a451c846]
     */
    int readerCount() override { return _ring ? _ring->readerCount() : 0; }

    /**
     * 设置或清空m3u8索引文件内容
     * Set or clear the m3u8 index file content
     
     * [AUTO-TRANSLATED:71db921d]
     */
    void setIndexFile(std::string index_file);

    /**
     * 异步获取m3u8文件
     * Asynchronously get the m3u8 file
     
     * [AUTO-TRANSLATED:e962b3ad]
     */
    void getIndexFile(std::function<void(const std::string &str)> cb);

    /**
     * 同步获取m3u8文件
     * Synchronously get the m3u8 file
     
     
     * [AUTO-TRANSLATED:52b228df]
     */
    std::string getIndexFile() const {
        std::lock_guard<std::mutex> lck(_mtx_index);
        return _index_file;
    }

    /**
     * 设置或清空dash mpd文件内容, 仅fmp4模式
     * Set or clear the dash mpd file content, fmp4 mode only
     */
    void setMpdFile(std::string mpd_file);

    /**
     * 异步获取dash mpd文件
     * Asynchronously get the dash mpd file
     */
    void getMpdFile(std::function<void(const std::string &str)> cb);

    /**
     * 同步获取dash mpd文件
     * Synchronously get the dash mpd file
     */
    std::string getMpdFile() const {
        std::lock_guard<std::mutex> lck(_mtx_mpd);
        return _mpd_file;
    }

    void onSegmentSize(size_t bytes) { _speed[TrackVideo] += bytes; }

    void getPlayerList(const std::function<void(const std::list<toolkit::Any> &info_list)> &cb,
                       const std::function<toolkit::Any(toolkit::Any &&info)> &on_change) override {
        _ring->getInfoList(cb, on_change);
    }

private:
    // 创建环形缓冲并注册MediaSource(幂等)
    // Create the ring buffer and register the MediaSource (idempotent)
    void registRing();

private:
    RingType::Ptr _ring;
    std::string _index_file;
    mutable std::mutex _mtx_index;
    toolkit::List<std::function<void(const std::string &)>> _list_cb;
    std::string _mpd_file;
    mutable std::mutex _mtx_mpd;
    toolkit::List<std::function<void(const std::string &)>> _list_mpd_cb;
};

class HlsCookieData {
public:
    using Ptr = std::shared_ptr<HlsCookieData>;

    HlsCookieData(const MediaInfo &info, const std::shared_ptr<toolkit::Session> &session);
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
