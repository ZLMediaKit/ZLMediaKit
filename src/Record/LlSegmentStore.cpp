/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <ctime>
#include <iomanip>
#include <sstream>
#include "Common/config.h"
#include "Common/Parser.h"
#include "HlsMediaSource.h"
#include "Util/util.h"
#include "Util/TimeTicker.h"
#include "LlSegmentStore.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

namespace {

// 把墙钟毫秒格式化为EXT-X-PROGRAM-DATE-TIME所需的UTC ISO 8601字符串
// Format wall-clock milliseconds as the UTC ISO 8601 string required by EXT-X-PROGRAM-DATE-TIME
string toProgramDateTimeStr(uint64_t wall_clock_ms) {
    auto sec = (time_t)(wall_clock_ms / 1000);
    struct tm tm_utc;
#if defined(_WIN32)
    gmtime_s(&tm_utc, &sec);
#else
    gmtime_r(&sec, &tm_utc);
#endif
    char buf[32];
    auto len = strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%S", &tm_utc);
    if (!len) {
        return "";
    }
    char msec[8];
    snprintf(msec, sizeof(msec), ".%03dZ", (int)(wall_clock_ms % 1000));
    return string(buf, len) + msec;
}

// 输出dash的xs:duration, 形如 PT10.500S
// Emit a dash xs:duration, e.g. PT10.500S
string dashDuration(double seconds) {
    char buf[64];
    snprintf(buf, sizeof(buf), "PT%.3fS", seconds);
    return buf;
}

string xmlEscape(const string &str) {
    string ret;
    ret.reserve(str.size());
    for (auto &ch : str) {
        switch (ch) {
            case '&': ret += "&amp;"; break;
            case '<': ret += "&lt;"; break;
            case '>': ret += "&gt;"; break;
            case '"': ret += "&quot;"; break;
            case '\'': ret += "&apos;"; break;
            default: ret += ch; break;
        }
    }
    return ret;
}

int initialPartTargetMs(const LlConfig &config) {
    // 帧时间戳量化会使实际Part略长于配置阈值；给初始声明留出余量，避免第一批Part超出PART-TARGET。
    // Frame timestamp quantization can make a Part slightly longer than its configured threshold.
    // Leave an initial margin so the first emitted Parts stay within PART-TARGET.
    auto margin_ms = config.part_dur_ms / 6;
    if (margin_ms < 50) {
        margin_ms = 50;
    }
    return config.part_dur_ms + margin_ms;
}

// 时间戳提供可读性，递增序号保证进程内唯一；随机后缀避免进程重启后与旧缓存URL碰撞。
// The timestamp is readable, the sequence is unique in-process, and the random suffix
// prevents collisions with URLs from a previous process lifetime.
string makeCacheBuster() {
    static atomic<uint64_t> s_sequence { 0 };
    return to_string(getCurrentMillisecond(true)) + "-" + to_string(++s_sequence) + "-" + makeRandStr(12);
}

} // namespace

LlConfig LlConfig::load() {
    LlConfig config;
    GET_CONFIG(float, part_dur, LlCmaf::kPartDuration);
    GET_CONFIG(float, part_hold_back, LlCmaf::kPartHoldBack);
    GET_CONFIG(float, seg_dur, LlCmaf::kSegmentDuration);
    GET_CONFIG(int, seg_num, LlCmaf::kSegmentNum);
    GET_CONFIG(uint32_t, blocking_timeout_ms, LlCmaf::kBlockingTimeoutMs);

    config.part_dur_ms = (int)(part_dur * 1000);
    if (config.part_dur_ms < 50) {
        // 部分分片过短会产生大量小请求, 这里兜底为50ms
        // Too short partial segments would produce a lot of small requests, clamp to 50ms
        config.part_dur_ms = 50;
    }
    config.part_hold_back = part_hold_back > 0 ? part_hold_back : 3.0f;
    config.seg_dur_ms = (int)(seg_dur * 1000);
    if (config.seg_dur_ms < config.part_dur_ms) {
        config.seg_dur_ms = config.part_dur_ms;
    }
    config.seg_num = seg_num > 1 ? seg_num : 1;
    config.blocking_timeout_ms = blocking_timeout_ms;
    return config;
}

LlSegmentStore::LlSegmentStore(const LlConfig &config)
        : _config(config), _max_part_duration_ms(initialPartTargetMs(config)),
            _cache_buster(makeCacheBuster()) {
}

LlSegmentStore::EventListener::~EventListener() {
    detach();
}

void LlSegmentStore::EventListener::detach() {
    if (!_attached.exchange(false)) {
        return;
    }
    if (auto store = _store.lock()) {
        store->removeEventListener(_type, _id);
    }
}

LlSegmentStore::EventListener::Ptr LlSegmentStore::addEventListener(EventType type, EventCallback cb) {
    if (!cb) {
        return nullptr;
    }
    auto id = ++_next_event_listener_id;
    {
        std::lock_guard<std::mutex> lck(_event_mtx);
        _event_listeners[(size_t)type].emplace(id, std::move(cb));
    }
    startTimeoutTimer();
    return std::shared_ptr<EventListener>(new EventListener(shared_from_this(), type, id));
}

