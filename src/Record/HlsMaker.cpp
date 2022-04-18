/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HlsMaker.h"
#include "Util/logger.h"
namespace mediakit {

HlsMaker::HlsMaker(float seg_duration, uint32_t seg_number, bool seg_keep) {
    // 最小允许设置为0，0个切片代表点播
    _seg_number = seg_number;
    _seg_duration = seg_duration;
    _seg_keep = seg_keep;
}

void HlsMaker::makeIndexFile(bool eof) {
    int maxSegmentDuration = 0;
    for (auto &tp : _seg_dur_list) {
        int dur = std::get<0>(tp);
        if (dur > maxSegmentDuration) {
            maxSegmentDuration = dur;
        }
    }

    char file_content[1024];
    auto sequence = _seg_number ? (_file_index > _seg_number ? _file_index - _seg_number : 0LL) : 0LL;
    if (_seg_number == 0) {
        // 录像点播支持时移
        snprintf(file_content, sizeof(file_content),
                 "#EXTM3U\n"
                 "#EXT-X-PLAYLIST-TYPE:EVENT\n"
                 "#EXT-X-VERSION:4\n"
                 "#EXT-X-TARGETDURATION:%u\n"
                 "#EXT-X-MEDIA-SEQUENCE:%llu\n",
                 (maxSegmentDuration + 999) / 1000,
                 sequence);
    } else {
        snprintf(file_content, sizeof(file_content),
                 "#EXTM3U\n"
                 "#EXT-X-VERSION:3\n"
                 "#EXT-X-ALLOW-CACHE:NO\n"
                 "#EXT-X-TARGETDURATION:%u\n"
                 "#EXT-X-MEDIA-SEQUENCE:%llu\n",
                 (maxSegmentDuration + 999) / 1000,
                 sequence);
    }
    
    std::string m3u8;
    m3u8.assign(file_content);

    for (auto &tp : _seg_dur_list) {
        snprintf(file_content, sizeof(file_content), "#EXTINF:%.3f,\n%s\n", std::get<0>(tp) / 1000.0, std::get<1>(tp).data());
        m3u8.append(file_content);
    }

    if (eof) {
        snprintf(file_content, sizeof(file_content), "#EXT-X-ENDLIST\n");
        m3u8.append(file_content);
    }
    onWriteHls(m3u8);
}


void HlsMaker::inputData(void *data, size_t len, uint32_t timestamp, bool is_idr_fast_packet) {
    if (data && len) {
        if (timestamp < _last_timestamp) {
            //时间戳回退了，切片时长重新计时
            WarnL << "stamp reduce: " << _last_timestamp << " -> " << timestamp;
            _last_seg_timestamp = _last_timestamp = timestamp;
        }
        if (is_idr_fast_packet) {
            //尝试切片ts
            addNewSegment(timestamp);
        }
        if (!_last_file_name.empty()) {
            //存在切片才写入ts数据
            onWriteSegment((char *) data, len);
            _last_timestamp = timestamp;
        }
    } else {
        //resetTracks时触发此逻辑
        flushLastSegment(false);
    }
}

void HlsMaker::delOldSegment() {
    if (_seg_number == 0) {
        // 点播模式不删除Segment
        return;
    }
    // 实时模式, 保证切片个数 <= _seg_number
    if (_file_index > _seg_number) {
        _seg_dur_list.pop_front();
    }
    //如果设置为一直保存，就不删除
    if (_seg_keep) {
        return;
    }
    GET_CONFIG(uint32_t, segRetain, Hls::kSegmentRetain);
    // 延迟删除切片，避免播放器下载不到刚被删除的切片
    if (_file_index > _seg_number + segRetain) {
        onDelSegment(_file_index - _seg_number - segRetain - 1);
    }
}

void HlsMaker::addNewSegment(uint32_t stamp) {
    if (!_last_file_name.empty() && stamp - _last_seg_timestamp < _seg_duration * 1000) {
        //存在上个切片，并且未到分片时间
        return;
    }

    //关闭并保存上一个切片
    flushLastSegment(false);
    //新增切片
    _last_file_name = onOpenSegment(_file_index++);
    //记录本次切片的起始时间戳
    _last_seg_timestamp = _last_timestamp ? _last_timestamp : stamp;
}

void HlsMaker::flushLastSegment(bool eof){
    if (_last_file_name.empty()) {
        //不存在上个切片
        return;
    }
    //文件创建到最后一次数据写入的时间即为切片长度
    auto seg_dur = _last_timestamp - _last_seg_timestamp;
    if (seg_dur <= 0) {
        seg_dur = 100;
    }
    _seg_dur_list.emplace_back(seg_dur, std::move(_last_file_name));

    delOldSegment();
    //先flush ts切片，否则可能存在ts文件未写完，就被访问的情况
    onFlushLastSegment(seg_dur);
    //后写m3u8文件
    makeIndexFile(eof);
}

bool HlsMaker::isKeep() {
    return _seg_keep;
}

void HlsMaker::clear() {
    _file_index = 0;
    _last_timestamp = 0;
    _last_seg_timestamp = 0;
    _seg_dur_list.clear();
    _last_file_name.clear();
}

}//namespace mediakit
