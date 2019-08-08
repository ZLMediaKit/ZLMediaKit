/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "HlsMaker.h"
namespace mediakit {

HlsMaker::HlsMaker(float seg_duration, uint32_t seg_number) {
    seg_number = MAX(1,seg_number);
    seg_duration = MAX(1,seg_duration);
    _seg_number = seg_number;
    _seg_duration = seg_duration;
}

HlsMaker::~HlsMaker() {
}

#define PRINT(...)  file_size += snprintf(file_content + file_size,sizeof(file_content) - file_size, ##__VA_ARGS__)

void HlsMaker::makeIndexFile(bool eof) {
    char file_content[4 * 1024];
    int file_size = 0;

    int maxSegmentDuration = 0;
    for (auto &tp : _seg_dur_list) {
        int dur = std::get<0>(tp);
        if (dur > maxSegmentDuration) {
            maxSegmentDuration = dur;
        }
    }
    PRINT("#EXTM3U\n"
          "#EXT-X-VERSION:3\n"
          "#EXT-X-ALLOW-CACHE:NO\n"
          "#EXT-X-TARGETDURATION:%u\n"
          "#EXT-X-MEDIA-SEQUENCE:%llu\n",
          (maxSegmentDuration + 999) / 1000,
          _file_index);

    for (auto &tp : _seg_dur_list) {
        PRINT("#EXTINF:%.3f,\n%s\n", std::get<0>(tp) / 1000.0, std::get<1>(tp).data());
    }

    if (eof) {
        PRINT("#EXT-X-ENDLIST\n");
    }
    onWriteHls(file_content, file_size);
}


void HlsMaker::inputData(void *data, uint32_t len, uint32_t timestamp) {
    addNewFile(timestamp);
    onWriteFile((char *) data, len);
}

void HlsMaker::delOldFile() {
    //在hls m3u8索引文件中,我们保存的切片个数跟_seg_number相关设置一致
    if (_file_index >= _seg_number + 2) {
        _seg_dur_list.pop_front();
    }

    //但是实际保存的切片个数比m3u8所述多两个,这样做的目的是防止播放器在切片删除前能下载完毕
    if (_file_index >= _seg_number + 4) {
        onDelFile(_file_index - _seg_number - 4);
    }
}

void HlsMaker::addNewFile(uint32_t) {
    int stampInc = _ticker.elapsedTime();
    if (stampInc >= _seg_duration * 1000) {
        _ticker.resetTime();
        auto file_name = onOpenFile(_file_index);
        if (_file_index++ > 0) {
            _seg_dur_list.push_back(std::make_tuple(stampInc, _last_file_name));
            delOldFile();
            makeIndexFile();
        }
        _last_file_name = file_name;
    }
}

}//namespace mediakit