void LlSegmentStore::startTimeoutTimer() {
    std::lock_guard<std::mutex> lck(_event_mtx);
    if (_timeout_timer) {
        return;
    }
    // 定时器仅负责粗粒度扫描，具体超时判断由每个LlCmafPlayer根据自己的截止时间完成。
    _timeout_timer = std::make_shared<Timer>(3.0f, [this]() {
        emitEvent(EventType::Timeout);
        return true;
    }, nullptr);
}

void LlSegmentStore::removeEventListener(EventType type, uint64_t id) {
    std::lock_guard<std::mutex> lck(_event_mtx);
    _event_listeners[(size_t)type].erase(id);
}

void LlSegmentStore::emitEvent(EventType type) {
    ++_event_versions[(size_t)type];
    std::vector<EventCallback> callbacks;
    {
        std::lock_guard<std::mutex> lck(_event_mtx);
        auto &listeners = _event_listeners[(size_t)type];
        callbacks.reserve(listeners.size());
        for (auto &listener : listeners) {
            callbacks.emplace_back(listener.second);
        }
    }
    for (auto &cb : callbacks) {
        cb();
    }
}

void LlSegmentStore::notifyCmafChunk() {
    emitEvent(EventType::CmafChunk);
}

size_t LlPart::getSize() const {
    size_t ret = 0;
    for (auto &chunk : chunks) {
        if (chunk && chunk->data) {
            ret += chunk->data->size();
        }
    }
    return ret;
}

std::list<Buffer::Ptr> LlPart::getBuffers() const {
    std::list<Buffer::Ptr> ret;
    for (auto &chunk : chunks) {
        if (chunk && chunk->data && chunk->data->size()) {
            ret.emplace_back(chunk->data);
        }
    }
    return ret;
}

size_t LlSegment::getSize() const {
    size_t ret = 0;
    for (auto &part : parts) {
        ret += part->getSize();
    }
    return ret;
}

std::list<Buffer::Ptr> LlSegment::getBuffers() const {
    std::list<Buffer::Ptr> ret;
    for (auto &part : parts) {
        auto buffers = part->getBuffers();
        ret.splice(ret.end(), buffers);
    }
    return ret;
}

void LlSegmentStore::setInitSegment(std::string data) {
    std::lock_guard<std::mutex> lck(_mtx);
    _init_segment = std::move(data);
}

void LlSegmentStore::setCodecs(std::string codecs) {
    std::lock_guard<std::mutex> lck(_mtx);
    _codecs = std::move(codecs);
}

void LlSegmentStore::setHaveVideo(bool have_video) {
    std::lock_guard<std::mutex> lck(_mtx);
    _have_video = have_video;
}

bool LlSegmentStore::haveVideo() const {
    return _have_video;
}

void LlSegmentStore::beginSegment(uint64_t msn, uint64_t start_stamp, uint64_t wall_clock_ms) {
    std::lock_guard<std::mutex> lck(_mtx);
    auto seg = std::make_shared<LlSegment>();
    seg->msn = msn;
    seg->start_stamp = start_stamp;
    GET_CONFIG(bool, program_date_time, Hls::kProgramDateTime);
    if (program_date_time) {
        seg->program_date_time = toProgramDateTimeStr(wall_clock_ms);
    }
    _building = std::move(seg);
}

void LlSegmentStore::addPart(int index, int duration_ms, bool independent, std::list<LlCmafChunk::Ptr> chunks) {
    {
        std::lock_guard<std::mutex> lck(_mtx);
        if (!_building) {
            return;
        }
        auto part = std::make_shared<LlPart>();
        part->msn = _building->msn;
        part->index = index;
        part->duration_ms = duration_ms;
        part->independent = independent;
        part->chunks = std::move(chunks);
        if (duration_ms > _max_part_duration_ms) {
            // PART-TARGET不得短于已发布的Part；保留流内最大值，避免滑窗淘汰后目标时长回退。
            // PART-TARGET must not be shorter than an emitted Part; keep the stream high-water mark
            // so it does not decrease when older segments leave the sliding window.
            _max_part_duration_ms = duration_ms;
        }

        _building->duration_ms += part->duration_ms;
        _building->parts.emplace_back(std::move(part));
    }
    emitEvent(EventType::Part);
}

void LlSegmentStore::endSegment() {
    {
        std::lock_guard<std::mutex> lck(_mtx);
        if (!_building) {
            return;
        }
        if (_building->parts.empty()) {
            // 空分片直接丢弃, 避免播放列表里出现零时长的分片
            // Drop empty segments to avoid zero-duration entries in the playlist
            _building = nullptr;
            return;
        }
        _building->completed = true;
        _segments.emplace_back(std::move(_building));
        _building = nullptr;
        evictIfNeed();
    }
    emitEvent(EventType::Segment);
}

void LlSegmentStore::dropBuildingSegment() {
    std::lock_guard<std::mutex> lck(_mtx);
    _building = nullptr;
}

void LlSegmentStore::evictIfNeed() {
    // 调用者已持锁
    // The caller already holds the lock
    while ((int)_segments.size() > _config.seg_num) {
        _evicted_duration_ms += (uint64_t)_segments.front()->duration_ms;
        _segments.pop_front();
    }
}

void LlSegmentStore::clear() {
    std::lock_guard<std::mutex> lck(_mtx);
    _segments.clear();
    _building = nullptr;
    _init_segment.clear();
    _cache_buster = makeCacheBuster();
    _max_part_duration_ms = initialPartTargetMs(_config);
    _evicted_duration_ms = 0;
    _ast_wall_ms = 0;
    _dash_last_emit_ms = 0;
}

