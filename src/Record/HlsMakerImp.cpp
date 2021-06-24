/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <ctime>
#include <sys/stat.h>
#include "HlsMakerImp.h"
#include "Util/util.h"
#include "Util/uv_errno.h"

using namespace toolkit;

namespace mediakit {

HlsMakerImp::HlsMakerImp(const string &m3u8_file,
                         const string &params,
                         uint32_t bufSize,
                         float seg_duration,
                         uint32_t seg_number) : HlsMaker(seg_duration, seg_number) {
    _poller = EventPollerPool::Instance().getPoller();
    _path_prefix = m3u8_file.substr(0, m3u8_file.rfind('/'));
    _path_hls = m3u8_file;
    _params = params;
    _buf_size = bufSize;
    _file_buf.reset(new char[bufSize], [](char *ptr) {
        delete[] ptr;
    });

    _info.folder = _path_prefix;
}

HlsMakerImp::~HlsMakerImp() {
    clearCache(false);
}

void HlsMakerImp::clearCache(bool immediately) {
    //录制完了
    flushLastSegment(true);
    if (!isLive()) {
        return;
    }

    clear();
    _file = nullptr;
    _segment_file_paths.clear();

    //hls直播才删除文件
    GET_CONFIG(uint32_t, delay, Hls::kDeleteDelaySec);
    if (!delay || immediately) {
        File::delete_file(_path_prefix.data());
    } else {
        auto path_prefix = _path_prefix;
        _poller->doDelayTask(delay * 1000, [path_prefix]() {
            File::delete_file(path_prefix.data());
            return 0;
        });
    }
}

string HlsMakerImp::onOpenSegment(uint64_t index) {
    string segment_name, segment_path;
    {
        auto strDate = getTimeStr("%Y-%m-%d");
        auto strHour = getTimeStr("%H");
        auto strTime = getTimeStr("%M-%S");
        segment_name = StrPrinter << strDate + "/" + strHour + "/" + strTime << "_" << index << ".ts";
        segment_path = _path_prefix + "/" + segment_name;
        if (isLive()) {
            _segment_file_paths.emplace(index, segment_path);
        }
    }
    _file = makeFile(segment_path, true);

    //保存本切片的元数据
    _info.start_time = ::time(NULL);
    _info.file_name = segment_name;
    _info.file_path = segment_path;
    _info.url = _info.app + "/" + _info.stream + "/" + segment_name;

    if (!_file) {
        WarnL << "create file failed," << segment_path << " " << get_uv_errmsg();
    }
    if (_params.empty()) {
        return segment_name;
    }
    return segment_name + "?" + _params;
}

void HlsMakerImp::onDelSegment(uint64_t index) {
    auto it = _segment_file_paths.find(index);
    if (it == _segment_file_paths.end()) {
        return;
    }
    File::delete_file(it->second.data());
    _segment_file_paths.erase(it);
}

void HlsMakerImp::onWriteSegment(const char *data, size_t len) {
    if (_file) {
        fwrite(data, len, 1, _file.get());
    }
    if (_media_src) {
        _media_src->onSegmentSize(len);
    }
}

void HlsMakerImp::onWriteHls(const char *data, size_t len) {
    auto hls = makeFile(_path_hls);
    if (hls) {
        fwrite(data, len, 1, hls.get());
        hls.reset();
        if (_media_src) {
            _media_src->registHls(true);
        }
    } else {
        WarnL << "create hls file failed," << _path_hls << " " << get_uv_errmsg();
    }
    //DebugL << "\r\n"  << string(data,len);
}

void HlsMakerImp::onFlushLastSegment(uint32_t duration_ms) {
    GET_CONFIG(bool, broadcastRecordTs, Hls::kBroadcastRecordTs);
    if (broadcastRecordTs) {
        //关闭ts文件以便获取正确的文件大小
        _file = nullptr;
        _info.time_len = duration_ms / 1000.0f;
        struct stat fileData;
        stat(_info.file_path.data(), &fileData);
        _info.file_size = fileData.st_size;
        NoticeCenter::Instance().emitEvent(Broadcast::kBroadcastRecordTs, _info);
    }
}

std::shared_ptr<FILE> HlsMakerImp::makeFile(const string &file, bool setbuf) {
    auto file_buf = _file_buf;
    auto ret = shared_ptr<FILE>(File::create_file(file.data(), "wb"), [file_buf](FILE *fp) {
        if (fp) {
            fclose(fp);
        }
    });
    if (ret && setbuf) {
        setvbuf(ret.get(), _file_buf.get(), _IOFBF, _buf_size);
    }
    return ret;
}

void HlsMakerImp::setMediaSource(const string &vhost, const string &app, const string &stream_id) {
    _media_src = std::make_shared<HlsMediaSource>(vhost, app, stream_id);
    _info.app = app;
    _info.stream = stream_id;
    _info.vhost = vhost;
}

HlsMediaSource::Ptr HlsMakerImp::getMediaSource() const {
    return _media_src;
}

}//namespace mediakit