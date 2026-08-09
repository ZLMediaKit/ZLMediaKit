/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <algorithm>
#include <array>
#include <cerrno>
#include <cmath>
#include <ctime>
#include <cstdlib>
#include <functional>
#include <iomanip> 
#include <limits>
#include <mutex>
#include <set>
#include <sstream>
#include <sys/stat.h>
#include <vector>
#include "HlsMakerImp.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/File.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

namespace hls_file_detail {

static bool writeOpenedFileAndClose(const string &path, FILE *file, const char *data, size_t len) {
    const bool target_opened = file != nullptr;
    bool ok = file && data && len;
    if (ok) {
        ok = fwrite(data, 1, len, file) == len;
    }
    if (file) {
        if (fclose(file) != 0) {
            ok = false;
        }
    }
    if (!ok && target_opened) {
        File::delete_file(path);
    }
    return ok;
}

static bool copyOpenedFilesAndClose(const string &target_path, FILE *input, FILE *output) {
    const bool target_opened = output != nullptr;
    bool ok = input && output;
    if (ok) {
        std::array<char, 64 * 1024> buffer;
        while (true) {
            auto bytes = fread(buffer.data(), 1, buffer.size(), input);
            if (bytes && fwrite(buffer.data(), 1, bytes, output) != bytes) {
                ok = false;
                break;
            }
            if (bytes != buffer.size()) {
                ok = !ferror(input);
                break;
            }
        }
    }
    if (input && fclose(input) != 0) {
        ok = false;
    }
    if (output && fclose(output) != 0) {
        ok = false;
    }
    if (!ok && target_opened) {
        File::delete_file(target_path);
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

bool copyFileChecked(const string &source_path, const string &target_path) {
    std::unique_ptr<FILE, decltype(&fclose)> input(fopen(source_path.data(), "rb"), &fclose);
    auto output = input ? File::create_file(target_path.data(), "wb") : nullptr;
    return hls_file_detail::copyOpenedFilesAndClose(target_path, input.release(), output);
}

struct VodSegmentRecord {
    uint64_t session_id = 0;
    uint64_t media_index = 0;
    bool has_session_id = false;
    bool discontinuity = false;
    string file_name;
    string init_segment;
    string extinf_line;
};

bool parseUnsignedDecimal(const string &text, uint64_t &value) {
    if (text.empty()) {
        return false;
    }
    uint64_t result = 0;
    const auto max = (numeric_limits<uint64_t>::max)();
    for (auto ch : text) {
        if (ch < '0' || ch > '9') {
            return false;
        }
        auto digit = static_cast<uint64_t>(ch - '0');
        if (result > (max - digit) / 10) {
            return false;
        }
        result = result * 10 + digit;
    }
    value = result;
    return true;
}

bool parseExtinfLine(const string &line, uint64_t &duration_ms) {
    static const string prefix = "#EXTINF:";
    if (line.compare(0, prefix.size(), prefix) != 0) {
        return false;
    }
    errno = 0;
    char *end = nullptr;
    auto value = strtod(line.c_str() + prefix.size(), &end);
    if (errno == ERANGE || end == line.c_str() + prefix.size() || !end || *end != ',' || end[1] != '\0'
        || !std::isfinite(value) || value < 0
        || value > static_cast<double>((numeric_limits<uint64_t>::max)()) / 1000.0) {
        return false;
    }
    auto value_ms = std::ceil(value * 1000.0);
    if (value_ms >= static_cast<double>((numeric_limits<uint64_t>::max)())) {
        return false;
    }
    duration_ms = static_cast<uint64_t>(value_ms);
    return true;
}

bool parseSessionCoordinates(const string &uri, uint64_t &session_id, uint64_t &media_index) {
    auto file_begin = uri.find_last_of("/\\");
    file_begin = file_begin == string::npos ? 0 : file_begin + 1;
    auto extension = uri.find('.', file_begin);
    if (extension == string::npos || extension == file_begin) {
        return false;
    }
    auto index_separator = uri.rfind('_', extension);
    if (index_separator == string::npos || index_separator < file_begin) {
        return false;
    }
    auto session_separator = index_separator == 0 ? string::npos : uri.rfind('_', index_separator - 1);
    if (session_separator == string::npos || session_separator < file_begin) {
        return false;
    }
    return parseUnsignedDecimal(uri.substr(session_separator + 1, index_separator - session_separator - 1), session_id)
        && parseUnsignedDecimal(uri.substr(index_separator + 1, extension - index_separator - 1), media_index);
}

bool readPlaylistLine(istringstream &input, string &line) {
    if (!getline(input, line)) {
        return false;
    }
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    return true;
}

bool expectPlaylistLine(istringstream &input, const char *expected) {
    string line;
    return readPlaylistLine(input, line) && line == expected;
}

bool parseGeneratedVodPlaylist(const string &text, bool is_fmp4, uint64_t &target_duration,
                               vector<VodSegmentRecord> &segments) {
    // 这里只读取本项目生成的小时 VOD 子集；未知结构必须失败，避免合并时破坏已有归档。
    // Parse only the hourly VOD subset emitted here; reject unknown structures instead of corrupting an archive.
    istringstream input(text);
    string line;
    if (!expectPlaylistLine(input, "#EXTM3U")
        || !expectPlaylistLine(input, is_fmp4 ? "#EXT-X-VERSION:7" : "#EXT-X-VERSION:4")
        || !expectPlaylistLine(input, "#EXT-X-ALLOW-CACHE:YES")) {
        return false;
    }
    static const string target_prefix = "#EXT-X-TARGETDURATION:";
    if (!readPlaylistLine(input, line) || line.compare(0, target_prefix.size(), target_prefix) != 0
        || !parseUnsignedDecimal(line.substr(target_prefix.size()), target_duration)) {
        return false;
    }
    if (!expectPlaylistLine(input, "#EXT-X-MEDIA-SEQUENCE:0")) {
        return false;
    }

    bool pending_discontinuity = false;
    bool saw_endlist = false;
    bool map_pending = false;
    string current_init_segment;
    while (readPlaylistLine(input, line)) {
        if (line == "#EXT-X-ENDLIST") {
            saw_endlist = true;
            break;
        }
        if (line == "#EXT-X-DISCONTINUITY") {
            if (pending_discontinuity) {
                return false;
            }
            pending_discontinuity = true;
            continue;
        }
        static const string map_prefix = "#EXT-X-MAP:URI=\"";
        if (line.compare(0, map_prefix.size(), map_prefix) == 0) {
            if (!is_fmp4 || map_pending || line.size() <= map_prefix.size() + 1 || line.back() != '\"') {
                return false;
            }
            current_init_segment = line.substr(map_prefix.size(), line.size() - map_prefix.size() - 1);
            map_pending = true;
            continue;
        }
        uint64_t duration_ms = 0;
        if (!parseExtinfLine(line, duration_ms)) {
            return false;
        }
        string file_name;
        if (!readPlaylistLine(input, file_name) || file_name.empty() || file_name[0] == '#') {
            return false;
        }
        if (is_fmp4 && current_init_segment.empty()) {
            return false;
        }
        auto segment_target = duration_ms / 1000 + (duration_ms % 1000 != 0);
        if (segment_target > target_duration) {
            return false;
        }
        VodSegmentRecord segment;
        segment.discontinuity = pending_discontinuity;
        segment.extinf_line = line;
        segment.file_name = std::move(file_name);
        segment.init_segment = current_init_segment;
        segment.has_session_id = parseSessionCoordinates(segment.file_name, segment.session_id, segment.media_index);
        segments.emplace_back(std::move(segment));
        pending_discontinuity = false;
        map_pending = false;
    }
    if (!saw_endlist || pending_discontinuity || map_pending || segments.empty()) {
        return false;
    }
    while (readPlaylistLine(input, line)) {
        if (!line.empty()) {
            return false;
        }
    }
    return true;
}

void orderVodSegments(vector<VodSegmentRecord> &segments) {
    set<string> seen_uris;
    segments.erase(remove_if(segments.begin(), segments.end(), [&](const VodSegmentRecord &segment) {
        return !seen_uris.emplace(segment.file_name).second;
    }), segments.end());

    auto first_session = stable_partition(segments.begin(), segments.end(), [](const VodSegmentRecord &segment) {
        return !segment.has_session_id;
    });
    // fileNameId 只提供同一进程内的 recorder 创建顺序；旧格式记录保持为不可重排的历史前缀。
    // fileNameId orders recorder creation only within this process; legacy records remain an opaque history prefix.
    stable_sort(first_session, segments.end(), [](const VodSegmentRecord &left, const VodSegmentRecord &right) {
        return left.session_id < right.session_id
            || (left.session_id == right.session_id && left.media_index < right.media_index);
    });

    for (size_t index = 1; index < segments.size(); ++index) {
        auto &previous = segments[index - 1];
        auto &current = segments[index];
        if (current.has_session_id && (!previous.has_session_id || current.session_id != previous.session_id)) {
            current.discontinuity = true;
        }
    }
}

string serializeVodPlaylist(bool is_fmp4, uint64_t target_duration, const vector<VodSegmentRecord> &segments) {
    string index_str;
    index_str.reserve(2048);
    index_str += "#EXTM3U\n";
    index_str += (is_fmp4 ? "#EXT-X-VERSION:7\n" : "#EXT-X-VERSION:4\n");
    index_str += "#EXT-X-ALLOW-CACHE:YES\n";
    index_str += "#EXT-X-TARGETDURATION:" + to_string(target_duration) + "\n";
    index_str += "#EXT-X-MEDIA-SEQUENCE:0\n";

    string last_init_segment;
    for (auto &segment : segments) {
        if (segment.discontinuity) {
            index_str += "#EXT-X-DISCONTINUITY\n";
        }
        if (is_fmp4 && segment.init_segment != last_init_segment) {
            index_str += "#EXT-X-MAP:URI=\"" + segment.init_segment + "\"\n";
            last_init_segment = segment.init_segment;
        }
        index_str += segment.extinf_line;
        index_str += '\n';
        index_str += segment.file_name;
        index_str += '\n';
    }
    index_str += "#EXT-X-ENDLIST\n";
    return index_str;
}

mutex &vodPlaylistMutex(const string &path) {
    // 仅串行化低频的小时归档 read-modify-write，不进入媒体或 live playlist 路径；固定锁池保留到进程退出，
    // 避免晚销毁的 recorder 受静态对象析构顺序影响。
    // Serialize only low-frequency hourly archive updates, never media/live paths; keep the fixed pool alive until exit
    // so late recorder destruction cannot hit static teardown order.
    static auto *locks = new array<mutex, 64>();
    return (*locks)[hash<string>()(path) % locks->size()];
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
    cleanupDelayPlaylistIfDisabled();
    if (!isLive() || isKeep()) {
        if (eof && isFmp4() && !_current_init_segment_referenced) {
            const auto &current_init_segment = getCurrentInitSegment();
            if (!current_init_segment.empty()) {
                auto init_path = _path_prefix + "/" + current_init_segment;
                File::delete_file(init_path.data(), true);
            }
        }
        return;
    }

    std::list<std::string> files;
    files.emplace_back(_path_hls);
    if (_delay_playlist_may_exist) {
        files.emplace_back(_path_hls_delay);
    }
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
        // 主/延迟 playlist 使用固定路径，旧 recorder 的延迟任务可能在新 recorder 启动后删除它们。
        // 媒体片和 init 使用 recorder 唯一文件名，不会被旧任务误删；playlist 会在下一次成功切片时重写，
        // 若没有后续切片则保持缺失。不能简单排除 playlist，否则流真正结束后会遗留引用已删除切片的陈旧索引。
        // Main/delay playlists use stable paths, so an old recorder's delayed task may delete them after a replacement starts.
        // Media and init files have recorder-unique names and remain safe; playlists are rewritten by the next successful segment,
        // or stay absent if no segment follows. Excluding playlists would leave a stale index after the stream actually ends.
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

    // 失败也必须结束本小时批次，避免后续目录保存时混入旧目录的相对 URI。
    // A failed save still consumes this hourly batch so relative URIs cannot leak into the next directory.
    deque<DirSegmentInfo> current_batch;
    current_batch.swap(_current_dir_seg_list);

    if (isFmp4()) {
        set<string> init_segments;
        for (auto &segment : current_batch) {
            init_segments.emplace(segment.init_segment);
        }
        for (auto &init_segment : init_segments) {
            auto source_path = _path_prefix + "/" + init_segment;
            auto target_path = _path_prefix + "/" + _current_dir + init_segment;
            if (!copyFileChecked(source_path, target_path)) {
                WarnL << "Copy fMP4 init segment failed: " << source_path << " -> " << target_path;
                return;
            }
        }
    }

    auto vod_path = _path_prefix + "/" + _current_dir + (isFmp4() ? "vod.fmp4.m3u8" : "vod.m3u8");
    lock_guard<mutex> lock(vodPlaylistMutex(vod_path));
    uint64_t target_duration = 0;
    vector<VodSegmentRecord> segments;
    if (File::fileExist(vod_path)) {
        auto previous = File::loadFile(vod_path);
        if (previous.empty() || !parseGeneratedVodPlaylist(previous, isFmp4(), target_duration, segments)) {
            WarnL << "Parse existing VOD playlist failed: " << vod_path;
            return;
        }
    }

    stringstream extinf;
    for (auto &segment : current_batch) {
        VodSegmentRecord record;
        record.discontinuity = segment.discontinuity;
        record.file_name = std::move(segment.file_name);
        record.init_segment = std::move(segment.init_segment);
        extinf.str(string());
        extinf.clear();
        extinf << "#EXTINF:" << setprecision(3) << segment.duration_ms / 1000.0 << ',';
        record.extinf_line = extinf.str();
        record.has_session_id = parseSessionCoordinates(record.file_name, record.session_id, record.media_index);
        segments.emplace_back(std::move(record));

        auto segment_target = segment.duration_ms / 1000 + (segment.duration_ms % 1000 != 0);
        if (segment_target > target_duration) {
            target_duration = segment_target;
        }
    }

    orderVodSegments(segments);

    auto index_str = serializeVodPlaylist(isFmp4(), target_duration, segments);
    // 沿用现有跨平台直接写入语义，不引入 Windows 行为不同的 rename 替换流程。
    // Keep the existing cross-platform direct-write behavior; do not add rename-based replacement semantics.
    auto output = File::create_file(vod_path.data(), "wb");
    if (!hls_file_detail::writeOpenedFileAndClose(vod_path, output, index_str.data(), index_str.size())) {
        WarnL << "Save VOD playlist failed: " << vod_path << " " << get_uv_errmsg();
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
    if (!hls_file_detail::writeOpenedFileAndClose(init_seg_path, file, data, len)) {
        WarnL << "Write init segment failed," << init_seg_path << " " << get_uv_errmsg();
        return false;
    }
    if (!isLive() || isKeep()) {
        _current_init_segment_referenced = false;
    }
    if (!previous_init_segment.empty() && !previous_init_referenced) {
        auto previous_init_path = _path_prefix + "/" + previous_init_segment;
        if (File::delete_file(previous_init_path.data(), true) != 0) {
            WarnL << "Delete unused init segment failed," << previous_init_path << " " << get_uv_errmsg();
        }
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
        if (include_delay) {
            // 以写模式打开后路径已可能被创建或截断，必须立即记录“可能存在”。
            // Opening in write mode may create or truncate the path, so mark it as possibly present immediately.
            _delay_playlist_may_exist = true;
        }
        fwrite(data.data(), data.size(), 1, hls.get());
        hls.reset();
        if (_media_src && !include_delay) {
            _media_src->setIndexFile(data);
        }
    } else {
        WarnL << "Create hls file failed," << path << " " << get_uv_errmsg();
    }
}

void HlsMakerImp::cleanupDelayPlaylistIfDisabled() {
    GET_CONFIG(uint32_t, seg_delay, Hls::kSegmentDelay);
    if (seg_delay || !_delay_playlist_may_exist) {
        return;
    }
    if (File::delete_file(_path_hls_delay) == 0 || !File::fileExist(_path_hls_delay)) {
        _delay_playlist_may_exist = false;
    }
}

void HlsMakerImp::onFlushLastSegment(uint64_t duration_ms, bool discontinuity, const std::string &init_segment) {
    // 关闭并flush文件到磁盘  [AUTO-TRANSLATED:9798ec4d]
    // Close and flush file to disk
    _file = nullptr;
    cleanupDelayPlaylistIfDisabled();
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