bool LlSegmentStore::isStarted() const {
    std::lock_guard<std::mutex> lck(_mtx);
    return !_init_segment.empty() && (!_segments.empty() || _building);
}

LlSegment::Ptr LlSegmentStore::getSegment(uint64_t msn) const {
    std::lock_guard<std::mutex> lck(_mtx);
    for (auto &seg : _segments) {
        if (seg->msn == msn) {
            return seg;
        }
    }
    if (_building && _building->msn == msn) {
        return _building;
    }
    return nullptr;
}

LlPart::Ptr LlSegmentStore::getPart(uint64_t msn, int index) const {
    std::lock_guard<std::mutex> lck(_mtx);
    const LlSegment::Ptr *target = nullptr;
    for (auto &seg : _segments) {
        if (seg->msn == msn) {
            target = &seg;
            break;
        }
    }
    if (!target && _building && _building->msn == msn) {
        target = &_building;
    }
    if (!target) {
        return nullptr;
    }
    for (auto &part : (*target)->parts) {
        if (part->index == index) {
            return part;
        }
    }
    return nullptr;
}

std::string LlSegmentStore::getInitSegment() const {
    std::lock_guard<std::mutex> lck(_mtx);
    return _init_segment;
}

bool LlSegmentStore::isMsnCompleted(uint64_t msn) const {
    std::lock_guard<std::mutex> lck(_mtx);
    // 已被淘汰的分片也视为"已完成"(数据已经拿不到了, 但序号确实生产过)
    // An evicted segment also counts as completed (its data is gone, but it has been produced)
    if (!_segments.empty() && msn < _segments.front()->msn) {
        return true;
    }
    for (auto &seg : _segments) {
        if (seg->msn == msn) {
            return seg->completed;
        }
    }
    return false;
}

bool LlSegmentStore::readSegmentBuffers(const LlSegment::Ptr &seg, size_t offset, size_t max_size,
                                        std::list<Buffer::Ptr> &buffers, size_t &total_size, bool &completed) const {
    if (!seg) {
        return false;
    }
    std::lock_guard<std::mutex> lck(_mtx);
    // 必须在锁内读取data/completed: 推流线程会持续增长这两个字段
    // data/completed must be read under the lock: the publisher thread keeps growing them
    total_size = seg->getSize();
    completed = seg->completed;
    buffers.clear();
    if (offset >= total_size) {
        return true;
    }
    size_t skipped = 0;
    size_t read_size = 0;
    for (auto &part : seg->parts) {
        for (auto &chunk : part->chunks) {
            if (!chunk || !chunk->data || !chunk->data->size()) {
                continue;
            }
            auto chunk_size = chunk->data->size();
            if (offset >= skipped + chunk_size) {
                skipped += chunk_size;
                continue;
            }
            auto chunk_offset = offset > skipped ? offset - skipped : 0;
            auto append_size = std::min(chunk_size - chunk_offset, max_size - read_size);
            if (!chunk_offset && append_size == chunk_size) {
                // 整个chunk都在范围内, 直接引用原始buffer, 零拷贝
                // The whole chunk is in range, reference the original buffer, zero copy
                buffers.emplace_back(chunk->data);
            } else {
                // 只取chunk的一部分, 用BufferOffset切片, 同样零拷贝
                // Only part of the chunk is needed, slice it with BufferOffset, also zero copy
                buffers.emplace_back(std::make_shared<BufferOffset<Buffer::Ptr>>(chunk->data, chunk_offset, append_size));
            }
            read_size += append_size;
            if (read_size == max_size) {
                return true;
            }
            skipped += chunk_size;
        }
    }
    return true;
}

bool LlSegmentStore::getCompletedSegmentBuffers(uint64_t msn, std::list<Buffer::Ptr> &buffers) const {
    std::lock_guard<std::mutex> lck(_mtx);
    for (auto &seg : _segments) {
        if (seg->msn == msn && seg->completed) {
            buffers = seg->getBuffers();
            return true;
        }
    }
    return false;
}

