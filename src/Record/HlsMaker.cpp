/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cmath>
#include <iomanip>
#include <limits>
#include "HlsMaker.h"
#include "Common/config.h"

using namespace std;

namespace mediakit {

constexpr uint64_t HlsMaker::kInvalidStamp;

static uint64_t durationToMilliseconds(float duration) {
    auto duration_ms = std::ceil(static_cast<double>(duration) * 1000);
    if (!(duration_ms > 0)) {
        return 0;
    }
    auto max_duration_ms = (std::numeric_limits<uint64_t>::max)();
    return duration_ms >= static_cast<double>(max_duration_ms) ? max_duration_ms : static_cast<uint64_t>(duration_ms);
}

HlsMaker::HlsMaker(bool is_fmp4, float seg_duration, uint32_t seg_number, bool seg_keep, float seg_max_duration) {
    _is_fmp4 = is_fmp4;
    if (_is_fmp4) {
        // 首代名称保持兼容；收到后续 init segment 时再切换到带 generation 的 URI。
        // Keep the first-generation name compatible; later init segments use generation-specific URIs.
        _current_init_segment = "init.mp4";
    }
    // 最小允许设置为0，0个切片代表点播  [AUTO-TRANSLATED:19235e8e]
    // Minimum allowed setting is 0, 0 slices represent on-demand
    _seg_number = seg_number;
    // 将配置一次性转换为毫秒并限制上界，避免热路径重复浮点计算及异常配置导致整数溢出。
    // Convert the setting to milliseconds once and cap it to avoid hot-path float work and integer overflow.
    _seg_duration_ms = durationToMilliseconds(seg_duration);
    auto seg_max_duration_ms = durationToMilliseconds(seg_max_duration);
    if (seg_max_duration_ms && seg_max_duration_ms <= _seg_duration_ms) {
        WarnL << "Invalid hls.segMaxDur: " << seg_max_duration << ", it must be greater than hls.segDur: "
              << seg_duration << "; hard limit disabled";
    } else {
        _seg_max_duration_ms = seg_max_duration_ms;
    }
    _seg_keep = seg_keep;
}

void HlsMaker::makeIndexFile(bool include_delay, bool eof) {
    GET_CONFIG(uint32_t, segDelay, Hls::kSegmentDelay);
    GET_CONFIG(uint32_t, segRetain, Hls::kSegmentRetain);
    std::deque<SegmentInfo> temp(_seg_dur_list);
    auto discontinuity_sequence = _discontinuity_sequence;
    if (!include_delay && _seg_number) {
        while (temp.size() > _seg_number) {
            // 普通与延迟索引的窗口不同，按各自裁掉的标签计算 discontinuity sequence。
            // Normal and delayed indexes have different windows, so count tags trimmed from each view.
            if (temp.front().discontinuity) {
                ++discontinuity_sequence;
            }
            temp.pop_front();
        }
    }
    uint64_t max_segment_duration = 0;
    bool has_discontinuity = discontinuity_sequence != 0;
    for (auto &segment : temp) {
        if (segment.duration_ms > max_segment_duration) {
            max_segment_duration = segment.duration_ms;
        }
        has_discontinuity = has_discontinuity || segment.discontinuity;
    }
    uint64_t index_seq;
    if (_seg_number) {
        if (include_delay) {
            if (_file_index > _seg_number + segDelay) {
                index_seq = _file_index - _seg_number - segDelay;
            } else {
                index_seq = 0LL;
            }
        } else {
            if (_file_index > _seg_number) {
                index_seq = _file_index - _seg_number;
            } else {
                index_seq = 0LL;
            }
        }
    } else {
        index_seq = 0LL;
    }

    string index_str;
    index_str.reserve(2048);
    index_str += "#EXTM3U\n";
    index_str += (_is_fmp4 ? "#EXT-X-VERSION:7\n" : "#EXT-X-VERSION:4\n");
    if (_seg_number == 0) {
        index_str += "#EXT-X-PLAYLIST-TYPE:EVENT\n";
    } else {
        index_str += "#EXT-X-ALLOW-CACHE:NO\n";
    }
    auto target_duration = max_segment_duration / 1000 + (max_segment_duration % 1000 != 0);
    index_str += "#EXT-X-TARGETDURATION:" + std::to_string(target_duration) + "\n";
    index_str += "#EXT-X-MEDIA-SEQUENCE:" + std::to_string(index_seq) + "\n";
    if (has_discontinuity) {
        index_str += "#EXT-X-DISCONTINUITY-SEQUENCE:" + std::to_string(discontinuity_sequence) + "\n";
    }
    stringstream ss;
    string last_init_segment;
    for (auto &segment : temp) {
        if (segment.discontinuity) {
            ss << "#EXT-X-DISCONTINUITY\n";
        }
        if (_is_fmp4 && segment.init_segment != last_init_segment) {
            ss << "#EXT-X-MAP:URI=\"" << segment.init_segment << "\"\n";
            last_init_segment = segment.init_segment;
        }
        ss << "#EXTINF:" << std::setprecision(3) << segment.duration_ms / 1000.0 << ",\n" << segment.file_name << "\n";
    }
    index_str += ss.str();

    if (eof) {
        index_str += "#EXT-X-ENDLIST\n";
    }
    onWriteHls(index_str, include_delay);
}

void HlsMaker::inputInitSegment(const char *data, size_t len) {
    if (!_is_fmp4) {
        throw std::invalid_argument("Only fmp4-hls can input init segment");
    }
    if (segmentOpened()) {
        // init segment 不能在媒体切片中途切换；先固化旧 generation，再开启新时间线。
        // An init segment cannot change mid-segment; finalize the old generation before starting a new timeline.
        flushLastSegment(false);
        _discontinuity = true;
    }
    if (_init_segment_index == 0) {
        _current_init_segment = "init.mp4";
    } else {
        _current_init_segment = "init-" + std::to_string(_init_segment_index) + ".mp4";
    }
    ++_init_segment_index;
    onWriteInitSegment(data, len);
}

void HlsMaker::inputData(const char *data, size_t len, uint64_t timestamp, bool is_idr_fast_packet) {
    if (!data || !len) {
        // resetTracks时触发此逻辑  [AUTO-TRANSLATED:0ba915ed]
        // This logic is triggered when resetTracks is called
        auto had_timeline = segmentOpened() || !_seg_dur_list.empty();
        flushLastSegment(false);
        resetSegmentState();
        _discontinuity = had_timeline;
        return;
    }

    auto key_packet = is_idr_fast_packet;
    if (segmentOpened() && timestamp < _last_timestamp) {
        // 时间戳回退代表时间线发生变化，旧切片必须先结束，避免混入新时间线数据。
        // A timestamp rollback starts a new timeline, so close the old segment before writing new-timeline data.
        WarnL << "Timestamp reduce: " << _last_timestamp << " -> " << timestamp;
        flushLastSegment(false);
        _discontinuity = true;
        openSegment(timestamp, false);
    } else if (!segmentOpened()) {
        if (!shouldOpenFirstSegment(timestamp, key_packet)) {
            return;
        }
        openSegment(timestamp, _file_index == 0 && key_packet);
    } else if (shouldRotateSegment(timestamp, key_packet)) {
        flushLastSegment(false);
        openSegment(timestamp, false);
    }

    if (segmentOpened()) {
        // 存在切片才写入数据。
        // Write data only when a segment is open.
        onWriteSegment(data, len);
        _last_timestamp = timestamp;
    }
}

void HlsMaker::delOldSegment() {
    GET_CONFIG(uint32_t, segDelay, Hls::kSegmentDelay);
    if (_seg_number == 0 || _seg_keep) {
        // 如果设置为保留0个切片，则认为是保存为点播；或者设置为一直保存，就不删除  [AUTO-TRANSLATED:5bf20108]
        // If set to keep 0 or all slices, it is considered to be saved as on-demand
        return;
    }
    // 在hls m3u8索引文件中,我们保存的切片个数跟_seg_number相关设置一致  [AUTO-TRANSLATED:b14b5b98]
    // In the hls m3u8 index file, the number of slices we save is consistent with the _seg_number setting
    if (_file_index > _seg_number + segDelay) {
        // 删除带标签的历史切片时推进基准序号，避免滑动窗口改变剩余切片的 discontinuity sequence。
        // Advance the base when removing a tagged segment so remaining segments keep stable sequence numbers.
        if (_seg_dur_list.front().discontinuity) {
            ++_discontinuity_sequence;
        }
        _seg_dur_list.pop_front();
    }
    GET_CONFIG(uint32_t, segRetain, Hls::kSegmentRetain);
    // 但是实际保存的切片个数比m3u8所述多若干个,这样做的目的是防止播放器在切片删除前能下载完毕  [AUTO-TRANSLATED:1688f857]
    // However, the actual number of slices saved is a few more than what is stated in the m3u8, this is done to prevent the player from downloading the slices before they are deleted
    if (_file_index > _seg_number + segDelay + segRetain) {
        onDelSegment(_file_index - _seg_number - segDelay - segRetain - 1);
    }
}

void HlsMaker::openSegment(uint64_t stamp, bool fast_register_pending) {
    _last_file_name = onOpenSegment(_file_index++);
    _pending_first_stamp = kInvalidStamp;
    if (_last_file_name.empty()) {
        _segment_start_stamp = kInvalidStamp;
        _last_timestamp = 0;
        _segment_init_segment.clear();
        _fast_register_pending = false;
        return;
    }
    _segment_start_stamp = stamp;
    _last_timestamp = stamp;
    _segment_init_segment = _current_init_segment;
    _fast_register_pending = fast_register_pending;
}

void HlsMaker::flushLastSegment(bool eof){
    GET_CONFIG(uint32_t, segDelay, Hls::kSegmentDelay);
    if (!segmentOpened()) {
        // 不存在上个切片  [AUTO-TRANSLATED:d81fe08e]
        // There is no previous slice
        if (eof && !_seg_dur_list.empty()) {
            // reset 可能已经关闭最后一个切片；EOF 仍需重写现有索引并声明不再追加。
            // Reset may have closed the last segment already; EOF must still finalize the existing indexes.
            makeIndexFile(false, true);
            if (segDelay) {
                makeIndexFile(true, true);
            }
        }
        return;
    }
    // 文件创建到最后一次数据写入的时间即为切片长度  [AUTO-TRANSLATED:1f85739c]
    // The time from file creation to the last data write is the slice length
    auto seg_dur = _last_timestamp >= _segment_start_stamp ? _last_timestamp - _segment_start_stamp : 0;
    if (!seg_dur) {
        seg_dur = 100;
    }
    auto discontinuity = _discontinuity;
    auto init_segment = _segment_init_segment;
    _seg_dur_list.emplace_back(seg_dur, std::move(_last_file_name), init_segment, discontinuity);
    _discontinuity = false;
    _segment_start_stamp = kInvalidStamp;
    _pending_first_stamp = kInvalidStamp;
    _last_timestamp = 0;
    _last_file_name.clear();
    _segment_init_segment.clear();
    _fast_register_pending = false;
    delOldSegment();
    // 先flush ts切片，否则可能存在ts文件未写入完毕就被访问的情况  [AUTO-TRANSLATED:f8d6dc87]
    // Flush the ts slice first, otherwise there may be a situation where the ts file is not written completely before it is accessed
    onFlushLastSegment(seg_dur, discontinuity, init_segment);
    // 然后写m3u8文件  [AUTO-TRANSLATED:67200ce1]
    // Then write the m3u8 file
    makeIndexFile(false, eof);
    // 写入切片延迟的m3u8文件  [AUTO-TRANSLATED:b1f12e43]
    // Write the m3u8 file with slice delay
    if (segDelay) {
        makeIndexFile(true, eof);
    }
}

void HlsMaker::resetSegmentState() {
    _last_timestamp = 0;
    _segment_start_stamp = kInvalidStamp;
    _pending_first_stamp = kInvalidStamp;
    _last_file_name.clear();
    _segment_init_segment.clear();
    _fast_register_pending = false;
}

bool HlsMaker::segmentOpened() const {
    return _segment_start_stamp != kInvalidStamp && !_last_file_name.empty();
}

bool HlsMaker::shouldOpenFirstSegment(uint64_t timestamp, bool key_packet) {
    // 首片优先等待关键帧；仅在配置了绝对最大时长时，才允许无关键帧兜底开片。
    // Prefer a key frame for the first segment; only a configured hard limit permits fallback opening without one.
    if (key_packet) {
        _pending_first_stamp = kInvalidStamp;
        return true;
    }

    // 等待期间发生时间戳回退时，从新时间线重新计时。
    // Restart the wait timer on a timestamp rollback while no segment is open.
    if (_pending_first_stamp == kInvalidStamp || timestamp < _pending_first_stamp) {
        _pending_first_stamp = timestamp;
        return false;
    }

    if (_seg_max_duration_ms && timestamp - _pending_first_stamp >= _seg_max_duration_ms) {
        _pending_first_stamp = kInvalidStamp;
        return true;
    }
    return false;
}

bool HlsMaker::shouldRotateSegment(uint64_t timestamp, bool key_packet) const {
    if (_fast_register_pending && key_packet) {
        // 保留 fastRegister 的动态配置语义：首片仍有效时，以当前配置决定是否在下一关键帧结束。
        // Preserve dynamic fastRegister behavior by checking the current setting at the next key frame.
        GET_CONFIG(bool, fastRegister, Hls::kFastRegister);
        if (fastRegister) {
            return true;
        }
    }

    // 达到目标时长后优先在关键帧切片；仅在启用绝对最大时长时允许从非关键帧强制切开。
    // Prefer a key-frame boundary after the target; only an enabled hard limit permits forced non-key rotation.
    auto duration = timestamp - _segment_start_stamp;
    if (duration < _seg_duration_ms) {
        return false;
    }
    return key_packet || (_seg_max_duration_ms && duration >= _seg_max_duration_ms);
}

bool HlsMaker::isLive() const {
    return _seg_number != 0;
}

bool HlsMaker::isKeep() const {
    return _seg_keep;
}

bool HlsMaker::isFmp4() const {
    return _is_fmp4;
}

const std::string &HlsMaker::getCurrentInitSegment() const {
    return _current_init_segment;
}

void HlsMaker::clear() {
    _file_index = 0;
    _discontinuity_sequence = 0;
    _seg_dur_list.clear();
    _discontinuity = false;
    resetSegmentState();
}

}//namespace mediakit
