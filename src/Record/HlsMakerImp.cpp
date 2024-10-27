/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <ctime>
#include <iomanip> 
#include <sys/stat.h>
#include "HlsMakerImp.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/File.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

std::string getDelayPath(const std::string& originalPath) {
    std::size_t pos = originalPath.find(".m3u8");
    if (pos != std::string::npos) {
        return originalPath.substr(0, pos) + "_delay.m3u8";
    }
    return originalPath;
}

HlsMakerImp::HlsMakerImp(bool is_fmp4, const string &m3u8_file, const string &params, uint32_t bufSize, float seg_duration,
                         uint32_t seg_number, bool seg_keep) : HlsMaker(is_fmp4, seg_duration, seg_number, seg_keep) {
    _poller = EventPollerPool::Instance().getPoller();
    _path_prefix = m3u8_file.substr(0, m3u8_file.rfind('/'));
    _path_hls = m3u8_file;
    _path_hls_delay = getDelayPath(m3u8_file);
    _params = params;
    _buf_size = bufSize;
    _file_buf.reset(new char[bufSize], [](char *ptr) { delete[] ptr; });
    _info.folder = _path_prefix;
}

HlsMakerImp::~HlsMakerImp() {
    try {
        // 可能hls注册时导致抛异常  [AUTO-TRANSLATED:82add30d]
        // Possible exception thrown during hls registration
        clearCache(false, true);
    } catch (std::exception &ex) {
        WarnL << ex.what();
    }

    if (!isLive() || isKeep()) {
        saveCurrentDir();
    }
}

void HlsMakerImp::clearCache() {
    clearCache(true, false);
}

static void clearHls(const std::list<std::string> &files) {
    for (auto &file : files) {
        File::delete_file(file);
    }
    File::deleteEmptyDir(File::parentDir(files.back()));
}

void HlsMakerImp::clearCache(bool immediately, bool eof) {
    // 录制完了  [AUTO-TRANSLATED:5d3bfbeb]
    // Recording finished
    flushLastSegment(eof);
    if (!isLive() || isKeep()) {
        return;
    }

    {
        std::list<std::string> lst;
        lst.emplace_back(_path_hls);
        lst.emplace_back(_path_hls_delay);
        if (!_path_init.empty() && eof) {
            lst.emplace_back(_path_init);
        }
        for (auto &pr : _segment_file_paths) {
            lst.emplace_back(std::move(pr.second));
        }

        // hls直播才删除文件  [AUTO-TRANSLATED:81d2aaa5]
        // Delete file only after hls live streaming
        GET_CONFIG(uint32_t, delay, Hls::kDeleteDelaySec);
        if (!delay || immediately) {
            clearHls(lst);
        } else {
            _poller->doDelayTask(delay * 1000, [lst]() {
                clearHls(lst);
                return 0;
            });
        }
    }

    clear();
    _file = nullptr;
    _segment_file_paths.clear();
}

/** 写入该目录的init.mp4文件以及m3u8文件 **/
void HlsMakerImp::saveCurrentDir() {
    if (_current_dir.empty() || _current_dir_seg_list.empty()) {
        return;
    }
    if (isFmp4()) {
        // 写入init.mp4文件
        File::saveFile(_current_dir_init_file, _path_prefix + "/" + _current_dir + "init.mp4");
    }

    int maxSegmentDuration = 0;
    for (auto &tp : _current_dir_seg_list) {
        int dur = std::get<0>(tp);
        if (dur > maxSegmentDuration) {
            maxSegmentDuration = dur;
        }
    }

    string index_str;
    index_str.reserve(2048);
    index_str += "#EXTM3U\n";
    index_str += (isFmp4() ? "#EXT-X-VERSION:7\n" : "#EXT-X-VERSION:4\n");
    index_str += "#EXT-X-ALLOW-CACHE:YES\n";
    index_str += "#EXT-X-TARGETDURATION:" + std::to_string((maxSegmentDuration + 999) / 1000) + "\n";
    index_str += "#EXT-X-MEDIA-SEQUENCE:0\n";
    if (isFmp4()) {
        index_str += "#EXT-X-MAP:URI=\"init.mp4\"\n";
    }
    stringstream ss;
    for (auto &t : _current_dir_seg_list) {
        ss << "#EXTINF:" << std::setprecision(3) << std::get<0>(t) / 1000.0 << ",\n" << std::get<1>(t) << "\n";
    }
    _current_dir_seg_list.clear();
    index_str += ss.str();
    index_str += "#EXT-X-ENDLIST\n";

    /** 写入该目录的m3u8文件 **/
    File::saveFile(index_str, _path_prefix + "/" + _current_dir + (isFmp4() ? "vod.fmp4.m3u8" : "vod.m3u8"));
}

string HlsMakerImp::onOpenSegment(uint64_t index) {
    string segment_name, segment_path;
    {
        auto strDate = getTimeStr("%Y-%m-%d");
        auto strHour = getTimeStr("%H");
        auto strTime = getTimeStr("%M-%S");
        auto current_dir = strDate + "/" + strHour + "/";
        segment_name = current_dir + strTime + "_" + std::to_string(index) + (isFmp4() ? ".mp4" : ".ts");
        segment_path = _path_prefix + "/" + segment_name;
        if (isLive()) {
            // 直播
            _segment_file_paths.emplace(index, segment_path);
        }
        if (!isLive() || isKeep()) {
            // 目录将发生变更，保留ts切片时，每个目录都生成一个m3u8文件
            if (!_current_dir.empty() && current_dir != _current_dir) {
                saveCurrentDir();
            }
            _current_dir = std::move(current_dir);
        }
    }
    _file = makeFile(segment_path, true);

    // 保存本切片的元数据  [AUTO-TRANSLATED:64e6f692]
    // Save metadata for this slice
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
    File::delete_file(it->second.data(), true);
    _segment_file_paths.erase(it);
}

void HlsMakerImp::onWriteInitSegment(const char *data, size_t len) {
    if (!isLive() || isKeep()) {
        _current_dir_init_file.assign(data, len);
    }
    string init_seg_path = _path_prefix + "/init.mp4";
    auto file = makeFile(init_seg_path);
    if (file) {
        fwrite(data, len, 1, file.get());
        _path_init = std::move(init_seg_path);
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

void HlsMakerImp::onWriteHls(const std::string &data, bool include_delay) {
    auto path = include_delay ? _path_hls_delay : _path_hls;
    auto hls = makeFile(path);
    if (hls) {
        fwrite(data.data(), data.size(), 1, hls.get());
        hls.reset();
        if (_media_src && !include_delay) {
            _media_src->setIndexFile(data);
        }
    } else {
        WarnL << "Create hls file failed," << path << " " << get_uv_errmsg();
    }
}

void HlsMakerImp::onFlushLastSegment(uint64_t duration_ms) {
    // 关闭并flush文件到磁盘  [AUTO-TRANSLATED:9798ec4d]
    // Close and flush file to disk
    _file = nullptr;
    if (!isLive() || isKeep()) {
        _current_dir_seg_list.emplace_back(duration_ms, _info.file_name.erase(0, _current_dir.size()));
    }
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

void HlsMakerImp::setMediaSource(const MediaTuple& tuple) {
    static_cast<MediaTuple &>(_info) = tuple;
    _media_src = std::make_shared<HlsMediaSource>(isFmp4() ? HLS_FMP4_SCHEMA : HLS_SCHEMA, _info);
}

HlsMediaSource::Ptr HlsMakerImp::getMediaSource() const {
    return _media_src;
}

} // namespace mediakit