std::string LlSegmentStore::makeHlsPlaylist() const {
    std::lock_guard<std::mutex> lck(_mtx);
    if (_init_segment.empty()) {
        return "";
    }

    // 视图中包含已完成分片以及正在生成的分片
    // The view contains the completed segments plus the one being built
    std::deque<LlSegment::Ptr> view(_segments);
    if (_building) {
        view.emplace_back(_building);
    }
    if (view.empty()) {
        return "";
    }

    int max_duration_ms = 0;
    auto part_target_ms = _max_part_duration_ms;
    auto part_hold_back_ms = part_target_ms * _config.part_hold_back;
    for (auto &seg : view) {
        auto dur = seg->completed ? seg->duration_ms : (seg->duration_ms + _config.seg_dur_ms);
        if (dur > max_duration_ms) {
            max_duration_ms = dur;
        }
    }

    ostringstream ss;
    ss << "#EXTM3U\n";
    ss << "#EXT-X-VERSION:9\n";
    ss << "#EXT-X-TARGETDURATION:" << (max_duration_ms + 999) / 1000 << "\n";
    ss << "#EXT-X-MEDIA-SEQUENCE:" << view.front()->msn << "\n";
    ss << "#EXT-X-PART-INF:PART-TARGET=" << setprecision(3) << part_target_ms / 1000.0 << "\n";
    // PART-HOLD-BACK通过llcamf.partHoldBack配置为PART-TARGET的倍数。
    // PART-HOLD-BACK is configured as a multiple of PART-TARGET by llcamf.partHoldBack.
    ss << "#EXT-X-SERVER-CONTROL:CAN-BLOCK-RELOAD=YES,PART-HOLD-BACK="
       << setprecision(3) << part_hold_back_ms / 1000.0 << "\n";
    ss << "#EXT-X-INDEPENDENT-SEGMENTS\n";
     auto cache_buster = "?session=" + _cache_buster;
     ss << "#EXT-X-MAP:URI=\"ll/init.mp4" << cache_buster << "\"\n";

    // 最新完成分片继续以 PART 形式保留一个 Segment，并与正在生成分片的 PART 相连。
    // hls.js 起播会先下载完整分片来建立媒体时间轴；Part 列表必须覆盖该完整分片和
    // live edge，播放器才能在建立时间轴后切换为 Part 下载。完成的 PART 后仍输出
    // EXTINF，供播放器将这些 Part 归属到同一个完整媒体分片并完成去重。
    // Keep the latest completed segment as PARTs alongside the building segment's PARTs. hls.js
    // starts by loading a complete fragment to establish the media timeline, so the Part list
    // must bridge that fragment and the live edge before it can switch to Part loading. Completed
    // PARTs are followed by EXTINF so the player can associate them with the complete fragment
    // and deduplicate already buffered media.
    auto latest_completed_msn = _segments.empty() ? uint64_t(-1) : _segments.back()->msn;
    for (auto &seg : view) {
        if (!seg->program_date_time.empty()) {
            ss << "#EXT-X-PROGRAM-DATE-TIME:" << seg->program_date_time << "\n";
        }
        if (seg->completed) {
            if (seg->msn == latest_completed_msn) {
                for (auto &part : seg->parts) {
                    ss << "#EXT-X-PART:DURATION=" << setprecision(3) << part->duration_ms / 1000.0
                       << ",URI=\"ll/" << seg->msn << "." << part->index << ".m4s" << cache_buster << "\"";
                    if (part->independent) {
                        ss << ",INDEPENDENT=YES";
                    }
                    ss << "\n";
                }
            }
            ss << "#EXTINF:" << setprecision(3) << seg->duration_ms / 1000.0 << ",\n"
                    << "ll/" << seg->msn << ".m4s" << cache_buster << "\n";
        } else {
            // LL-HLS 清单必须包含已生成的所有Part；PART-HOLD-BACK是播放器的目标延迟，不是服务端隐藏Part的数量。
            // The LL-HLS playlist must include every produced Part. PART-HOLD-BACK is the client's
            // playback delay target, not a number of Parts for the server to hide.
            for (auto &part : seg->parts) {
                ss << "#EXT-X-PART:DURATION=" << setprecision(3) << part->duration_ms / 1000.0
                   << ",URI=\"ll/" << seg->msn << "." << part->index << ".m4s" << cache_buster << "\"";
                if (part->independent) {
                    ss << ",INDEPENDENT=YES";
                }
                ss << "\n";
            }
            // PRELOAD-HINT 指向下一个待产出的部分分片
            // PRELOAD-HINT points to the next partial segment to be produced
                ss << "#EXT-X-PRELOAD-HINT:TYPE=PART,URI=\"ll/" << seg->msn << "." << seg->partCount() << ".m4s"
                    << cache_buster << "\"\n";
        }
    }

    // RENDITION-REPORT 的 LAST-PART 指向当前清单实际列出的最新Part。
    // RENDITION-REPORT points to the newest Part actually listed in this playlist.
    uint64_t report_msn = 0;
    int report_part = -1;
    for (auto &seg : _segments) {
        if (seg->partCount() > 0) {
            report_msn = seg->msn;
            report_part = seg->partCount() - 1;
        }
    }
    if (_building && _building->partCount() > 0) {
        report_msn = _building->msn;
        report_part = _building->partCount() - 1;
    }
    ss << "#EXT-X-RENDITION-REPORT:URI=\"hls.ll.m3u8\",LAST-MSN=" << report_msn
       << ",LAST-PART=" << report_part << "\n";
    return ss.str();
}

