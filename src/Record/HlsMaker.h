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

namespace mediakit {

// m3u8索引中单个切片的元信息
// Metadata of a single segment in the m3u8 index
struct HlsSegmentInfo {
    // 切片时长，单位毫秒
    // Segment duration in milliseconds
    int duration_ms;
    // 切片文件名(相对m3u8的路径)
    // Segment file name (path relative to the m3u8)
    std::string name;
    // 切片首帧对应的服务器系统时间，已格式化为EXT-X-PROGRAM-DATE-TIME的取值
    // 未开启该标签时为空串。索引文件会被反复生成，故在切片入列时只格式化一次
    // The segment's first-sample wall-clock time, pre-formatted as the EXT-X-PROGRAM-DATE-TIME value.
    // Empty when the tag is disabled. The index is rebuilt repeatedly, so it is formatted once on insert
    std::string program_date_time;
};

class HlsMaker {
public:
    /**
     * @param is_fmp4 使用fmp4还是mpegts
     * @param seg_duration 切片文件长度
     * @param seg_number 切片个数
     * @param seg_keep 是否保留切片文件
     * @param is_fmp4 Use fmp4 or mpegts
     * @param seg_duration Segment file length
     * @param seg_number Number of segments
     * @param seg_keep Whether to keep the segment file
     
     * [AUTO-TRANSLATED:260bbca3]
     */
    HlsMaker(bool is_fmp4 = false, float seg_duration = 5, uint32_t seg_number = 3, bool seg_keep = false);
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
     * Input fmp4 init segment
     * @param data Data
     * @param len Data length
     
     * [AUTO-TRANSLATED:8d613a42]
     */
    void inputInitSegment(const char *data, size_t len);

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
     * 写init.mp4切片文件回调
     * @param data
     * @param len
     * Write init.mp4 segment file callback
     * @param data
     * @param len
     
     * [AUTO-TRANSLATED:e0021ec5]
     */
    virtual void onWriteInitSegment(const char *data, size_t len) = 0;

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
     * 上一个 ts 切片写入完成, 可在这里进行通知处理
     * @param duration_ms 上一个 ts 切片的时长, 单位为毫秒
     * The previous ts segment is written, you can notify here
     * @param duration_ms The duration of the previous ts segment, in milliseconds
     
     * [AUTO-TRANSLATED:36b42bc0]
     */
    virtual void onFlushLastSegment(uint64_t duration_ms) {};

    /**
     * 关闭上个ts切片并且写入m3u8索引
     * @param eof HLS直播是否已结束
     * Close the previous ts segment and write the m3u8 index
     * @param eof Whether the HLS live broadcast has ended
     
     * [AUTO-TRANSLATED:614b7e14]
     */
    void flushLastSegment(bool eof);

    /**
     * 获取刚结束切片已格式化的EXT-X-PROGRAM-DATE-TIME取值，未开启该标签时为空串
     * 仅在onFlushLastSegment()回调期间有效
     * Get the pre-formatted EXT-X-PROGRAM-DATE-TIME value of the segment that just ended;
     * empty when the tag is disabled. Only valid during the onFlushLastSegment() callback
     */
    const std::string &getLastSegmentDateTime() const;

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
     * 添加新的ts切片
     * @param timestamp
     * Add new ts segments
     * @param timestamp
     
     * [AUTO-TRANSLATED:e321e9f0]
     */
    void addNewSegment(uint64_t timestamp);

private:
    bool _is_fmp4 = false;
    float _seg_duration = 0;
    uint32_t _seg_number = 0;
    bool _seg_keep = false;
    uint64_t _last_timestamp = 0;
    uint64_t _last_seg_timestamp = 0;
    // 当前切片首帧对应的服务器系统时间，单位毫秒
    // Wall-clock time of the current segment's first sample, in milliseconds
    uint64_t _last_seg_wall_clock = 0;
    // 刚结束切片已格式化的EXT-X-PROGRAM-DATE-TIME取值，供onFlushLastSegment()期间取用
    // Pre-formatted EXT-X-PROGRAM-DATE-TIME value of the segment that just ended,
    // consumed during the onFlushLastSegment() callback
    std::string _last_seg_date_time;
    uint64_t _file_index = 0;
    std::string _last_file_name;
    std::deque<HlsSegmentInfo> _seg_dur_list;
};

}//namespace mediakit
#endif //HLSMAKER_H
