/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HLSMAKER_H
#define HLSMAKER_H

#include <deque>
#include <tuple>
#include "Common/config.h"
#include "Util/TimeTicker.h"
#include "Util/File.h"
#include "Util/util.h"
#include "Util/logger.h"
using namespace toolkit;

namespace mediakit {

class HlsMaker {
public:
    /**
     * @param seg_duration 切片文件长度
     * @param seg_number 切片个数
     */
    HlsMaker(float seg_duration = 5, uint32_t seg_number = 3);
    virtual ~HlsMaker();

    /**
     * 写入ts数据
     * @param data 数据
     * @param len 数据长度
     * @param timestamp 毫秒时间戳
     * @param is_idr_fast_packet 是否为关键帧第一个包
     */
    void inputData(void *data, uint32_t len, uint32_t timestamp, bool is_idr_fast_packet);
protected:
    /**
     * 创建ts切片文件回调
     * @param index
     * @return
     */
    virtual string onOpenSegment(int index) = 0;

    /**
     * 删除ts切片文件回调
     * @param index
     */
    virtual void onDelSegment(int index) = 0;

    /**
     * 写ts切片文件回调
     * @param data
     * @param len
     */
    virtual void onWriteSegment(const char *data, int len) = 0;

    /**
     * 写m3u8文件回调
     * @param data
     * @param len
     */
    virtual void onWriteHls(const char *data, int len) = 0;

    /**
     * 关闭上个ts切片并且写入m3u8索引
     * @param eof
     */
    void flushLastSegment(bool eof = false);

    /**
     * 是否为直播
     */
    bool isLive();
private:
    /**
     * 生成m3u8文件
     * @param eof true代表点播
     */
    void makeIndexFile(bool eof = false);

    /**
     * 删除旧的ts切片
     */
    void delOldSegment();

    /**
     * 添加新的ts切片
     * @param timestamp
     */
    void addNewSegment(uint32_t timestamp);
private:
    uint32_t _seg_number = 0;
    float _seg_duration = 0;
    uint64_t _file_index = 0;
    Ticker _ticker;
    Ticker _ticker_last_data;
    string _last_file_name;
    std::deque<tuple<int,string> > _seg_dur_list;
};

}//namespace mediakit
#endif //HLSMAKER_H