std::string LlSegmentStore::makeDashMpd() const {
    std::lock_guard<std::mutex> lck(_mtx);
    if (_init_segment.empty() || (_segments.empty() && (!_building || !_building->duration_ms))) {
        return "";
    }

    uint64_t total_dur_ms = 0;
    uint64_t total_bytes = 0;
    uint64_t max_seg_ms = 0;
    for (auto &seg : _segments) {
        total_dur_ms += (uint64_t)seg->duration_ms;
        total_bytes += seg->getSize();
        if ((uint64_t)seg->duration_ms > max_seg_ms) {
            max_seg_ms = (uint64_t)seg->duration_ms;
        }
    }
    auto building_duration_ms = _building ? (uint64_t)_building->duration_ms : 0;
    if (building_duration_ms) {
        total_dur_ms += building_duration_ms;
        total_bytes += _building->getSize();
        if (building_duration_ms > max_seg_ms) {
            max_seg_ms = building_duration_ms;
        }
    }
    // 在窗分片的平均码率, 作为Representation的bandwidth
    // Average bitrate of the listed segments, used as the representation bandwidth
    uint64_t bandwidth = total_dur_ms ? (total_bytes * 8 * 1000 / total_dur_ms) : 0;

    // total_dur_ms已包含当前在建分片的已产出时长，使AST和时间线的live edge保持一致。
    // total_dur_ms includes the produced duration of the building segment, keeping the AST and
    // timeline live edge consistent.
    uint64_t media_end_ms = _evicted_duration_ms + total_dur_ms;
    // SegmentTimeline用绝对媒体时间描述窗口, startNumber只负责将窗口内S条目映射回MSN。
    // SegmentTimeline uses absolute media times; startNumber maps the listed timeline entries to their MSN values.
    uint64_t now_ms = getCurrentMillisecond(true);

    // availabilityStartTime是Period时间轴的锚点，发布后不得随每次MPD刷新而移动。
    // 若持续修改AST，dash.js会把相同S条目的媒体时间映射到不同墙钟时间，最终停止
    // 调度后续分片或以异常倍率追赶。首次MPD生成时以(now - media_end)固定该锚点，
    // 后续仅更新publishTime和SegmentTimeline。
    // availabilityStartTime anchors a Period's timeline and must not move on every MPD refresh.
    // Moving it remaps identical S entries to different wall-clock times, causing dash.js to stop
    // scheduling later segments or to use abnormal catch-up rates. Fix it at the first MPD as
    // (now - media_end); later MPDs only update publishTime and SegmentTimeline.
    uint64_t target_ast_ms = now_ms > media_end_ms ? now_ms - media_end_ms : 0;
    if (_ast_wall_ms == 0) {
        _ast_wall_ms = target_ast_ms;
    }
    _dash_last_emit_ms = now_ms;
    string ast_iso = toProgramDateTimeStr(_ast_wall_ms);
    string now_iso = toProgramDateTimeStr(now_ms);

    // minimumUpdatePeriod用partDur, 体现LL的频繁刷新
    // Use partDur for minimumUpdatePeriod to reflect the LL frequent refresh
    uint64_t min_update_ms = (uint64_t)_config.part_dur_ms;
    if (min_update_ms < 200) {
        min_update_ms = 200;
    }
    // suggestedPresentationDelay至少覆盖一个完整分片、一次MPD刷新和1秒可交付缓冲，
    // 否则播放器的minBufferTime目标可能永远无法达到，导致起播失败或反复卡顿
    // Keep at least one segment, one MPD refresh and 1s of deliverable buffer ahead of the playhead;
    // otherwise minBufferTime can become unreachable and the player may fail to start or stall.
    uint64_t part_target_ms = (uint64_t)_config.part_dur_ms;
    uint64_t target_buffer_ms = part_target_ms * 2;
    if (target_buffer_ms < 300) {
        target_buffer_ms = 300;
    }
    // 当前在建Segment以availabilityTimeComplete=false公布，可通过chunked响应渐进下载；
    // 目标延迟只保留几个Part和一次MPD刷新，不再额外等待一个完整Segment。
    // The in-progress segment is advertised with availabilityTimeComplete=false and downloaded
    // progressively through a chunked response. Keep only a few Parts plus one MPD refresh.
    uint64_t suggested_delay_ms = target_buffer_ms + min_update_ms;
    auto min_delay_for_buffer_ms = min_update_ms + target_buffer_ms;
    if (suggested_delay_ms < min_delay_for_buffer_ms) {
        suggested_delay_ms = min_delay_for_buffer_ms;
    }
    // 保证播放点留在可用滑动窗口内
    // Keep the playhead inside the available sliding window
    uint64_t max_delay_ms = total_dur_ms > max_seg_ms ? total_dur_ms - max_seg_ms : total_dur_ms / 2;
    if (suggested_delay_ms > max_delay_ms) {
        suggested_delay_ms = max_delay_ms;
    }
    // 分片可渐进下载, minBufferTime按part粒度设置, 不再强制播放器等待一个完整segment。
    // Segments are progressively downloadable; use a part-sized buffer instead of waiting for a full segment.
    uint64_t min_buffer_ms = target_buffer_ms;
    uint64_t ahead_ms = suggested_delay_ms > min_update_ms
                            ? suggested_delay_ms - min_update_ms
                            : 0;
    if (min_buffer_ms > ahead_ms) {
        min_buffer_ms = ahead_ms;
    }
    if (min_buffer_ms < 100) {
        min_buffer_ms = 100;
    }
    // live edge由播放器自己的时钟推算, 可能领先最后一个列出分片最多一个分片, 窗口留余量
    // The live edge is derived from the player's own clock and can run up to one segment
    // ahead of the last listed one, keep a margin on the window
    uint64_t window_margin_ms = max_seg_ms / 2;
    if (window_margin_ms > 2000) {
        window_margin_ms = 2000;
    }
    ostringstream ss;
    ss << "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
        ss << "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" profiles=\"urn:mpeg:dash:profile:isoff-live:2011\"";
    ss << " type=\"dynamic\"";
    ss << " availabilityStartTime=\"" << ast_iso << "\"";
    ss << " publishTime=\"" << now_iso << "\"";
    uint64_t window_duration_ms = total_dur_ms;
    ss << " timeShiftBufferDepth=\"" << dashDuration((window_duration_ms + window_margin_ms) / 1000.0) << "\"";
    ss << " suggestedPresentationDelay=\"" << dashDuration(suggested_delay_ms / 1000.0) << "\"";
    ss << " minimumUpdatePeriod=\"" << dashDuration(min_update_ms / 1000.0) << "\"";
    ss << " minBufferTime=\"" << dashDuration(min_buffer_ms / 1000.0) << "\"";
    ss << ">\n";
    // 把播放器时钟钉到服务器, 避免依赖外部UTCTiming服务
    // Pin the player clock to the server, avoiding external UTCTiming servers
    ss << "  <UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2014\" value=\"" << now_iso << "\"/>\n";
    ss << "  <Period id=\"1\" start=\"PT0S\">\n";
    ss << "    <AdaptationSet id=\"1\" mimeType=\"" << (_have_video ? "video" : "audio")
       << "/mp4\" segmentAlignment=\"true\" startWithSAP=\"1\">\n";
    ss << "      <Representation id=\"r1\" bandwidth=\"" << bandwidth << "\"";
    if (!_codecs.empty()) {
        // dash.js需要codecs属性通过MediaSource能力检查
        // dash.js needs the codecs attribute to pass its MediaSource capability check
        ss << " codecs=\"" << xmlEscape(_codecs) << "\"";
    }
    ss << ">\n";
    // 使用SegmentTimeline描述当前窗口的绝对媒体时间和每个分片的精确时长。
    // startNumber仍与存储中的MSN对应; PTO保持0, 使Period时间轴与媒体时间及AST一致。
    // SegmentTimeline expresses absolute media times and exact durations for the current window.
    // startNumber maps directly to the stored MSN; PTO stays 0 so Period time, media time and AST align.
    // 追加当前在建Segment并声明availabilityTimeComplete=false，使dash.js可在它完成前请求；
    // HttpSession随后通过chunked响应持续下发新产生的CMAF数据。
    // Append the in-progress segment and declare availabilityTimeComplete=false so dash.js can
    // request it before completion; HttpSession then streams newly produced CMAF data in chunks.
    // DASH分片编号会在流重连后复用；把当前存储会话加入URL，确保浏览器不会命中上一次
    // 服务进程运行期间缓存的同编号分片。xmlEscape用于属性上下文，尤其是查询串中的'&'。
    // DASH segment numbers are reused after a stream reconnect. Include the current store session
    // in the URL so browsers cannot reuse same-numbered segments cached from a prior server run.
    // xmlEscape is required in an XML attribute context, particularly for query-string '&'.
    auto cache_buster = "?session=" + xmlEscape(_cache_buster);
    // 在建分片必须公布一个"在生成期间保持不变"的预计时长, 不能公布已产出时长:
    // dash.js按"上一个分片起始时间 + 时长 + 偏移"推算下一个分片的媒体时间, 若该时长随每次
    // MPD刷新不断增长, 推算出的时间会再次落回同一个在建分片, 于是同一个segment在传输尚未
    // 完成时就被重复请求。取最近一个已完成分片的时长作为预计时长(分片按关键帧对齐切分,
    // 相邻分片时长基本一致), 该值在建片期间恒定, 分片完成后再改用其真实时长。
    // The in-progress segment must be advertised with an expected duration that stays constant
    // while it is produced; advertising the produced-so-far duration is wrong: dash.js derives the
    // next segment from "previous segment start + duration + offset", so a duration that keeps
    // growing on every MPD refresh makes the derived media time fall back into the same
    // in-progress segment, which then gets requested again while its transfer is still running.
    // Use the duration of the latest completed segment as the estimate (segments are cut on key
    // frames, so neighbouring durations are nearly equal); it stays constant while the segment is
    // built and is replaced by the real duration once the segment completes.
    uint64_t building_expected_ms = (uint64_t)_config.seg_dur_ms;
    if (!_segments.empty()) {
        building_expected_ms = std::max(building_expected_ms, (uint64_t)_segments.back()->duration_ms);
    }
    // 再留一个Part的余量: 关键帧对齐使相邻分片时长存在帧级抖动, 若公布时长小于真实时长,
    // 分片完成时dash.js推算的媒体时间仍会落回该分片, 又触发一次重复请求。余量远小于分片时长,
    // 只会让推算时间越过本分片落到下一个分片(本分片已完整下载, 不会漏数据)。
    // Add one Part of margin: key-frame alignment makes neighbouring durations jitter by a frame;
    // if the advertised duration is smaller than the real one, dash.js's derived media time still
    // falls back into this segment when it completes, triggering another duplicate request. The
    // margin is far smaller than a segment, so it only pushes the derived time past this segment
    // into the next one (this segment is already downloaded in full, nothing is skipped).
    building_expected_ms += (uint64_t)_config.part_dur_ms;
    ss << "        <SegmentTemplate timescale=\"1000\"";
    auto start_number = _segments.empty() ? _building->msn : _segments.front()->msn;
    ss << " startNumber=\"" << start_number << "\"";
    ss << " presentationTimeOffset=\"0\"";
    ss << " media=\"ll/$Number$.m4s" << cache_buster << "\" initialization=\"ll/init.mp4" << cache_buster << "\"";
    // availabilityTimeOffset等于公布的分片时长, 则分片的可用时刻 = 结束时刻 - ATO = 其起始时刻,
    // 使在建分片从起始时刻起即可被渐进请求(与在建分片公布的是预计时长而非真实时长无关)。
    // availabilityTimeOffset equals the advertised duration, so the availability time of a segment
    // is its end time minus the offset, i.e. its start time: the in-progress segment can be
    // progressively requested right from its start, independent of the expected duration used.
    ss << " availabilityTimeOffset=\"" << setprecision(3) << building_expected_ms / 1000.0 << "\"";
    ss << " availabilityTimeComplete=\"false\">\n";
    ss << "          <SegmentTimeline>\n";
    uint64_t timeline_ms = _evicted_duration_ms;
    for (auto &seg : _segments) {
        auto duration_ms = (uint64_t)seg->duration_ms;
        ss << "            <S t=\"" << timeline_ms << "\" d=\"" << duration_ms << "\"/>\n";
        timeline_ms += duration_ms;
    }
    if (building_duration_ms) {
        // d用预计时长(在建期间恒定), 分片完成后该S条目才换成真实时长
        // d uses the expected duration (constant while being built); the real duration is used
        // once the segment completes
        ss << "            <S t=\"" << timeline_ms << "\" d=\"" << building_expected_ms << "\"/>\n";
    }
    ss << "          </SegmentTimeline>\n";
    ss << "        </SegmentTemplate>\n";
    ss << "      </Representation>\n";
    ss << "    </AdaptationSet>\n";
    ss << "  </Period>\n";
    ss << "</MPD>\n";
    return ss.str();
}

