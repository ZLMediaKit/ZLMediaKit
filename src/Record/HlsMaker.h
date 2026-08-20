/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HLSMAKER_H
#define HLSMAKER_H

#include <string>
#include <deque>
#include <cstdint>
#include <utility>

namespace mediakit {

/**
 * HLS playlist 与逻辑切片状态机。
 * fMP4 init URI 使用 init-<file-name-id>-<revision>.mp4；生产文件实现生成的 TS/fMP4 媒体 URI
 * 还包含文件命名号和 recorder 内媒体命名序号。这些唯一名称不是稳定 API，也不生成旧名兼容别名；
 * 消费者必须以 playlist 发布的 URI 为准。
 * HLS playlist and logical-segment state machine.
 * fMP4 init URIs use init-<file-name-id>-<revision>.mp4. Production TS/fMP4 media URIs also contain
 * the file-name ID and recorder-local media index. These unique names are not a stable API and have no
 * legacy aliases; consumers must follow the URIs published by the playlist.
 */
class HlsMaker {
public:
    /**
     * @param is_fmp4 使用fmp4还是mpegts
     * @param seg_duration 切片文件长度
     * @param seg_number 切片个数
     * @param seg_keep 是否保留切片文件
     * @param seg_max_duration 异常流强制开片或切片的绝对最大时长，0表示关闭
     * @param is_fmp4 Use fmp4 or mpegts
     * @param seg_duration Segment file length
     * @param seg_number Number of segments
     * @param seg_keep Whether to keep the segment file
     * @param seg_max_duration Absolute hard limit for force-opening or rotating abnormal streams; 0 disables it
     
     * [AUTO-TRANSLATED:260bbca3]
     */
    HlsMaker(bool is_fmp4 = false, float seg_duration = 5, uint32_t seg_number = 3, bool seg_keep = false,
             float seg_max_duration = 0);
    virtual ~HlsMaker() = default;

    /**
     * 写入ts数据
     * @param data 数据
     * @param len 数据长度
     * @param timestamp 毫秒时间戳
     * @param is_idr_fast_packet 是否为关键帧第一个包
     * Write ts data
     * @param data Data
     * @param len Data length
     * @param timestamp Millisecond timestamp
     * @param is_idr_fast_packet Whether it is the first packet of the key frame
     
     * [AUTO-TRANSLATED:b886bbbf]
     */
    void inputData(const char *data, size_t len, uint64_t timestamp, bool is_idr_fast_packet);

    /**
     * 输入fmp4 init segment
     * @param data 数据
     * @param len 数据长度
     * @return 是否成功提交新的 init URI
     * Input fmp4 init segment
     * @param data Data
     * @param len Data length
     * @return Whether a new init URI was committed successfully
     
     * [AUTO-TRANSLATED:8d613a42]
     */
    bool inputInitSegment(const char *data, size_t len);

    /**
     * 是否为直播
     * Whether it is live
     
     * [AUTO-TRANSLATED:1dae0496]
     */
    bool isLive() const;

    /**
     * 是否保留切片文件
     * Whether to keep the segment file
     
     * [AUTO-TRANSLATED:c2d1bce5]
     */
    bool isKeep() const;

    /**
     * 是否采用fmp4切片还是mpegts
     * Whether to use fmp4 segmentation or mpegts
     
     * [AUTO-TRANSLATED:36763fc8]
     */
    bool isFmp4() const;

    /**
     * 清空记录
     * Clear records
     
     * [AUTO-TRANSLATED:34a4b6cd]
     */
    void clear();

protected:
    /**
     * 创建ts切片文件回调
     * @param index
     * @return
     * Create ts segment file callback
     * @param index
     * @return
     
     * [AUTO-TRANSLATED:2a3806fc]
     */
    virtual std::string onOpenSegment(uint64_t index) = 0;

    /**
     * 删除ts切片文件回调
     * @param index
     * Delete ts segment file callback
     * @param index
     
     * [AUTO-TRANSLATED:1c0d4397]
     */
    virtual void onDelSegment(uint64_t index) = 0;

    /**
     * 写候选 init 切片文件回调
     * @param candidate_uri 候选 init URI
     * @param data
     * @param len
     * @return 是否完整写入并成功关闭
     * Write candidate init segment file callback
     * @param candidate_uri Candidate init URI
     * @param data
     * @param len
     * @return Whether the candidate was fully written and successfully closed
     
     * [AUTO-TRANSLATED:e0021ec5]
     */
    virtual bool onWriteInitSegment(const std::string &candidate_uri, const char *data, size_t len) = 0;

    /**
     * 写ts切片文件回调
     * @param data
     * @param len
     * Write ts segment file callback
     * @param data
     * @param len
     
     * [AUTO-TRANSLATED:bb81e206]
     */
    virtual void onWriteSegment(const char *data, size_t len) = 0;

    /**
     * 写m3u8文件回调
     * Write m3u8 file callback
     
     * [AUTO-TRANSLATED:5754525f]
     */
    virtual void onWriteHls(const std::string &data, bool include_delay) = 0;

