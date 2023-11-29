﻿/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HLSMAKER_H
#define HLSMAKER_H

#include <string>
#include <deque>
#include <tuple>
#include <cstdint>

namespace mediakit {

class HlsMaker {
public:
    /**
     * @param is_fmp4 使用fmp4还是mpegts
     * @param seg_duration 切片文件长度
     * @param seg_number 切片个数
     * @param seg_keep 是否保留切片文件
     */
    HlsMaker(bool is_fmp4 = false, float seg_duration = 5, uint32_t seg_number = 3, bool seg_keep = false);
    virtual ~HlsMaker() = default;

    /**
     * 写入ts数据
     * @param data 数据
     * @param len 数据长度
     * @param timestamp 毫秒时间戳
     * @param is_idr_fast_packet 是否为关键帧第一个包
     */
    void inputData(const char *data, size_t len, uint64_t timestamp, bool is_idr_fast_packet);

    /**
     * 输入fmp4 init segment
     * @param data 数据
     * @param len 数据长度
     */
    void inputInitSegment(const char *data, size_t len);

    /**
     * 是否为直播
     */
    bool isLive() const;

    /**
     * 是否保留切片文件
     */
    bool isKeep() const;

    /**
     * 是否采用fmp4切片还是mpegts
     */
    bool isFmp4() const;

    /**
     * 清空记录
     */
    void clear();

    /**
     * 重载m3u8文件
     */
    void restoreM3u8(const std::string &text);

    /**
     * 重载m3u8文件
     */
    void restoreM3u82(std::vector<std::string>);

protected:
    /**
     * 创建ts切片文件回调
     * @param index
     * @return
     */
    virtual std::string onOpenSegment(uint64_t index) = 0;

    /**
     * 删除ts切片文件回调
     * @param index
     */
    virtual void onDelSegment(uint64_t index) = 0;

    /**
     * 写init.mp4切片文件回调
     * @param data
     * @param len
     */
    virtual void onWriteInitSegment(const char *data, size_t len) = 0;

    /**
     * 写ts切片文件回调
     * @param data
     * @param len
     */
    virtual void onWriteSegment(const char *data, size_t len) = 0;

    /**
     * 写m3u8文件回调
     */
    virtual void onWriteHls(const std::string &data) = 0;

    /**
     * 写m3u8文件回调(按时间)
     */
    virtual void onWriteHlsTime(const std::string &data) = 0;

    /**
     * 上一个 ts 切片写入完成, 可在这里进行通知处理
     * @param duration_ms 上一个 ts 切片的时长, 单位为毫秒
     */
    virtual void onFlushLastSegment(uint64_t duration_ms) {};

    /**
     * 关闭上个ts切片并且写入m3u8索引
     * @param eof HLS直播是否已结束
     */
    void flushLastSegment(bool eof);

private:
    /**
     * 生成m3u8文件
     * @param eof true代表点播
     */
    void makeIndexFile(bool eof = false);

    /**
     * 生成m3u8文件(按时间)
     * @param eof true代表点播
     */
    void makeIndexFileTime(bool eof = false);

    /**
     * 删除旧的ts切片
     */
    void delOldSegment();

    /**
     * 添加新的ts切片
     * @param timestamp
     */
    void addNewSegment(uint64_t timestamp);

private:
    bool _is_fmp4 = false;
    float _seg_duration = 0;
    uint32_t _seg_number = 0;
    bool _seg_keep = false;
    uint64_t _last_timestamp = 0;
    uint64_t _last_seg_timestamp = 0;
    uint64_t _file_index = 0;
    std::string _last_file_name;
    std::string _last_m3u8_time;
    std::deque<std::tuple<int,std::string> > _seg_dur_list;
    std::deque<std::tuple<int,std::string> > _seg_dur_list_time;
};

}//namespace mediakit
#endif //HLSMAKER_H
