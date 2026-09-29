/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdio>
#include <ctime>
#include <iomanip>
#include "HlsMaker.h"
#include "Common/config.h"
#include "Util/util.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

namespace {

// 将毫秒级系统时间格式化为EXT-X-PROGRAM-DATE-TIME所需的ISO 8601字符串(UTC)
// 形如 2010-02-19T14:54:23.031Z，格式要求见RFC 8216第4.3.2.6节
// Format wall-clock milliseconds as the ISO 8601 string required by EXT-X-PROGRAM-DATE-TIME (UTC)
// e.g. 2010-02-19T14:54:23.031Z, as specified in RFC 8216 section 4.3.2.6
string toProgramDateTimeStr(uint64_t wall_clock_ms) {
    // 统一以UTC输出(尾缀Z)，避免依赖进程启动时刻的时区快照：
    // ZLToolKit的getGMTOff()不含夏令时修正，且在夏令时切换后不会刷新，会导致标注的偏移与实际时刻错位
    // Always emit UTC (with the trailing Z) instead of relying on the timezone snapshot taken at startup:
    // ZLToolKit's getGMTOff() excludes the DST correction and is never refreshed across DST transitions,
    // which would make the declared offset disagree with the actual instant
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
        // 理论上不会发生，缓冲区足以容纳固定长度的日期时间
        // Should never happen; the buffer is large enough for the fixed-length date-time
        return "";
    }
    char msec[8];
    snprintf(msec, sizeof(msec), ".%03dZ", (int)(wall_clock_ms % 1000));
    return string(buf, len) + msec;
}

} // namespace

// 由切片数据流驱动的两次mpd重发布之间的最小间隔(ms)
// Minimum interval(ms) between two mpd republish driven by the segment data flow
static const uint64_t kDashRefreshMs = 500;

// 输出dash的xs:duration，形如 PT10.500S
// Emit a dash xs:duration, e.g. PT10.500S
static string dashDuration(double seconds) {
    char buf[64];
    snprintf(buf, sizeof(buf), "PT%.3fS", seconds);
    return buf;
}