    /**
     * 上一个切片写入完成，并提供生成归档索引所需的时间线元数据。
     * @param duration_ms 切片时长，单位毫秒
     * @param discontinuity 该切片前是否存在时间线中断
     * @param init_segment fMP4 初始化段 URI；TS 切片为空
     * The previous segment is complete, with timeline metadata required by archive indexes.
     * @param duration_ms Segment duration in milliseconds
     * @param discontinuity Whether a timeline discontinuity precedes this segment
     * @param init_segment fMP4 initialization segment URI; empty for TS
     */
    virtual void onFlushLastSegment(uint64_t duration_ms, bool discontinuity, const std::string &init_segment) = 0;

    /**
     * 获取当前 fMP4 初始化段 URI。
     * Get the current fMP4 initialization segment URI.
     */
    const std::string &getCurrentInitSegment() const;

    /**
     * 获取当前 maker 实例的唯一文件命名编号。
     * Get the unique file-name identifier of this maker instance.
     */
    uint64_t getFileNameId() const;

    /**
     * 关闭上个ts切片并且写入m3u8索引
     * @param eof HLS直播是否已结束
     * Close the previous ts segment and write the m3u8 index
     * @param eof Whether the HLS live broadcast has ended
     
     * [AUTO-TRANSLATED:614b7e14]
     */
    void flushLastSegment(bool eof);

private:
    /**
     * 生成m3u8文件
     * @param eof true代表点播
     * Generate m3u8 file
     * @param eof true represents on-demand
     
     * [AUTO-TRANSLATED:d6c74fb6]
     */
    void makeIndexFile(bool include_delay, bool eof = false);

    /**
     * 删除旧的ts切片
     * Delete old ts segments
     
     * [AUTO-TRANSLATED:5da8bd70]
     */
    void delOldSegment();

    /**
     * 打开新的切片
     * @param timestamp 切片起始时间戳
     * @param fast_register_pending 是否允许首片在下一个关键帧快速结束
     * Open a new segment
     * @param timestamp Segment start timestamp
     * @param fast_register_pending Whether the first segment may end early at the next key frame
     */
    bool openSegment(uint64_t timestamp, bool fast_register_pending);

    /**
     * 重置当前切片状态，但保留历史切片列表和文件序号
     * Reset the current segment state while preserving segment history and file sequence
     */
    void resetSegmentState();

    /**
     * 判断当前是否存在已打开的切片
     * Check whether a segment is currently open
     */
    bool segmentOpened() const;

    /**
     * 判断等待阶段是否应该开启首片
     * Decide whether to open the first segment while waiting
     */
    bool shouldOpenFirstSegment(uint64_t timestamp, bool key_packet);

private:
    struct SegmentInfo {
        SegmentInfo(uint64_t duration, std::string file, std::string init, bool discontinuous)
            : duration_ms(duration)
            , file_name(std::move(file))
            , init_segment(std::move(init))
            , discontinuity(discontinuous) {}

        uint64_t duration_ms;
        std::string file_name;
        std::string init_segment;
        bool discontinuity;
    };

    // 时间戳 0 是合法值，使用最大值表示“未设置”。
    // Timestamp 0 is valid, so use the maximum value as the unset sentinel.
    static constexpr uint64_t kInvalidStamp = (uint64_t)-1;

    bool _is_fmp4 = false;
    uint64_t _seg_duration_ms = 0;
    uint64_t _seg_max_duration_ms = 0;
    uint32_t _seg_number = 0;
    bool _seg_keep = false;
    uint64_t _last_timestamp = 0;
    uint64_t _segment_start_stamp = kInvalidStamp;
    uint64_t _pending_first_stamp = kInvalidStamp;
    uint64_t _file_index = 0;
    uint64_t _file_name_id = 0;
    // 已从内部历史列表移除的 discontinuity 数量。
    // Number of discontinuities removed from the internal segment history.
    uint64_t _discontinuity_sequence = 0;
    std::string _last_file_name;
    // 每个已完成切片都固化自己的时长、文件名、初始化段和时间线状态。
    // Each completed segment owns its duration, file name, initialization segment, and timeline state.
    std::deque<SegmentInfo> _seg_dur_list;
    // 当前 fMP4 track generation 对应的初始化段 URI；clear() 后仍然有效。
    // Initialization segment URI for the current fMP4 track generation; preserved by clear().
    std::string _current_init_segment;
    uint64_t _init_segment_index = 0;
    bool _init_segment_available = false;
    bool _fatal_init_error = false;
    // 当前已打开或下一次成功打开的切片是否属于新时间线。
    // Whether the current open segment, or the next successfully opened one, starts a new timeline.
    bool _discontinuity = false;
    // 首片由关键帧开启时保持为 true，直到该片因任意原因结束。
    // Remains true when the first segment starts at a key frame, until that segment ends for any reason.
    bool _fast_register_pending = false;
};

}//namespace mediakit
#endif //HLSMAKER_H
