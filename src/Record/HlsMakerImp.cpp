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
#include <set>
#include <sys/stat.h>
#include "HlsMakerImp.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/File.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

namespace hls_file_detail {

using BeforeClose = void (*)(FILE *);

bool writeOpenedFileAndClose(const string &path, FILE *file, const char *data, size_t len,
                             BeforeClose before_close) {
    bool ok = file && data && len;
    if (ok) {
        ok = fwrite(data, 1, len, file) == len;
    }
    if (file) {
        if (before_close) {
            before_close(file);
        }
        if (fclose(file) != 0) {
            ok = false;
        }
    }
    if (!ok) {
        File::delete_file(path);
    }
    return ok;
}

} // namespace hls_file_detail

namespace {

void clearHls(const std::list<std::string> &files) {
    for (auto &file : files) {
        File::delete_file(file);
    }
    File::deleteEmptyDir(File::parentDir(files.back()));
}

} // namespace

std::string getDelayPath(const std::string& originalPath) {
    std::size_t pos = originalPath.find(".m3u8");
    if (pos != std::string::npos) {
        return originalPath.substr(0, pos) + "_delay.m3u8";
    }
    return originalPath;
}

HlsMakerImp::HlsMakerImp(bool is_fmp4, const string &m3u8_file, const string &params, uint32_t bufSize, float seg_duration,
                         uint32_t seg_number, bool seg_keep, const string &fmp4_seg_ext, float seg_max_duration)
    : HlsMaker(is_fmp4, seg_duration, seg_number, seg_keep, seg_max_duration) {
    _poller = EventPollerPool::Instance().getPoller();
    _path_prefix = m3u8_file.substr(0, m3u8_file.rfind('/'));
    _path_hls = m3u8_file;
    _path_hls_delay = getDelayPath(m3u8_file);
    _params = params;
    _buf_size = bufSize;
    // 兼容用户配置不带前导点的扩展名(例如 m4s)，统一补上"."  [AUTO-TRANSLATED]
    // Tolerate user-configured extensions without a leading dot (e.g. m4s) by normalizing to ".m4s"
    _fmp4_seg_ext = fmp4_seg_ext.empty() ? ".mp4" : (fmp4_seg_ext.front() == '.' ? fmp4_seg_ext : "." + fmp4_seg_ext);
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

void HlsMakerImp::clearCache(bool immediately, bool eof) {
    // 录制完了  [AUTO-TRANSLATED:5d3bfbeb]
    // Recording finished
    flushLastSegment(eof);
    if (!isLive() || isKeep()) {
        return;
    }

    std::list<std::string> files;
    files.emplace_back(_path_hls);
    files.emplace_back(_path_hls_delay);
    std::set<std::string> init_segments;
    const auto &current_init_segment = getCurrentInitSegment();
    for (auto &segment : _segment_files) {
        files.emplace_back(std::move(segment.file_path));
        if (!segment.init_segment.empty() && (eof || segment.init_segment != current_init_segment)) {
            init_segments.emplace(segment.init_segment);
        }
    }
    if (eof && !current_init_segment.empty()) {
        init_segments.emplace(current_init_segment);
    }
    for (auto &init_segment : init_segments) {
        files.emplace_back(_path_prefix + "/" + init_segment);
    }

    // hls直播才删除文件  [AUTO-TRANSLATED:81d2aaa5]
    // Delete file only after hls live streaming
    GET_CONFIG(uint32_t, delay, Hls::kDeleteDelaySec);
    if (!delay || immediately) {
        clearHls(files);
    } else {
        _poller->doDelayTask(uint64_t(delay) * 1000, [files]() {
            clearHls(files);
            return 0;
        });
    }

    _file = nullptr;
    _segment_files.clear();
    _current_init_segment_referenced = false;
    clear();
}

/** 写入该目录引用的初始化段以及m3u8文件 **/
void HlsMakerImp::saveCurrentDir() {
    if (_current_dir.empty() || _current_dir_seg_list.empty()) {
        return;
    }

    uint64_t max_segment_duration = 0;
    for (auto &segment : _current_dir_seg_list) {
        if (segment.duration_ms > max_segment_duration) {
            max_segment_duration = segment.duration_ms;
        }
    }

    string index_str;
    index_str.reserve(2048);
    index_str += "#EXTM3U\n";
    index_str += (isFmp4() ? "#EXT-X-VERSION:7\n" : "#EXT-X-VERSION:4\n");
    index_str += "#EXT-X-ALLOW-CACHE:YES\n";
    auto target_duration = max_segment_duration / 1000 + (max_segment_duration % 1000 != 0);
    index_str += "#EXT-X-TARGETDURATION:" + std::to_string(target_duration) + "\n";
    index_str += "#EXT-X-MEDIA-SEQUENCE:0\n";

    stringstream ss;
    string last_init_segment;
    set<string> saved_init_segments;
    for (auto &segment : _current_dir_seg_list) {
        if (segment.discontinuity) {
            ss << "#EXT-X-DISCONTINUITY\n";
        }
        if (isFmp4() && segment.init_segment != last_init_segment) {
            ss << "#EXT-X-MAP:URI=\"" << segment.init_segment << "\"\n";
            last_init_segment = segment.init_segment;
        }
        if (isFmp4() && saved_init_segments.emplace(segment.init_segment).second) {
            auto it = _init_segments.find(segment.init_segment);
            if (it == _init_segments.end()) {
                WarnL << "Missing fMP4 init segment: " << segment.init_segment;
            } else {
                auto path = _path_prefix + "/" + _current_dir + segment.init_segment;
                if (!File::saveFile(it->second, path)) {
                    WarnL << "Save fMP4 init segment failed: " << path;
                }
            }
        }
        ss << "#EXTINF:" << std::setprecision(3) << segment.duration_ms / 1000.0 << ",\n" << segment.file_name << "\n";
    }
    _current_dir_seg_list.clear();
    index_str += ss.str();
    index_str += "#EXT-X-ENDLIST\n";

    /** 写入该目录的m3u8文件 **/
    File::saveFile(index_str, _path_prefix + "/" + _current_dir + (isFmp4() ? "vod.fmp4.m3u8" : "vod.m3u8"));

    // 已归档的旧 generation 不再需要常驻内存；当前 generation 可能继续用于下一个小时目录。
    // Archived generations need not stay in memory; the current one may still be used by the next hourly directory.
    auto current_init_segment = getCurrentInitSegment();
    for (auto it = _init_segments.begin(); it != _init_segments.end();) {
        if (it->first == current_init_segment) {
            ++it;
        } else {
            it = _init_segments.erase(it);
        }
    }
}

string HlsMakerImp::onOpenSegment(uint64_t index) {
    auto media_index = _media_file_index++;
    auto strDate = getTimeStr("%Y-%m-%d");
    auto strHour = getTimeStr("%H");
    auto strTime = getTimeStr("%M-%S");
    auto current_dir = strDate + "/" + strHour + "/";
    auto segment_name = current_dir + strTime + "_" + std::to_string(getFileNameId()) + "_"
                      + std::to_string(media_index) + (isFmp4() ? _fmp4_seg_ext : ".ts");
    auto segment_path = _path_prefix + "/" + segment_name;
    _file = makeFile(segment_path, true);
    if (!_file) {
        WarnL << "Create file failed," << segment_path << " " << get_uv_errmsg();
        return {};
    }

    auto init_segment = isFmp4() ? getCurrentInitSegment() : string();
    if (isLive() && !isKeep()) {
        _segment_files.emplace_back(index, segment_path, init_segment);
    } else {
        if (!_current_dir.empty() && current_dir != _current_dir) {
            saveCurrentDir();
        }
        _current_dir = std::move(current_dir);
        if (isFmp4()) {
            _current_init_segment_referenced = true;
        }
    }

    // 保存本切片的元数据  [AUTO-TRANSLATED:64e6f692]
    // Save metadata for this slice
    _info.start_time = ::time(NULL);
    _info.file_name = segment_name;
    _info.file_path = segment_path;
    _info.url = _info.app + "/" + _info.stream + "/" + segment_name;

    if (_params.empty()) {
        return segment_name;
    }
    return segment_name + "?" + _params;
}

void HlsMakerImp::onDelSegment(uint64_t index) {
    const auto &current_init_segment = getCurrentInitSegment();
    while (!_segment_files.empty() && _segment_files.front().index <= index) {
        // 同一 init 的媒体记录必然连续；先 move 到局部值再弹出，避免 pop 后访问失效引用。
        // Media records sharing an init are necessarily contiguous; move before pop to avoid invalid references.
        auto segment = std::move(_segment_files.front());
        _segment_files.pop_front();
        File::delete_file(segment.file_path.data(), true);
        if (!segment.init_segment.empty() && segment.init_segment != current_init_segment
            && (_segment_files.empty() || _segment_files.front().init_segment != segment.init_segment)) {
            auto init_path = _path_prefix + "/" + segment.init_segment;
            File::delete_file(init_path.data(), true);
        }
    }
}

bool HlsMakerImp::onWriteInitSegment(const string &init_segment, const char *data, size_t len) {
    const auto previous_init_segment = getCurrentInitSegment();
    bool previous_init_referenced = false;
    if (!previous_init_segment.empty()) {
        if (isLive() && !isKeep()) {
            previous_init_referenced = !_segment_files.empty()
                                     && _segment_files.back().init_segment == previous_init_segment;
        } else {
            previous_init_referenced = _current_init_segment_referenced;
        }
    }

    string init_seg_path = _path_prefix + "/" + init_segment;
    auto file = data && len ? File::create_file(init_seg_path.data(), "wb") : nullptr;
    if (!hls_file_detail::writeOpenedFileAndClose(init_seg_path, file, data, len, nullptr)) {
        WarnL << "Write init segment failed," << init_seg_path << " " << get_uv_errmsg();
        return false;
    }
    if (!isLive() || isKeep()) {
        _init_segments[init_segment].assign(data, len);
        _current_init_segment_referenced = false;
    }
    if (!previous_init_segment.empty() && !previous_init_referenced) {
        auto previous_init_path = _path_prefix + "/" + previous_init_segment;
        if (File::delete_file(previous_init_path.data(), true) != 0) {
            WarnL << "Delete unused init segment failed," << previous_init_path << " " << get_uv_errmsg();
        }
        _init_segments.erase(previous_init_segment);
    }
    return true;
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

void HlsMakerImp::onFlushLastSegment(uint64_t duration_ms, bool discontinuity, const std::string &init_segment) {
    // 关闭并flush文件到磁盘  [AUTO-TRANSLATED:9798ec4d]
    // Close and flush file to disk
    _file = nullptr;
    if (!isLive() || isKeep()) {
        auto file_name = _info.file_name;
        if (file_name.compare(0, _current_dir.size(), _current_dir) == 0) {
            file_name.erase(0, _current_dir.size());
        }
        _current_dir_seg_list.emplace_back(duration_ms, std::move(file_name), discontinuity, init_segment);
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
