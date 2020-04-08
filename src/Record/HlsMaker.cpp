/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HlsMaker.h"
namespace mediakit {

HlsMaker::HlsMaker(float seg_duration, uint32_t seg_number) {
    //最小允许设置为0，0个切片代表点播
    seg_number = MAX(0,seg_number);
    seg_duration = MAX(1,seg_duration);
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

    string m3u8;
    snprintf(file_content,sizeof(file_content),
          "#EXTM3U\n"
          "#EXT-X-VERSION:3\n"
          "#EXT-X-ALLOW-CACHE:NO\n"
          "#EXT-X-TARGETDURATION:%u\n"
          "#EXT-X-MEDIA-SEQUENCE:%llu\n",
          (maxSegmentDuration + 999) / 1000,
          _seg_number ? _file_index : 0);

    m3u8.assign(file_content);

    for (auto &tp : _seg_dur_list) {
        snprintf(file_content,sizeof(file_content), "#EXTINF:%.3f,\n%s\n", std::get<0>(tp) / 1000.0, std::get<1>(tp).data());
        m3u8.append(file_content);
    }

    if (eof) {
        snprintf(file_content,sizeof(file_content),"#EXT-X-ENDLIST\n");
        m3u8.append(file_content);
    }
    onWriteHls(m3u8.data(), m3u8.size());
}


void HlsMaker::inputData(void *data, uint32_t len, uint32_t timestamp, bool is_idr_fast_packet) {
    if (data && len) {
        if(is_idr_fast_packet){
            addNewSegment(timestamp);
        }
        onWriteSegment((char *) data, len);
        //记录上次写入数据时间
        _ticker_last_data.resetTime();
    } else {
        //resetTracks时触发此逻辑
        flushLastSegment(true);
    }
}

void HlsMaker::delOldSegment() {
    if(_seg_number == 0){
        //如果设置为保留0个切片，则认为是保存为点播
        return;
    }
    //在hls m3u8索引文件中,我们保存的切片个数跟_seg_number相关设置一致
    if (_file_index > _seg_number) {
        _seg_dur_list.pop_front();
    }

    GET_CONFIG(uint32_t,segRetain,Hls::kSegmentRetain);
    //但是实际保存的切片个数比m3u8所述多若干个,这样做的目的是防止播放器在切片删除前能下载完毕
    if (_file_index > _seg_number + segRetain) {
        onDelSegment(_file_index - _seg_number - segRetain - 1);
    }
}

void HlsMaker::addNewSegment(uint32_t) {
    if(!_last_file_name.empty() && _ticker.elapsedTime() < _seg_duration * 1000){
        //存在上个切片，并且未到分片时间
        return;
    }

    //关闭并保存上一个切片，如果_seg_number==0,那么是点播。
    flushLastSegment(_seg_number == 0);
    //新增切片
    _last_file_name = onOpenSegment(_file_index++);
    //重置切片计时器
    _ticker.resetTime();
}

void HlsMaker::flushLastSegment(bool eof){
    if(_last_file_name.empty()){
        //不存在上个切片
        return;
    }
    //文件创建到最后一次数据写入的时间即为切片长度
    auto seg_dur = _ticker.elapsedTime() - _ticker_last_data.elapsedTime();
    if(seg_dur <= 0){
        seg_dur = 100;
    }
    _seg_dur_list.push_back(std::make_tuple(seg_dur, _last_file_name));
    delOldSegment();
    makeIndexFile(eof);
    _last_file_name.clear();
}

bool HlsMaker::isLive() {
    return _seg_number != 0;
}

}//namespace mediakit