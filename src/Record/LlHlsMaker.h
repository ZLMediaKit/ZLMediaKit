/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_LLHLSMAKER_H
#define ZLMEDIAKIT_LLHLSMAKER_H

#include <string>

#include "LlSegmentStore.h"

namespace mediakit {

/**
 * 把 fmp4 fragment 按时间和关键帧切分为部分分片(part)与完整分片(segment),
 * 并写入 LlSegmentStore
 *
 * 输入粒度: MP4MuxerMemory 每输入一帧就产出一个 moof+mdat, 因此回调粒度是"一帧"。
 *   - stamp 是该fragment所属帧的dts(毫秒), 可作为该分片片段在媒体时间轴上的起点
 *   - key_pos 表示该帧是GOP首个关键帧; 纯音频流每帧都为true
 *
 * Splits fmp4 fragments into partial segments and full segments by time and key
 * frame, and writes them into the LlSegmentStore
 *
 * Input granularity: MP4MuxerMemory produces one moof+mdat per input frame, so the
 * callback has frame granularity.
 *   - stamp is the dts (ms) of the frame inside the fragment, i.e. the start of that
 *     fragment on the media timeline
 *   - key_pos tells whether that frame is the first key frame of a GOP; for an
 *     audio-only stream it is true for every frame
 */
class LlHlsMaker {
public:
    using Ptr = std::shared_ptr<LlHlsMaker>;

    LlHlsMaker(const LlConfig &config, const LlSegmentStore::Ptr &store);

    /**
     * 输入fmp4 fragment
     * @param data fmp4字节, 为空表示track重置
     * @param len 长度
     * @param stamp 该帧的dts(毫秒)
     * @param key_pos 该帧是否为GOP首个关键帧
     * Input an fmp4 fragment
     * @param data fmp4 bytes, empty means a track reset
     * @param len Length
     * @param stamp dts of the frame (ms)
     * @param key_pos Whether the frame is the first key frame of a GOP
     */
    void inputData(const LlCmafChunk::Ptr &chunk, uint64_t reset_stamp = 0);

    /**
     * 清空状态(与store的clear成对调用)
     * Reset the state (called together with store's clear)
     */
    void clear();

private:
    /**
     * 输出当前累积的部分分片
     * @param end_dts 该部分分片的结束时间(下一位帧的dts)
     * Emit the accumulated partial segment
     * @param end_dts End time of the partial segment (dts of the next frame)
     */
    void flushPart(uint64_t end_dts);

private:
    LlConfig _config;
    LlSegmentStore::Ptr _store;

    // 下一个待分配的完整分片序号(从1开始, 以满足DASH startNumber>=1的要求)
    // The msn to be assigned to the next segment (starts from 1, to satisfy the DASH startNumber>=1 requirement)
    uint64_t _next_msn = 1;
    // 是否已经接收到过数据
    // Whether any data has been received
    bool _has_data = false;
    // 上一个接受到的帧dts, 用于检测时间戳回退
    // dts of the last received frame, used to detect timestamp regression
    uint64_t _last_dts = 0;

    // 当前完整分片是否已经在store中开启
    // Whether the current segment has been opened in the store
    bool _segment_started = false;
    // 当前完整分片的时间轴起点(毫秒)
    // Timeline base of the current segment (ms)
    uint64_t _segment_base_dts = 0;

    // 当前部分分片是否已累积数据
    // Whether the current partial segment has accumulated data
    bool _part_pending = false;
    // 当前部分分片的时间轴起点(毫秒)
    // Timeline base of the current partial segment (ms)
    uint64_t _part_base_dts = 0;
    // 当前部分分片的第一个fragment是否为关键帧(决定INDEPENDENT)
    // Whether the first fragment of the current partial segment is a key frame (decides INDEPENDENT)
    bool _part_first_is_key = false;
    // 当前部分分片的数据
    // Bytes of the current partial segment
    std::list<LlCmafChunk::Ptr> _part_chunks;
    // 当前部分分片在所属完整分片内的序号
    // Index of the current partial segment inside the owning segment
    int _part_index = 0;
};

} // namespace mediakit
#endif // ZLMEDIAKIT_LLHLSMAKER_H
