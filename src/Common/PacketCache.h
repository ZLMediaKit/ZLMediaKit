/*
* Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
*
* This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
*
* Use of this source code is governed by MIT-like license that can be found in the
* LICENSE file in the root of the source tree. All contributing project authors
* may be found in the AUTHORS file in the root of the source tree.
*/

#ifndef ZLMEDIAKIT_PACKET_CACHE_H_
#define ZLMEDIAKIT_PACKET_CACHE_H_

#include "Common/config.h"
#include "Util/List.h"

namespace mediakit {
/// 缓存刷新策略类
class FlushPolicy {
public:
    bool isFlushAble(bool is_video, bool is_key, uint64_t new_stamp, size_t cache_size);

private:
    // 音视频的最后时间戳
    uint64_t _last_stamp[2] = { 0, 0 };
};

/// 合并写缓存模板
/// \tparam packet 包类型
/// \tparam policy 刷新缓存策略
/// \tparam packet_list 包缓存类型
template<typename packet, typename policy = FlushPolicy, typename packet_list = toolkit::List<std::shared_ptr<packet> > >
class PacketCache {
public:
    PacketCache() { _cache = std::make_shared<packet_list>(); }

    virtual ~PacketCache() = default;

    void inputPacket(uint64_t stamp, bool is_video, std::shared_ptr<packet> pkt, bool key_pos) {
        bool flag = flushImmediatelyWhenCloseMerge();
        if (!flag && _policy.isFlushAble(is_video, key_pos, stamp, _cache->size())) {
            flush();
        }

        //追加数据到最后
        _cache->emplace_back(std::move(pkt));
        if (key_pos) {
            _key_pos = key_pos;
        }

        if (flag) {
            flush();
        }
    }

    void flush() {
        if (_cache->empty()) {
            return;
        }
        onFlush(std::move(_cache), _key_pos);
        _cache = std::make_shared<packet_list>();
        _key_pos = false;
    }

    virtual void clearCache() {
        _cache->clear();
    }

    virtual void onFlush(std::shared_ptr<packet_list>, bool key_pos) = 0;

private:
    bool flushImmediatelyWhenCloseMerge() {
        // 一般的协议关闭合并写时，立即刷新缓存，这样可以减少一帧的延时，但是rtp例外
        // 因为rtp的包很小，一个RtpPacket包中也不是完整的一帧图像，所以在关闭合并写时，
        // 还是有必要缓冲一帧的rtp(也就是时间戳相同的rtp)再输出，这样虽然会增加一帧的延时
        // 但是却对性能提升很大，这样做还是比较划算的

        GET_CONFIG(int, mergeWriteMS, General::kMergeWriteMS);
        GET_CONFIG(int, rtspLowLatency, Rtsp::kLowLatency);
        return std::is_same<packet, RtpPacket>::value ? rtspLowLatency : (mergeWriteMS <= 0);
    }

private:
    bool _key_pos = false;
    policy _policy;
    std::shared_ptr<packet_list> _cache;
};
}

#endif //ZLMEDIAKIT_PACKET_CACHE_H_