bool LlSegmentStore::parseBlockingParams(const std::string &url_args, int64_t &msn, int &part) {
    msn = -1;
    part = -1;
    auto args = Parser::parseArgs(url_args);
    auto it = args.find("_HLS_msn");
    if (it == args.end()) {
        return false;
    }
    msn = atoll(it->second.data());
    it = args.find("_HLS_part");
    if (it != args.end()) {
        part = atoi(it->second.data());
    }
    return true;
}

bool LlSegmentStore::checkBlockingCondition(int64_t msn, int part) const {
    std::lock_guard<std::mutex> lck(_mtx);
    if (_init_segment.empty()) {
        return false;
    }

    int64_t completed_msn = _segments.empty() ? -1 : (int64_t)_segments.back()->msn;

    // 仅指定msn: 该分片已完成(出现EXTINF)即满足; 正在生成的分片不算"已完成", 继续等待
    // Only msn given: satisfied once that segment is completed (its EXTINF appears);
    // a segment still being built does not count as completed, keep waiting
    if (part < 0) {
        return completed_msn >= msn;
    }

    // 指定part: _HLS_part 是客户端希望获取的下一个part编号。已生成的Part会立即加入播放列表，
    // 因此检查最新实际产出的Part是否到达请求位置。
    // With part: _HLS_part is the index of the next part the client wants to receive.
    // Every produced Part is listed immediately; check whether the newest produced Part has
    // reached the requested (msn, part).
    int64_t listable_msn = -1;
    int listable_part = -1;
    for (auto &seg : _segments) {
        if (seg->partCount() > 0) {
            listable_msn = (int64_t)seg->msn;
            listable_part = seg->partCount() - 1;
        }
    }
    if (_building && _building->partCount() > 0) {
        listable_msn = (int64_t)_building->msn;
        listable_part = _building->partCount() - 1;
    }
    if (listable_msn < 0) {
        // 尚无任何可列出的部分分片
        // No listable partial segment yet
        return false;
    }
    if (listable_msn > msn) {
        return true;
    }
    if (listable_msn < msn) {
        return false;
    }
    return listable_part >= part;
}