static string dashXmlEscape(const string &str) {
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

HlsMaker::HlsMaker(bool is_fmp4, float seg_duration, uint32_t seg_number, bool seg_keep) {
    _is_fmp4 = is_fmp4;
    // 最小允许设置为0，0个切片代表点播  [AUTO-TRANSLATED:19235e8e]
    // Minimum allowed setting is 0, 0 slices represent on-demand
    _seg_number = seg_number;
    _seg_duration = seg_duration;
    _seg_keep = seg_keep;
}

void HlsMaker::makeIndexFile(bool include_delay, bool eof) {
    GET_CONFIG(uint32_t, segDelay, Hls::kSegmentDelay);
    GET_CONFIG(uint32_t, segRetain, Hls::kSegmentRetain);
    std::deque<HlsSegmentInfo> temp(_seg_dur_list);
    if (!include_delay && _seg_number) {
        while (temp.size() > _seg_number) {
            temp.pop_front();
        }
    }
    int maxSegmentDuration = 0;
    for (auto &info : temp) {
        if (info.duration_ms > maxSegmentDuration) {
            maxSegmentDuration = info.duration_ms;
        }
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
    index_str += "#EXT-X-TARGETDURATION:" + std::to_string((maxSegmentDuration + 999) / 1000) + "\n";
    index_str += "#EXT-X-MEDIA-SEQUENCE:" + std::to_string(index_seq) + "\n";
    if (_is_fmp4) {
        index_str += "#EXT-X-MAP:URI=\"init.mp4\"\n";
    }

    stringstream ss;
    for (auto &info : temp) {
        // EXT-X-PROGRAM-DATE-TIME只作用于其后的第一个切片，故每个切片前都写入
        // EXT-X-PROGRAM-DATE-TIME applies only to the next segment, so write it before each one
        if (!info.program_date_time.empty()) {
            ss << "#EXT-X-PROGRAM-DATE-TIME:" << info.program_date_time << "\n";
        }
        ss << "#EXTINF:" << std::setprecision(3) << info.duration_ms / 1000.0 << ",\n" << info.name << "\n";
    }
    index_str += ss.str();

    if (eof) {
        index_str += "#EXT-X-ENDLIST\n";
    }
    onWriteHls(index_str, include_delay);
}

void HlsMaker::makeDashFile(bool eof, bool memory_only) {
    if (!_is_fmp4) {
        // dash依赖fmp4切片，仅在fmp4模式下生成
        // Dash relies on the fmp4 segments, only generated in fmp4 mode
        return;
    }

    std::deque<HlsSegmentInfo> temp(_seg_dur_list);
    // 首个列出切片在dash媒体时间轴上的起始时间
    // Start time of the first listed segment on the dash media timeline
    uint64_t time_offset = _dash_timeline_offset_ms;
    if (_seg_number) {
        while (temp.size() > _seg_number) {
            time_offset += (uint64_t)temp.front().duration_ms;
            temp.pop_front();
        }
    }
    if (temp.empty()) {
        return;
    }

    uint64_t total_dur_ms = 0;
    uint64_t total_bytes = 0;
    uint64_t max_seg_ms = 0;
    for (auto &info : temp) {
        total_dur_ms += (uint64_t)info.duration_ms;
        total_bytes += (uint64_t)info.bytes;
        if ((uint64_t)info.duration_ms > max_seg_ms) {
            max_seg_ms = (uint64_t)info.duration_ms;
        }
    }
    // 在窗分片的平均码率，作为Representation的bandwidth
    // Average bitrate of the listed segments, used as the representation bandwidth
    uint64_t bandwidth = total_dur_ms ? (total_bytes * 8 * 1000 / total_dur_ms) : 0;

    bool is_live = isLive() && !eof;
    // 最后一个可用分片在dash媒体时间轴上的末尾
    // End of the last available segment on the dash media timeline
    uint64_t media_end_ms = time_offset + total_dur_ms;
    uint64_t now_ms = getCurrentMillisecond(true);

    // dash媒体时间轴到墙钟的映射为:
    //   wall_clock(presentation_time) = availabilityStartTime + presentation_time
    // 把availabilityStartTime锚定到(now - media_end)，live edge正好落在最后一个可用分片的末尾
    //
    // 媒体时间轴的推进速度通常并不等于墙钟(丢帧、源端抖动、时钟校正)，所以锚点每次发布都要修正。
    // 一次性对齐全部偏差会让live edge阶跃：播放器测到延迟跳变后用live catch-up变速(0.5x/1.5x)追赶，
    // 1.5倍速消费必然快于分片生产，缓冲被吃光后waiting，延迟又变大，形成自激振荡。
    // 因此把修正限制在"流逝时间的一个比例"内，由周期性刷新把余量摊平
    // The dash media timeline is mapped to the wall clock by:
    //   wall_clock(presentation_time) = availabilityStartTime + presentation_time
    // Anchoring availabilityStartTime to (now - media_end) puts the live edge
    // exactly at the end of the last available segment.
    //
    // The media timeline usually does NOT advance at exactly the wall clock rate
    // (lost frames, source jitter, wall clock corrections), so the anchor has to
    // be corrected on every publish. Correcting the whole difference at once
    // makes the live edge step: the player measures a latency jump and answers
    // with a live catch-up playback rate change (0.5x/1.5x), which consumes the
    // buffer faster than the segments are produced and ends up starving it on
    // every segment. Limit the correction to a fraction of the elapsed time and
    // let the periodic mpd refresh spread the rest, the live edge then advances
    // smoothly at the media rate.
    uint64_t target_ast_ms = now_ms > media_end_ms ? now_ms - media_end_ms : 0;
    int64_t ast_diff = (int64_t)target_ast_ms - (int64_t)_ast_wall_ms;
    if (_ast_wall_ms == 0 || _dash_last_emit_ms == 0 || now_ms <= _dash_last_emit_ms) {
        // 首次发布(或reset后): 直接对齐
        // First publish (or after a reset): align right away
        _ast_wall_ms = target_ast_ms;
    } else if ((uint64_t)(ast_diff < 0 ? -ast_diff : ast_diff) > max_seg_ms * 3) {
        // 偏差过大，限速器永远追不上: 直接对齐
        // Way out of sync, the rate limiter would never catch up: realign
        _ast_wall_ms = target_ast_ms;
    } else {
        uint64_t max_step_ms = (now_ms - _dash_last_emit_ms) / 4;
        if (max_step_ms < 50) {
            max_step_ms = 50;
        }
        if (ast_diff > (int64_t)max_step_ms) {
            ast_diff = (int64_t)max_step_ms;
        } else if (ast_diff < -(int64_t)max_step_ms) {
            ast_diff = -(int64_t)max_step_ms;
        }
        _ast_wall_ms += (uint64_t)ast_diff;
    }
    _dash_last_emit_ms = now_ms;
    string ast_iso = toProgramDateTimeStr(_ast_wall_ms);
    string now_iso = toProgramDateTimeStr(now_ms);

    // 播放器至少要能缓冲一个完整分片
    // The player must be able to buffer at least one full segment
    uint64_t min_buffer_ms = max_seg_ms > 2000 ? max_seg_ms : 2000;
    // 让播放点落后live edge: 2倍最大分片时长，至少4秒。同时必须高出滑动窗口底部至少一个整片，
    // 否则正在播放的就是即将过期的那一片，dash.js拒拉后停顿，等窗口滑动才恢复(周期性顿挫)
    // Keep the play head behind the live edge: 2x the max segment duration, at
    // least 4 seconds. It must also stay at least one full segment above the
    // bottom of the sliding window, otherwise the segment being played is the
    // one that is about to expire and dash.js refuses to fetch it: the player
    // then stalls until the window slides and it recovers (periodic stalls).
    uint64_t suggested_delay_ms = max_seg_ms * 2;
    if (suggested_delay_ms < 4000) {
        suggested_delay_ms = 4000;
    }
    uint64_t max_delay_ms = total_dur_ms > max_seg_ms ? total_dur_ms - max_seg_ms : total_dur_ms / 2;
    if (suggested_delay_ms > max_delay_ms) {
        suggested_delay_ms = max_delay_ms;
    }
    // minimumUpdatePeriod必须远小于分片周期。播放器只有在重新拉取mpd时才知道有新分片，
    // 这期间它的live edge随墙钟前进而内容不动，播放点前方缓冲 = suggestedPresentationDelay
    // - 距上次flush的时间 - mpd更新周期。按分片周期采样会与分片产出混叠，播放器周期性无片可拉而停顿
    // minimumUpdatePeriod MUST be far smaller than the segment period. The player
    // only learns about a new segment when it re-fetches the mpd, and until then
    // its live edge keeps moving while the content does not: the buffer ahead of
    // the play head is (suggestedPresentationDelay - time since the last flush -
    // mpd update period). Sampling at the segment period aliases with the segment
    // production, the player periodically finds nothing to fetch and stalls until
    // the next successful update.
    uint64_t update_ms = max_seg_ms / 3;
    if (update_ms < 1000) {
        update_ms = 1000;
    } else if (update_ms > 2000) {
        update_ms = 2000;
    }
    // minBufferTime不得超过播放点前方实际可交付的缓冲(suggestedPresentationDelay
    // - 一个分片周期 - minimumUpdatePeriod)，否则播放器会一直等一个永远填不满的目标，
    // 延迟随之超过目标值，live catch-up启动后消费快于生产，每个分片周期饿死一次
    // Never ask the player to buffer more than the stream can actually deliver
    // ahead of the play head (suggestedPresentationDelay minus one segment period
    // minus the mpd update period): a minBufferTime that cannot be reached makes
    // the player wait for buffer that never comes, the latency then grows past the
    // target and the live catch-up starts consuming faster than the segments are
    // produced, which starves the buffer on every segment.
    uint64_t ahead_ms = suggested_delay_ms > max_seg_ms + update_ms ? suggested_delay_ms - max_seg_ms - update_ms : 0;
    if (min_buffer_ms > ahead_ms) {
        min_buffer_ms = ahead_ms > 1000 ? ahead_ms : 1000;
    }
    // live edge由播放器自己的时钟推算，可能比最后一个列出分片领先最多一个分片，
    // 窗口留出余量，避免最老的分片提前"过期"
    // The live edge is computed from the player's own clock, so it can sit up to
    // one segment ahead of the last listed segment. Keep a margin on the window
    // so the oldest listed segment does not fall out of it ahead of time.
    uint64_t window_margin_ms = max_seg_ms / 2;
    if (window_margin_ms > 2000) {
        window_margin_ms = 2000;
    }

    string mpd;
    mpd.reserve(4096);
    mpd += "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n";
    mpd += "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" profiles=\"urn:mpeg:dash:profile:isoff-live:2011\"";
    if (is_live) {
        mpd += " type=\"dynamic\"";
        mpd += " availabilityStartTime=\"" + ast_iso + "\"";
        // publishTime是本mpd生成时的墙钟时间，它必须每次更新都递增，
        // 否则播放器认为清单未变，live edge被冻结
        // publishTime is the wall clock time at which this mpd was generated,
        // it MUST advance on every update, otherwise the player considers the
        // manifest unchanged and its live edge freezes
        mpd += " publishTime=\"" + now_iso + "\"";
        // 可用窗口为在窗分片总时长 + 服务器与播放器时钟偏差的余量
        // The available window is the duration of the listed segments plus a
        // margin for the skew between the server and the player clock
        mpd += " timeShiftBufferDepth=\"" + dashDuration((total_dur_ms + window_margin_ms) / 1000.0) + "\"";
        mpd += " suggestedPresentationDelay=\"" + dashDuration(suggested_delay_ms / 1000.0) + "\"";
        mpd += " minimumUpdatePeriod=\"" + dashDuration(update_ms / 1000.0) + "\"";
    } else {
        mpd += " type=\"static\"";
        mpd += " mediaPresentationDuration=\"" + dashDuration(total_dur_ms / 1000.0) + "\"";
    }
    mpd += " minBufferTime=\"" + dashDuration(min_buffer_ms / 1000.0) + "\"";
    mpd += ">\n";
    if (is_live) {
        // 把播放器时钟钉到服务器：避免依赖外部UTCTiming服务，并让live edge与publishTime对齐
        // Pin the player clock to the server: avoids external UTCTiming
        // servers and keeps the live edge aligned with publishTime
        mpd += "  <UTCTiming schemeIdUri=\"urn:mpeg:dash:utc:direct:2014\" value=\"" + now_iso + "\"/>\n";
    }
    // Period恒从0开始，滑动窗口只体现在SegmentTimeline的绝对@t上，
    // 播放器在整个直播会话中看到的是稳定的单Period时间轴
    // The period always starts at 0, the sliding window is expressed by the
    // absolute @t of the segment timeline: the player keeps a single stable
    // period for the whole live session
    mpd += "  <Period id=\"1\" start=\"PT0S\">\n";
    mpd += string("    <AdaptationSet id=\"1\" mimeType=\"") + (_have_video ? "video" : "audio")
        + "/mp4\" segmentAlignment=\"true\" startWithSAP=\"1\">\n";
    mpd += "      <Representation id=\"r1\" bandwidth=\"" + std::to_string(bandwidth) + "\"";
    if (!_dash_codecs.empty()) {
        // dash.js需要codecs属性通过MediaSource能力检查
        // dash.js needs the codecs attribute to pass its MediaSource capability check
        mpd += " codecs=\"" + dashXmlEscape(_dash_codecs) + "\"";
    }
    mpd += ">\n";
    // SegmentTimeline逐片给出真实时长: 分片按关键帧切割，时长永远不等于配置值，
    // SegmentList@duration的常量时长寻址会与媒体时间戳错位，产生缓冲空洞/重叠(播放不流畅)
    // SegmentTimeline with the real duration of every segment: segments are cut
    // on keyframes so their duration is never exactly the configured one, the
    // constant duration addressing of SegmentList@duration would drift from the
    // media timestamps and produce buffer gaps/overlaps (stuttering playback)
    mpd += "        <SegmentList timescale=\"1000\">\n";
    mpd += "          <Initialization sourceURL=\"init.mp4\"/>\n";
    mpd += "          <SegmentTimeline>\n";
    uint64_t seg_t_ms = time_offset;
    for (auto &info : temp) {
        mpd += "            <S t=\"" + std::to_string(seg_t_ms) + "\" d=\"" + std::to_string((uint64_t)info.duration_ms) + "\"/>\n";
        seg_t_ms += (uint64_t)info.duration_ms;
    }
    mpd += "          </SegmentTimeline>\n";
    for (auto &info : temp) {
        mpd += "          <SegmentURL media=\"" + dashXmlEscape(info.name) + "\"/>\n";
    }
    mpd += "        </SegmentList>\n";
    mpd += "      </Representation>\n";
    mpd += "    </AdaptationSet>\n";
    mpd += "  </Period>\n";
    mpd += "</MPD>\n";
    onWriteDash(mpd, memory_only);
}

void HlsMaker::refreshDashFile() {
    if (!_is_fmp4 || !isLive() || _seg_dur_list.empty() || _dash_last_emit_ms == 0) {
        // 尚未发布过，或非fmp4直播流
        // Nothing published yet, or not a live fmp4 stream
        return;
    }
    auto now_ms = getCurrentMillisecond(true);
    if (now_ms <= _dash_last_emit_ms || now_ms - _dash_last_emit_ms < kDashRefreshMs) {
        // 节流: 每个分片fragment都会调用到这里
        // Throttled: this is called for every segment fragment
        return;
    }
    // 内容与上次发布相同，但availabilityStartTime与publishTime是新的，
    // 播放器的live edge得以随媒体速率持续前进
    // Same content as the last publish but with a fresh availabilityStartTime
    // and publishTime, so the player's live edge keeps moving with the media
    makeDashFile(false, true);
}

void HlsMaker::inputInitSegment(const char *data, size_t len) {
    if (!_is_fmp4) {
        throw std::invalid_argument("Only fmp4-hls can input init segment");
    }
    onWriteInitSegment(data, len);
}

void HlsMaker::inputData(const char *data, size_t len, uint64_t timestamp, bool is_idr_fast_packet) {
    if (data && len) {
        if (timestamp < _last_timestamp) {
            // 时间戳回退了，切片时长重新计时  [AUTO-TRANSLATED:fe91bd7f]
            // Timestamp has been rolled back, slice duration is recalculated
            WarnL << "Timestamp reduce: " << _last_timestamp << " -> " << timestamp;
            _last_seg_timestamp = _last_timestamp = timestamp;
        }
        if (is_idr_fast_packet) {
            // 尝试切片ts  [AUTO-TRANSLATED:62264109]
            // Attempt to slice ts
            addNewSegment(timestamp);
        }
        if (!_last_file_name.empty()) {
            // 存在切片才写入ts数据  [AUTO-TRANSLATED:ddd46115]
            // Write ts data only if there are slices
            onWriteSegment(data, len);
            _last_timestamp = timestamp;
            _current_seg_bytes += len;
        }
    } else {
        // resetTracks时触发此逻辑  [AUTO-TRANSLATED:0ba915ed]
        // This logic is triggered when resetTracks is called
        flushLastSegment(false);
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
        // 保持dash媒体时间轴连续: 累加被移除切片的时长
        // Keep the dash media timeline stable: remember the duration of the removed segment
        _dash_timeline_offset_ms += (uint64_t)_seg_dur_list.front().duration_ms;
        _seg_dur_list.pop_front();
    }
    GET_CONFIG(uint32_t, segRetain, Hls::kSegmentRetain);
    // 但是实际保存的切片个数比m3u8所述多若干个,这样做的目的是防止播放器在切片删除前能下载完毕  [AUTO-TRANSLATED:1688f857]
    // However, the actual number of slices saved is a few more than what is stated in the m3u8, this is done to prevent the player from downloading the slices before they are deleted
    if (_file_index > _seg_number + segDelay + segRetain) {
        onDelSegment(_file_index - _seg_number - segDelay - segRetain - 1);
    }
}

void HlsMaker::addNewSegment(uint64_t stamp) {
    GET_CONFIG(bool, fastRegister, Hls::kFastRegister);
    if (_file_index > fastRegister  && stamp - _last_seg_timestamp < _seg_duration * 1000) {
        // 确保序号为0的切片立即open，如果开启快速注册功能，序号为1的切片也应该遇到关键帧立即生成；否则需要等切片时长够长  [AUTO-TRANSLATED:d81d1a1c]
        // Ensure that the slice with sequence number 0 is opened immediately, if the fast registration function is enabled, the slice with sequence number 1 should also be generated immediately when it encounters a keyframe; otherwise, it needs to wait until the slice duration is long enough
        return;
    }
    // 关闭并保存上一个切片，如果_seg_number==0,那么是点播。  [AUTO-TRANSLATED:14076b61]
    // Close and save the previous slice, if _seg_number==0, then it is on-demand.
    flushLastSegment(false);
    // 新增切片  [AUTO-TRANSLATED:b8623419]
    // Add a new slice
    _last_file_name = onOpenSegment(_file_index++);
    // 记录本次切片的起始时间戳  [AUTO-TRANSLATED:8eb776e9]
    // Record the starting timestamp of this slice
    _last_seg_timestamp = _last_timestamp ? _last_timestamp : stamp;
    // 同时记录起始的服务器系统时间，用于生成EXT-X-PROGRAM-DATE-TIME
    // Also record the starting wall-clock time, used to generate EXT-X-PROGRAM-DATE-TIME
    _last_seg_wall_clock = getCurrentMillisecond(true);
}

void HlsMaker::flushLastSegment(bool eof){
    GET_CONFIG(uint32_t, segDelay, Hls::kSegmentDelay);
    if (_last_file_name.empty()) {
        // 不存在上个切片  [AUTO-TRANSLATED:d81fe08e]
        // There is no previous slice
        return;
    }
    // 文件创建到最后一次数据写入的时间即为切片长度  [AUTO-TRANSLATED:1f85739c]
    // The time from file creation to the last data write is the slice length
    auto seg_dur = _last_timestamp - _last_seg_timestamp;
    if (seg_dur <= 0) {
        seg_dur = 100;
    }
    GET_CONFIG(bool, program_date_time, Hls::kProgramDateTime);
    _last_seg_date_time = program_date_time ? toProgramDateTimeStr(_last_seg_wall_clock) : string();
    _seg_dur_list.push_back(HlsSegmentInfo { (int)seg_dur, std::move(_last_file_name), _last_seg_date_time, _current_seg_bytes });
    _current_seg_bytes = 0;
    delOldSegment();
    // 先flush ts切片，否则可能存在ts文件未写入完毕就被访问的情况  [AUTO-TRANSLATED:f8d6dc87]
    // Flush the ts slice first, otherwise there may be a situation where the ts file is not written completely before it is accessed
    onFlushLastSegment(seg_dur);
    // 然后写m3u8文件  [AUTO-TRANSLATED:67200ce1]
    // Then write the m3u8 file
    makeIndexFile(false, eof);
    // 写入切片延迟的m3u8文件  [AUTO-TRANSLATED:b1f12e43]
    // Write the m3u8 file with slice delay
    if (segDelay) {
        makeIndexFile(true, eof);
    }
    // 写dash mpd文件, 与m3u8共用同一套fmp4分片
    // Write the dash mpd file, sharing the same fmp4 segments
    makeDashFile(eof);
}

const string &HlsMaker::getLastSegmentDateTime() const {
    return _last_seg_date_time;
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

void HlsMaker::clear() {
    _file_index = 0;
    _last_timestamp = 0;
    _last_seg_timestamp = 0;
    _last_seg_wall_clock = 0;
    _last_seg_date_time.clear();
    _seg_dur_list.clear();
    _last_file_name.clear();
    _dash_timeline_offset_ms = 0;
    _ast_wall_ms = 0;
    _dash_last_emit_ms = 0;
    _current_seg_bytes = 0;
}

}//namespace mediakit
