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
#include <mutex>
#include <set>
#include <sys/stat.h>
#include <unordered_map>
#include "HlsMakerImp.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/File.h"
#include "Common/config.h"

using namespace std;
using namespace toolkit;

namespace mediakit {

struct HlsCleanupOwner {
    std::mutex mutex;
};

namespace {

void clearHls(const std::list<std::string> &files) {
    for (auto &file : files) {
        File::delete_file(file);
    }
    File::deleteEmptyDir(File::parentDir(files.back()));
}

// 延时任务与同路径新会话共享一次性票据，确保旧文件只被删除一次。
// The delayed task and a replacement session share a once-only ticket so old files are deleted exactly once.
struct CleanupTicket {
    CleanupTicket(std::shared_ptr<HlsCleanupOwner> owner, std::list<std::string> files)
        : owner(std::move(owner)), files(std::move(files)) {}

    std::once_flag once;
    std::shared_ptr<HlsCleanupOwner> owner;
    std::list<std::string> files;
    bool ready = false;
};

struct CleanupPathState {
    std::weak_ptr<HlsCleanupOwner> latest_owner;
    bool owner_active = false;
    std::list<std::shared_ptr<CleanupTicket>> pending;
};

bool hasActiveCleanupOwner(const CleanupPathState &state) {
    return state.owner_active && !state.latest_owner.expired();
}

// 同一路径理论上只有一个 recorder；保留列表仍可安全容纳尚未注销的历史任务。
// A path normally has one recorder; the list also safely retains historical tasks not yet unregistered.
struct CleanupRegistry {
    std::mutex mutex;
    std::unordered_map<std::string, CleanupPathState> paths;
};

std::shared_ptr<CleanupRegistry> getCleanupRegistry() {
    static auto registry = std::make_shared<CleanupRegistry>();
    return registry;
}

void runCleanup(const std::shared_ptr<CleanupTicket> &ticket) {
    if (!ticket) {
        return;
    }
    std::call_once(ticket->once, [&ticket]() { clearHls(ticket->files); });
}

void finishCleanup(const std::shared_ptr<CleanupRegistry> &registry, const std::string &path,
                   const std::list<std::shared_ptr<CleanupTicket>> &tickets) {
    std::lock_guard<std::mutex> lock(registry->mutex);
    auto it = registry->paths.find(path);
    if (it == registry->paths.end()) {
        return;
    }
    for (auto &ticket : tickets) {
        it->second.pending.remove(ticket);
    }
    if (!hasActiveCleanupOwner(it->second) && it->second.pending.empty()) {
        registry->paths.erase(it);
    }
}

void drainPendingCleanup(const std::string &path) {
    auto registry = getCleanupRegistry();
    std::list<std::shared_ptr<CleanupTicket>> tickets;
    {
        std::lock_guard<std::mutex> lock(registry->mutex);
        auto it = registry->paths.find(path);
        if (it == registry->paths.end()) {
            return;
        }
        tickets.splice(tickets.end(), it->second.pending);
        if (!hasActiveCleanupOwner(it->second)) {
            registry->paths.erase(it);
        }
    }
    // 新会话必须等旧文件清理完成后再开始写入；延时回调随后由 call_once 保证不再重复删除。
    // A replacement waits for old-file cleanup before writing; call_once makes the delayed callback a no-op later.
    for (auto &ticket : tickets) {
        runCleanup(ticket);
    }
}

std::shared_ptr<HlsCleanupOwner> beginCleanupSession(const std::string &path) {
    auto registry = getCleanupRegistry();
    auto owner = std::make_shared<HlsCleanupOwner>();
    std::shared_ptr<HlsCleanupOwner> previous_owner;
    std::list<std::shared_ptr<CleanupTicket>> tickets;
    {
        std::lock_guard<std::mutex> lock(registry->mutex);
        auto &state = registry->paths[path];
        previous_owner = state.latest_owner.lock();
        state.latest_owner = owner;
        state.owner_active = true;
        tickets.splice(tickets.end(), state.pending);
    }
    if (previous_owner) {
        std::lock_guard<std::mutex> lock(previous_owner->mutex);
    }
    // 新 owner 返回前必须与已开始的旧清理会合，确保文件删除不会与新写入重叠。
    // Join any old cleanup before returning the new owner so deletion cannot overlap new writes.
    for (auto &ticket : tickets) {
        runCleanup(ticket);
    }
    return owner;
}

std::shared_ptr<CleanupTicket> registerCleanup(const std::string &path, std::list<std::string> files,
                                               const std::shared_ptr<HlsCleanupOwner> &owner,
                                               std::shared_ptr<CleanupRegistry> &registry) {
    registry = getCleanupRegistry();
    auto ticket = std::make_shared<CleanupTicket>(owner, std::move(files));
    std::lock_guard<std::mutex> lock(registry->mutex);
    auto &state = registry->paths[path];
    state.pending.emplace_back(ticket);
    auto latest_owner = state.latest_owner.lock();
    if (!latest_owner) {
        state.latest_owner = owner;
        state.owner_active = false;
    } else if (latest_owner == owner) {
        state.owner_active = false;
    }
    return ticket;
}

bool isLatestCleanupOwner(const std::string &path, const std::shared_ptr<HlsCleanupOwner> &owner) {
    auto registry = getCleanupRegistry();
    std::lock_guard<std::mutex> lock(registry->mutex);
    auto it = registry->paths.find(path);
    if (it == registry->paths.end()) {
        return true;
    }
    auto latest_owner = it->second.latest_owner.lock();
    return !latest_owner || latest_owner == owner;
}

void tryCleanup(const std::shared_ptr<CleanupRegistry> &registry, const std::string &path,
                const std::shared_ptr<CleanupTicket> &ready_ticket) {
    std::list<std::shared_ptr<CleanupTicket>> tickets;
    {
        std::lock_guard<std::mutex> lock(registry->mutex);
        auto it = registry->paths.find(path);
        if (it == registry->paths.end()) {
            return;
        }
        ready_ticket->ready = true;
        if (hasActiveCleanupOwner(it->second) || it->second.pending.empty()) {
            return;
        }
        for (auto &ticket : it->second.pending) {
            if (!ticket->ready) {
                return;
            }
        }
        // 执行期间保留登记，使并发构造能通过 call_once 等待同一批删除完成。
        // Keep registration while running so a concurrent constructor can join through call_once.
        tickets = it->second.pending;
    }
    for (auto &ticket : tickets) {
        runCleanup(ticket);
    }
    finishCleanup(registry, path, tickets);
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
    if (isLive() && !isKeep()) {
        _cleanup_owner = beginCleanupSession(_path_hls);
    } else {
        drainPendingCleanup(_path_hls);
    }
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
    if (_cleanup_owner && isLatestCleanupOwner(_path_hls, _cleanup_owner)) {
        drainPendingCleanup(_path_hls);
    }
    clearCache(true, false);
}

void HlsMakerImp::clearCache(bool immediately, bool eof) {
    // 录制完了  [AUTO-TRANSLATED:5d3bfbeb]
    // Recording finished
    std::unique_lock<std::mutex> owner_lock;
    if (eof && _cleanup_owner) {
        owner_lock = std::unique_lock<std::mutex>(_cleanup_owner->mutex);
    }
    if (!eof || !_cleanup_owner || isLatestCleanupOwner(_path_hls, _cleanup_owner)) {
        flushLastSegment(eof);
    } else {
        // 被新 generation 替代后不得再重写共享 playlist；只关闭旧媒体文件并登记回收。
        // A replaced generation must not rewrite the shared playlist; close its media file and only register cleanup.
        _file = nullptr;
    }
    if (!isLive() || isKeep()) {
        return;
    }

    {
        std::list<std::string> lst;
        lst.emplace_back(_path_hls);
        lst.emplace_back(_path_hls_delay);
        auto &current_init_segment = getCurrentInitSegment();
        for (auto &pr : _init_segment_paths) {
            if (eof || pr.first != current_init_segment) {
                lst.emplace_back(pr.second);
            }
        }
        for (auto &pr : _segment_files) {
            lst.emplace_back(std::move(pr.second.file_path));
        }

        // hls直播才删除文件  [AUTO-TRANSLATED:81d2aaa5]
        // Delete file only after hls live streaming
        GET_CONFIG(uint32_t, delay, Hls::kDeleteDelaySec);
        if (!eof) {
            clearHls(lst);
        } else {
            std::shared_ptr<CleanupRegistry> registry;
            auto cleanup_path = _path_hls;
            auto ticket = registerCleanup(cleanup_path, std::move(lst), _cleanup_owner, registry);
            if (!delay || immediately) {
                tryCleanup(registry, cleanup_path, ticket);
            } else {
                _poller->doDelayTask(static_cast<uint64_t>(delay) * 1000, [registry, cleanup_path, ticket]() {
                    tryCleanup(registry, cleanup_path, ticket);
                    return 0;
                });
            }
        }
    }

    clear();
    _file = nullptr;
    _segment_files.clear();
    _init_segment_last_indexes.clear();
    if (eof) {
        _init_segment_paths.clear();
        _init_segments.clear();
    } else {
        auto &current_init_segment = getCurrentInitSegment();
        for (auto it = _init_segment_paths.begin(); it != _init_segment_paths.end();) {
            if (it->first == current_init_segment) {
                ++it;
            } else {
                it = _init_segment_paths.erase(it);
            }
        }
        for (auto it = _init_segments.begin(); it != _init_segments.end();) {
            if (it->first == current_init_segment) {
                ++it;
            } else {
                it = _init_segments.erase(it);
            }
        }
    }
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
    string segment_name, segment_path;
    {
        auto strDate = getTimeStr("%Y-%m-%d");
        auto strHour = getTimeStr("%H");
        auto strTime = getTimeStr("%M-%S");
        auto current_dir = strDate + "/" + strHour + "/";
        segment_name = current_dir + strTime + "_" + std::to_string(index) + (isFmp4() ? _fmp4_seg_ext : ".ts");
        segment_path = _path_prefix + "/" + segment_name;
        auto init_segment = isFmp4() ? getCurrentInitSegment() : string();
        if (isFmp4() && (!isLive() || isKeep()) && init_segment == _last_written_init_segment) {
            _last_written_init_segment_referenced = true;
        }
        if (isLive()) {
            // 直播
            _segment_files.emplace(index, SegmentFileInfo(segment_path, init_segment));
            if (isFmp4()) {
                // segment index 单调递增，记录最后引用即可确定该 generation 的物理回收边界。
                // Segment indexes are monotonic, so the last reference identifies the physical cleanup boundary.
                _init_segment_last_indexes[init_segment] = index;
            }
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
    auto it = _segment_files.find(index);
    if (it == _segment_files.end()) {
        return;
    }
    auto init_segment = std::move(it->second.init_segment);
    File::delete_file(it->second.file_path.data(), true);
    _segment_files.erase(it);

    auto last_it = _init_segment_last_indexes.find(init_segment);
    if (last_it != _init_segment_last_indexes.end() && last_it->second == index) {
        _init_segment_last_indexes.erase(last_it);
    }
    cleanupUnusedInitSegments();
}

void HlsMakerImp::onWriteInitSegment(const char *data, size_t len) {
    auto &init_segment = getCurrentInitSegment();
    string init_seg_path = _path_prefix + "/" + init_segment;
    auto file = makeFile(init_seg_path);
    if (file) {
        fwrite(data, len, 1, file.get());
        if (!isLive() || isKeep()) {
            cleanupPreviousUnusedInitSegment(init_segment);
            _init_segments[init_segment].assign(data, len);
            _last_written_init_segment = init_segment;
            _last_written_init_segment_referenced = false;
        } else {
            // 路径表只服务 live 非 keep 的物理文件回收；VOD/keep 无需永久保存历史路径。
            // The path map only serves physical cleanup for non-kept live output; VOD/keep need no path history.
            _init_segment_paths[init_segment] = std::move(init_seg_path);
        }
        cleanupUnusedInitSegments();
    } else {
        WarnL << "Create file failed," << init_seg_path << " " << get_uv_errmsg();
    }
}

void HlsMakerImp::cleanupPreviousUnusedInitSegment(const std::string &next_init_segment) {
    if (_last_written_init_segment.empty() || _last_written_init_segment == next_init_segment
        || _last_written_init_segment_referenced) {
        return;
    }

    // 没有媒体切片引用的 init 无需作为历史 generation 保留。
    // An init never referenced by a media segment need not be retained as a historical generation.
    auto previous_init_path = _path_prefix + "/" + _last_written_init_segment;
    File::delete_file(previous_init_path.data(), true);
    _init_segments.erase(_last_written_init_segment);
}

void HlsMakerImp::cleanupUnusedInitSegments() {
    if (!isFmp4() || !isLive() || isKeep()) {
        return;
    }

    auto &current_init_segment = getCurrentInitSegment();
    for (auto it = _init_segment_paths.begin(); it != _init_segment_paths.end();) {
        if (it->first == current_init_segment || _init_segment_last_indexes.count(it->first)) {
            ++it;
            continue;
        }
        // 与最后一个物理媒体片同步回收，避免破坏 delay/retain 窗口内的旧 playlist。
        // Reclaim with the last physical media segment so old playlists remain valid through delay/retain.
        File::delete_file(it->second.data(), true);
        _init_segments.erase(it->first);
        it = _init_segment_paths.erase(it);
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