//////////////////////////////////// LlMediaSource ////////////////////////////////////

LlMediaSource::LlMediaSource(const MediaTuple &tuple, const LlSegmentStore::Ptr &store)
    : MediaSource(LLCMAF_SCHEMA, tuple), _store(store) {
}

void LlMediaSource::registRing() {
    if (_ring) {
        return;
    }
    std::weak_ptr<LlMediaSource> weak_self = std::static_pointer_cast<LlMediaSource>(shared_from_this());
    _ring = std::make_shared<RingType>(4096, [weak_self](int size) {
        if (auto strong_self = weak_self.lock()) {
            strong_self->onReaderChanged(size);
        }
    }, _store->getConfig().seg_num + 1);
    try {
        regist();
    } catch (std::exception &ex) {
        // 极端情况下(如resetTracks后立即重新注册)可能已存在同名媒体源, 不应让推流线程抛异常
        // In a rare case (e.g. re-registering right after resetTracks) a media source with the
        // same name may already exist; the publisher thread must not be interrupted by a throw
        WarnL << "LlMediaSource regist failed: " << ex.what();
    }
}

void LlMediaSource::inputCmafChunk(const LlCmafChunk::Ptr &chunk) {
    if (!chunk || !chunk->data || !chunk->data->size()) {
        return;
    }
    if (!_ring) {
        registRing();
    }
    // RingBuffer只承载逻辑播放器，不能再写入CMAF数据，否则会继续触发跨poller的chunk派发。
    // CMAF的可用性由Store事件和Store中的Part/Segment状态表达。
    _store->notifyCmafChunk();
}

//////////////////////////////////// LlCmafPlayer ////////////////////////////////////

