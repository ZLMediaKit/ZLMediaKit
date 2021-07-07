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
namespace mediakit {

HlsMaker::HlsMaker(float seg_duration, uint32_t seg_number) {
    //最小允许设置为0，0个切片代表点播
    _seg_number = seg_number;
    _seg_duration = seg_duration;
}

HlsMaker::~HlsMaker() {
}


void HlsMaker::makeIndexFile(bool eof) {
    char file_content[1024];
    int maxSegmentDuration = 0;

    for (auto &tp : _seg_dur_list) {
        int dur = std::get<0>(tp);
        if (dur > maxSegmentDuration) {
            maxSegmentDuration = dur;
        }
    }

    auto sequence = _seg_number ? (_file_index > _seg_number ? _file_index - _seg_number : 0LL) : 0LL;

    string m3u8;
    snprintf(file_content, sizeof(file_content),
             "#EXTM3U\n"
             "#EXT-X-VERSION:3\n"
             "#EXT-X-ALLOW-CACHE:NO\n"
             "#EXT-X-TARGETDURATION:%u\n"
             "#EXT-X-MEDIA-SEQUENCE:%llu\n",
             (maxSegmentDuration + 999) / 1000,
             sequence);

    m3u8.assign(file_content);

    for (auto &tp : _seg_dur_list) {
        snprintf(file_content, sizeof(file_content), "#EXTINF:%.3f,\n%s\n", std::get<0>(tp) / 1000.0, std::get<1>(tp).data());
        m3u8.append(file_content);
    }

    if (eof) {
        snprintf(file_content, sizeof(file_content), "#EXT-X-ENDLIST\n");
        m3u8.append(file_content);
    }
    onWriteHls(m3u8.data(), m3u8.size());
}


void HlsMaker::inputData(void *data, size_t len, uint32_t timestamp, bool is_idr_fast_packet) {
    if (data && len) {
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
        flushLastSegment(true);
    }
}

void HlsMaker::delOldSegment() {
    if (_seg_number == 0) {
        //如果设置为保留0个切片，则认为是保存为点播
        return;
    }
    //在hls m3u8索引文件中,我们保存的切片个数跟_seg_number相关设置一致
    if (_file_index > _seg_number) {
        _seg_dur_list.pop_front();
    }

    GET_CONFIG(uint32_t, segRetain, Hls::kSegmentRetain);
    //但是实际保存的切片个数比m3u8所述多若干个,这样做的目的是防止播放器在切片删除前能下载完毕
    if (_file_index > _seg_number + segRetain) {
        onDelSegment(_file_index - _seg_number - segRetain - 1);
    }
}

void HlsMaker::addNewSegment(uint32_t stamp) {
    if (!_last_file_name.empty() && stamp - _last_seg_timestamp < _seg_duration * 1000) {
        //存在上个切片，并且未到分片时间
        return;
    }

    //关闭并保存上一个切片，如果_seg_number==0,那么是点播。
    flushLastSegment(_seg_number == 0);
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
    _seg_dur_list.push_back(std::make_tuple(seg_dur, std::move(_last_file_name)));
    _last_file_name.clear();
    delOldSegment();
    makeIndexFile(eof);
    onFlushLastSegment(seg_dur);
}

bool HlsMaker::isLive() {
    return _seg_number != 0;
}

void HlsMaker::clear() {
    _file_index = 0;
    _last_seg_timestamp = 0;
    _seg_dur_list.clear();
    _last_file_name.clear();
}

}//namespace mediakit