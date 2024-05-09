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
     */
    const RingType::Ptr &getRing() const { return _ring; }

    /**
     * 获取播放器个数
     */
    int readerCount() override { return _ring ? _ring->readerCount() : 0; }

    /**
     * 设置或清空m3u8索引文件内容
     */
    void setIndexFile(std::string index_file);

    /**
     * 异步获取m3u8文件
     */
    void getIndexFile(std::function<void(const std::string &str)> cb);

    /**
     * 同步获取m3u8文件
     */
    std::string getIndexFile() const {
        std::lock_guard<std::mutex> lck(_mtx_index);
        return _index_file;
    }

    void onSegmentSize(size_t bytes) { _speed[TrackVideo] += bytes; }

    void getPlayerList(const std::function<void(const std::list<toolkit::Any> &info_list)> &cb,
                       const std::function<toolkit::Any(toolkit::Any &&info)> &on_change) override {
        _ring->getInfoList(cb, on_change);
    }

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
