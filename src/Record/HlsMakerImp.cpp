﻿/*
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
#include "Util/File.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

HlsMakerImp::HlsMakerImp(bool is_fmp4, const string &m3u8_file, const string &params, uint32_t bufSize, float seg_duration,
                         uint32_t seg_number, bool seg_keep) : HlsMaker(is_fmp4, seg_duration, seg_number, seg_keep) {
    _poller = EventPollerPool::Instance().getPoller();
    _path_prefix = m3u8_file.substr(0, m3u8_file.rfind('/'));
    _path_hls = m3u8_file;
    _path_hls_time = m3u8_file;
    _params = params;
    _buf_size = bufSize;
    _file_buf.reset(new char[bufSize], [](char *ptr) { delete[] ptr; });
    _info.folder = _path_prefix;
}

HlsMakerImp::~HlsMakerImp() {
    try {
        // 可能hls注册时导致抛异常
        clearCache(false, true);
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }
}

void HlsMakerImp::clearCache() {
    clearCache(true, false);
}

void HlsMakerImp::clearCache(bool immediately, bool eof) {
    // 录制完了
    flushLastSegment(eof);
    if (!isLive() || isKeep()) {
        return;
    }

    clear();
    _file = nullptr;
    _segment_file_paths.clear();

    // hls直播才删除文件
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
        segment_name = StrPrinter << strDate + "/" + strHour + "/" + strTime << "_" << index << (isFmp4() ? ".mp4" : ".ts");
        segment_path = _path_prefix + "/" + segment_name;
        if (isLive()) {
            _segment_file_paths.emplace(index, segment_path);
        }
    }
    _file = makeFile(segment_path, true);

    // 保存本切片的元数据
    _info.start_time = ::time(NULL);
    _info.file_name = segment_name;
    _info.file_path = segment_path;
    _info.url = _info.app + "/" + _info.stream + "/" + segment_name;

    if (!_file) {
        WarnL << "Create file failed," << segment_path << " " << get_uv_errmsg();
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

void HlsMakerImp::onWriteInitSegment(const char *data, size_t len) {
    string init_seg_path = _path_prefix + "/init.mp4";
    _file = makeFile(init_seg_path, true);

    if (_file) {
        fwrite(data, len, 1, _file.get());
        _file = nullptr;
    } else {
        WarnL << "Create file failed," << init_seg_path << " " << get_uv_errmsg();
    }
}

void HlsMakerImp::onWriteSegment(const char *data, size_t len) {
    if (_file) {
        fwrite(data, len, 1, _file.get());
    }
    if (_media_src) {
        _media_src->onSegmentSize(len);
    }
}

void HlsMakerImp::onWriteHls(const std::string &data) {
    auto hls = makeFile(_path_hls);
    if (hls) {
        fwrite(data.data(), data.size(), 1, hls.get());
        hls.reset();
        if (_media_src) {
            _media_src->setIndexFile(data);
        }
    } else {
        WarnL << "Create hls file failed," << _path_hls << " " << get_uv_errmsg();
    }
}

void HlsMakerImp::onWriteHlsTime(const std::string &data) {
    auto strDate = getTimeStr("%Y-%m-%d");
    auto strHour = getTimeStr("%H");
    string path_new = StrPrinter << _path_prefix + "/" << strDate + "/" + strHour + "/" +  "index.m3u8";
    auto hls1 = makeFile(path_new);
    if (hls1) {
        fwrite(data.data(), data.size(), 1, hls1.get());
        hls1.reset();
        if (_media_src) {
            _media_src->setIndexFile(data);
        }
    } else {
        WarnL << "Create hls file failed," << path_new << " " << get_uv_errmsg();
    }
}


void HlsMakerImp::onFlushLastSegment(uint64_t duration_ms) {
    // 关闭并flush文件到磁盘
    _file = nullptr;

    GET_CONFIG(bool, broadcastRecordTs, Hls::kBroadcastRecordTs);
    if (broadcastRecordTs) {
        _info.time_len = duration_ms / 1000.0f;
        _info.file_size = File::fileSize(_info.file_path.data());
        NOTICE_EMIT(BroadcastRecordTsArgs, Broadcast::kBroadcastRecordTs, _info);
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
    _info.app = app;
    _info.stream = stream_id;
    _info.vhost = vhost;
    _media_src = std::make_shared<HlsMediaSource>(isFmp4() ? HLS_FMP4_SCHEMA : HLS_SCHEMA, _info);
}

HlsMediaSource::Ptr HlsMakerImp::getMediaSource() const {
    return _media_src;
}

} // namespace mediakit