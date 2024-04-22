/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iomanip>
#include "HlsMaker.h"
#include "Common/config.h"

using namespace std;

namespace mediakit {

HlsMaker::HlsMaker(bool is_fmp4, float seg_duration, uint32_t seg_number, bool seg_keep) {
	_is_fmp4 = is_fmp4;
    //最小允许设置为0，0个切片代表点播
    _seg_number = seg_number;
    _seg_duration = seg_duration;
    _seg_keep = seg_keep;
}

void HlsMaker::makeIndexFile(bool include_delay, bool eof) {
    GET_CONFIG(uint32_t, segDelay, Hls::kSegmentDelay);
    GET_CONFIG(uint32_t, segRetain, Hls::kSegmentRetain);
    std::deque<std::tuple<int, std::string>> temp(_seg_dur_list);
    if (!include_delay && _seg_number) {
        while (temp.size() > _seg_number) {
            temp.pop_front();
        }
    }
    int maxSegmentDuration = 0;
    for (auto &tp : temp) {
        int dur = std::get<0>(tp);
        if (dur > maxSegmentDuration) {
            maxSegmentDuration = dur;
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
    for (auto &tp : temp) {
        ss << "#EXTINF:" << std::setprecision(3) << std::get<0>(tp) / 1000.0 << ",\n" << std::get<1>(tp) << "\n";
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
    onWriteInitSegment(data, len);
}

void HlsMaker::inputData(const char *data, size_t len, uint64_t timestamp, bool is_idr_fast_packet) {
    if (data && len) {
        if (timestamp < _last_timestamp) {
            // 时间戳回退了，切片时长重新计时
            WarnL << "Timestamp reduce: " << _last_timestamp << " -> " << timestamp;
            _last_seg_timestamp = _last_timestamp = timestamp;
        }
        if (is_idr_fast_packet) {
            // 尝试切片ts
            addNewSegment(timestamp);
        }
        if (!_last_file_name.empty()) {
            // 存在切片才写入ts数据
            onWriteSegment(data, len);
            _last_timestamp = timestamp;
        }
    } else {
        // resetTracks时触发此逻辑
        flushLastSegment(false);
    }
}

void HlsMaker::delOldSegment() {
    GET_CONFIG(uint32_t, segDelay, Hls::kSegmentDelay);
    if (_seg_number == 0) {
        //如果设置为保留0个切片，则认为是保存为点播
        return;
    }
    //在hls m3u8索引文件中,我们保存的切片个数跟_seg_number相关设置一致
    if (_file_index > _seg_number + segDelay) {
        _seg_dur_list.pop_front();
    }
    //如果设置为一直保存，就不删除
    if (_seg_keep) {
        return;
    }
    GET_CONFIG(uint32_t, segRetain, Hls::kSegmentRetain);
    //但是实际保存的切片个数比m3u8所述多若干个,这样做的目的是防止播放器在切片删除前能下载完毕
    if (_file_index > _seg_number + segDelay + segRetain) {
        onDelSegment(_file_index - _seg_number - segDelay - segRetain - 1);
    }
}

void HlsMaker::addNewSegment(uint64_t stamp) {
    GET_CONFIG(bool, fastRegister, Hls::kFastRegister);
    if (_file_index > fastRegister  && stamp - _last_seg_timestamp < _seg_duration * 1000) {
        //确保序号为0的切片立即open，如果开启快速注册功能，序号为1的切片也应该遇到关键帧立即生成；否则需要等切片时长够长
        return;
    }
    //关闭并保存上一个切片，如果_seg_number==0,那么是点播。
    flushLastSegment(false);
    //新增切片
    _last_file_name = onOpenSegment(_file_index++);
    //记录本次切片的起始时间戳
    _last_seg_timestamp = _last_timestamp ? _last_timestamp : stamp;
}

void HlsMaker::flushLastSegment(bool eof){
    GET_CONFIG(uint32_t, segDelay, Hls::kSegmentDelay);
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
    //先flush ts切片，否则可能存在ts文件未写入完毕就被访问的情况
    onFlushLastSegment(seg_dur);
    //然后写m3u8文件
    makeIndexFile(false, eof);
    //写入切片延迟的m3u8文件
    if (segDelay) {
        makeIndexFile(true, eof);
    }
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
    _seg_dur_list.clear();
    _last_file_name.clear();
}

}//namespace mediakit
