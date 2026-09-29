/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "LlHlsMaker.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

LlHlsMaker::LlHlsMaker(const LlConfig &config, const LlSegmentStore::Ptr &store)
    : _config(config), _store(store) {
}

void LlHlsMaker::inputData(const LlCmafChunk::Ptr &chunk, uint64_t reset_stamp) {
    if (!chunk || !chunk->data || !chunk->data->size()) {
        // track重置: 输出已累积的部分分片, 避免数据滞留在maker内
        // Track reset: emit the accumulated partial segment so no data is stranded
        if (_part_pending) {
            flushPart(reset_stamp);
        }
        return;
    }

    if (!_has_data) {
        // 首帧: 初始化时间轴
        // First frame: initialize the timeline
        _has_data = true;
        _last_dts = chunk->stamp;
        _part_base_dts = chunk->stamp;
        _segment_base_dts = chunk->stamp;
    } else if (chunk->stamp < _last_dts) {
        // 时间戳回退(推流重启), 丢弃当前片段并重置时间轴
        // Timestamp regression (publisher restarted), drop the current data and reset the timeline
        WarnL << "LL timestamp reduce: " << _last_dts << " -> " << chunk->stamp;
        _part_chunks.clear();
        _part_pending = false;
        _part_index = 0;
        _segment_started = false;
        _part_base_dts = chunk->stamp;
        _segment_base_dts = chunk->stamp;
    }

    // 结算上一段累积的数据: 新帧的dts即为上一部分分片的结束时刻,
    // 因此part时长是精确的媒体时长
    // Settle the previously accumulated data: the dts of the new frame is the end of
    // the previous partial segment, so the part duration is an exact media duration
    if (_part_pending && chunk->key_pos && (chunk->stamp - _segment_base_dts) >= (uint64_t)_config.seg_dur_ms) {
        // 关键帧且当前完整分片时长已满足: 结算当前part并结束当前完整分片,
        // 随后该关键帧会开启一个新分片, 保证每个Segment都以IDR起始
        // Key frame and the current segment is long enough: settle the current part and
        // close the segment; the key frame then opens a new segment, so every segment
        // starts with an IDR
        flushPart(chunk->stamp);
        _store->endSegment();
        _next_msn++;
        _segment_started = false;
        _part_index = 0;
    } else if (_part_pending && (chunk->stamp - _part_base_dts) >= (uint64_t)_config.part_dur_ms) {
        // 部分分片达到partDur即可切出, 完整分片保持开启
        // A partial segment is cut once partDur is reached, the full segment stays open
        flushPart(chunk->stamp);
    }

    if (!_part_pending) {
        // 开启新的部分分片
        // Open a new partial segment
        _part_base_dts = chunk->stamp;
        _part_first_is_key = chunk->key_pos;
        if (!_segment_started) {
            // 开启新的完整分片
            // Open a new full segment
            _store->beginSegment(_next_msn, chunk->stamp, getCurrentMillisecond(true));
            _segment_started = true;
            _segment_base_dts = chunk->stamp;
        }
        _part_pending = true;
    }

    _part_chunks.emplace_back(chunk);
    _last_dts = chunk->stamp;
}

void LlHlsMaker::flushPart(uint64_t end_dts) {
    if (!_part_pending || _part_chunks.empty()) {
        return;
    }
    auto duration_ms = (int)(end_dts > _part_base_dts ? (end_dts - _part_base_dts) : _config.part_dur_ms);
    if (duration_ms <= 0) {
        duration_ms = _config.part_dur_ms;
    }
    _store->addPart(_part_index++, duration_ms, _part_first_is_key, std::move(_part_chunks));
    _part_chunks.clear();
    _part_pending = false;
    _part_base_dts = end_dts;
}

void LlHlsMaker::clear() {
    _next_msn = 1;
    _has_data = false;
    _last_dts = 0;
    _segment_started = false;
    _segment_base_dts = 0;
    _part_pending = false;
    _part_base_dts = 0;
    _part_first_is_key = false;
    _part_chunks.clear();
    _part_index = 0;
}

} // namespace mediakit