LlCmafPlayer::~LlCmafPlayer() {
    if (!_reader || !_sock_info) {
        return;
    }
    uint64_t duration = (_ticker.createdTime() - _ticker.elapsedTime()) / 1000;
    WarnL << _sock_info->getIdentifier() << "(" << _sock_info->get_peer_ip() << ":" << _sock_info->get_peer_port()
          << ") LL-CMAF播放器(" << _info.shortUrl() << ")断开,耗时(s):" << duration;

    GET_CONFIG(uint32_t, iFlowThreshold, General::kFlowThreshold);
    uint64_t bytes = _bytes.load();
    if (bytes >= iFlowThreshold * 1024) {
        try {
            NOTICE_EMIT(BroadcastFlowReportArgs, Broadcast::kBroadcastFlowReport, _info, bytes, duration, true, *_sock_info);
        } catch (std::exception &ex) {
            WarnL << "Exception occurred: " << ex.what();
        }
    }
}

void LlCmafPlayer::setSession(const std::shared_ptr<Session> &session) {
    _sock_info = std::make_shared<SockInfoImp>(session);
}

void LlCmafPlayer::addByteUsage(size_t bytes) {
    _bytes += bytes;
    _ticker.resetTime();
}

void LlCmafPlayer::attach(const EventPoller::Ptr &poller) {
    std::lock_guard<std::mutex> lck(_reader_mtx);
    if (_reader) {
        return;
    }
    auto source = getSource();
    if (!source || !source->getRing()) {
        return;
    }
    _reader = source->getRing()->attach(poller, false);
    std::weak_ptr<LlCmafPlayer> weak_self = shared_from_this();

    auto sock_info = _sock_info;
    _reader->setGetInfoCB([sock_info]() {
        Any ret;
        ret.set(std::static_pointer_cast<Session>(sock_info));
        return ret;
    });
    _reader->setDetachCB([weak_self]() {
        if (auto strong_self = weak_self.lock()) {
            strong_self->onDetach();
        }
    });
    auto store = source->getStore();
    if (!store) {
        return;
    }
    for (size_t i = 0; i < _event_listeners.size(); ++i) {
        auto type = (LlSegmentStore::EventType)i;
        _event_listeners[i] = store->addEventListener(type, [weak_self, type]() {
            if (auto strong_self = weak_self.lock()) {
                strong_self->onEvent(type);
            }
        });
    }
}

void LlCmafPlayer::waitForEvent(LlSegmentStore::EventType type, uint64_t event_version, const EventPoller::Ptr &poller,
                                uint64_t timeout_ms, std::function<void()> cb) {
    std::array<uint64_t, LlSegmentStore::kEventCount> event_versions {};
    event_versions[(size_t)type] = event_version;
    waitForEvents(event_versions, { type }, poller, timeout_ms, std::move(cb));
}

void LlCmafPlayer::waitForEvents(const std::array<uint64_t, LlSegmentStore::kEventCount> &event_versions,
                                 std::initializer_list<LlSegmentStore::EventType> types, const EventPoller::Ptr &poller,
                                 uint64_t timeout_ms, std::function<void()> cb) {
    if (!poller || !cb) {
        return;
    }
    uint8_t event_mask = 0;
    for (auto type : types) {
        event_mask |= 1U << (uint8_t)type;
    }
    if (!event_mask) {
        return;
    }
    auto id = ++_next_waiter_id;
    auto ticker = std::make_shared<Ticker>();
    {
        std::lock_guard<std::mutex> lck(_waiter_mtx);
        _waiters.emplace(id, Waiter { poller, event_mask, event_versions, std::move(ticker), timeout_ms, std::move(cb) });
    }
    auto source = getSource();
    auto store = source ? source->getStore() : nullptr;
    auto current_versions = store ? store->getEventVersions() : std::array<uint64_t, LlSegmentStore::kEventCount> {};
    for (size_t i = 0; i < current_versions.size(); ++i) {
        if (!store || ((event_mask & (1U << i)) && current_versions[i] != event_versions[i])) {
            notifyWaiter(id);
            return;
        }
    }
}

void LlCmafPlayer::onEvent(LlSegmentStore::EventType type) {
    std::unordered_map<uint64_t, Waiter> waiters;
    {
        std::lock_guard<std::mutex> lck(_waiter_mtx);
        for (auto it = _waiters.begin(); it != _waiters.end();) {
            auto timeout = type == LlSegmentStore::EventType::Timeout && it->second.ticker->elapsedTime() >= it->second.timeout_ms;
            if (timeout || (it->second.event_mask & (1U << (uint8_t)type))) {
                waiters.emplace(it->first, std::move(it->second));
                it = _waiters.erase(it);
            } else {
                ++it;
            }
        }
    }
    for (auto &pr : waiters) {
        auto &waiter = pr.second;
        waiter.poller->async(std::move(waiter.cb), false);
    }
}

void LlCmafPlayer::onDetach() {
    std::unordered_map<uint64_t, Waiter> waiters;
    {
        std::lock_guard<std::mutex> lck(_waiter_mtx);
        waiters.swap(_waiters);
    }
    for (auto &pr : waiters) {
        auto &waiter = pr.second;
        waiter.poller->async(std::move(waiter.cb), false);
    }
}

void LlCmafPlayer::notifyWaiter(uint64_t id) {
    Waiter waiter;
    {
        std::lock_guard<std::mutex> lck(_waiter_mtx);
        auto it = _waiters.find(id);
        if (it == _waiters.end()) {
            return;
        }
        waiter = std::move(it->second);
        _waiters.erase(it);
    }
    waiter.poller->async(std::move(waiter.cb), false);
}

} // namespace mediakit
