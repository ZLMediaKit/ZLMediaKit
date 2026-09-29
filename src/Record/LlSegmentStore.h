/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_LLSEGMENTSTORE_H
#define ZLMEDIAKIT_LLSEGMENTSTORE_H

#include <deque>
#include <list>
#include <cstdint>
#include <initializer_list>
#include <mutex>
#include <string>
#include <memory>
#include <atomic>
#include <array>
#include <functional>
#include <unordered_map>
#include <vector>

#include "Common/MediaSource.h"
#include "Network/Buffer.h"
#include "Util/RingBuffer.h"
#include "Util/TimeTicker.h"
#include "Poller/Timer.h"

namespace mediakit {

/**
 * LL-HLS / LL-DASH 配置
 * LL-HLS / LL-DASH configuration
 */
struct LlConfig {
    // 部分分片(partial segment / CMAF chunk)时长, 单位毫秒
    // Duration of a partial segment (CMAF chunk), in milliseconds
    int part_dur_ms = 300;
    // PART-HOLD-BACK相对于PART-TARGET的倍数
    // PART-HOLD-BACK as a multiple of PART-TARGET
    float part_hold_back = 3.0f;
    // 完整分片时长, 单位毫秒
    // Duration of a full segment, in milliseconds
    int seg_dur_ms = 1000;
    // 内存中保留的完整分片个数
    // Number of full segments retained in memory
    int seg_num = 6;
    // 阻塞请求最长挂起时长, 单位毫秒
    // Maximum hold time for blocking requests, in milliseconds
    uint32_t blocking_timeout_ms = 15000;

    // 从全局配置加载
    // Load from the global configuration
    static LlConfig load();
};

/**
 * MP4MuxerMemory 产出的一个 CMAF chunk。
 * A CMAF chunk emitted by MP4MuxerMemory.
 */
class LlCmafChunk {
public:
    using Ptr = std::shared_ptr<LlCmafChunk>;

    toolkit::Buffer::Ptr data;
    uint64_t stamp = 0;
    bool key_pos = false;
};

/**
 * 部分分片，由多个 CMAF chunk 组成。
 * Partial segment made of CMAF chunks.
 */
class LlPart {
public:
    using Ptr = std::shared_ptr<LlPart>;

    // 所属完整分片的序号
    // Sequence number of the owning full segment
    uint64_t msn = 0;
    // 在本完整分片内的序号, 从0开始
    // Index inside the owning full segment, starting from 0
    int index = 0;
    // 时长, 单位毫秒
    // Duration in milliseconds
    int duration_ms = 0;
    // 是否独立可解码(以关键帧起始)
    // Whether it is independently decodable (starts with a key frame)
    bool independent = false;
    // CMAF chunk 列表；仅持有原始 Buffer，不拼接复制。
    // CMAF chunks; only retains the original buffers without concatenation.
    std::list<LlCmafChunk::Ptr> chunks;

    size_t getSize() const;
    std::list<toolkit::Buffer::Ptr> getBuffers() const;
};

/**
 * 完整分片, 以关键帧起始, 由若干个部分分片拼接而成
 * Full segment, starting with a key frame, made of several partial segments
 */
class LlSegment {
public:
    using Ptr = std::shared_ptr<LlSegment>;

    // 分片序号(media sequence number)
    // Segment sequence number (media sequence number)
    uint64_t msn = 0;
    // 首帧在媒体时间轴上的时间戳, 单位毫秒
    // Timestamp of the first frame on the media timeline, in milliseconds
    uint64_t start_stamp = 0;
    // 已累计时长, 单位毫秒
    // Accumulated duration in milliseconds
    int duration_ms = 0;
    // EXT-X-PROGRAM-DATE-TIME 取值, 未开启时为空串
    // Value of EXT-X-PROGRAM-DATE-TIME, empty when disabled
    std::string program_date_time;
    // 是否已写完
    // Whether it has been fully written
    bool completed = false;
    // 部分分片列表
    // Partial segment list
    std::list<LlPart::Ptr> parts;
    int partCount() const { return (int)parts.size(); }
    size_t getSize() const;
    std::list<toolkit::Buffer::Ptr> getBuffers() const;
};

/**
 * LL-HLS / LL-DASH 纯内存分片存储
 *
 * 线程模型:
 *  - LlHlsMaker(推流线程) 调用 beginSegment/addPart/endSegment 写入
 *  - HTTP poller 线程调用查询/读取接口
 * 所有公开接口内部加锁, 返回的 LlPart/LlSegment 均为 shared_ptr, 可跨线程安全保活
 *
 * In-memory segment store for LL-HLS / LL-DASH
 *
 * Threading model:
 *  - LlHlsMaker (publisher thread) calls beginSegment/addPart/endSegment to write
 *  - HTTP poller threads call the query/read interfaces
 * All public interfaces are internally locked; returned LlPart/LlSegment are shared_ptr
 * so they stay alive safely across threads
 */
class LlSegmentStore : public std::enable_shared_from_this<LlSegmentStore> {
public:
    using Ptr = std::shared_ptr<LlSegmentStore>;

    enum class EventType : uint8_t {
        CmafChunk,
        Part,
        Segment,
        Timeout,
    };
    static constexpr size_t kEventCount = 4;
    using EventCallback = std::function<void()>;

    class EventListener {
    public:
        using Ptr = std::shared_ptr<EventListener>;
        ~EventListener();
        void detach();

    private:
        friend class LlSegmentStore;
        EventListener(const std::weak_ptr<LlSegmentStore> &store, EventType type, uint64_t id)
            : _store(store), _type(type), _id(id) {}

    private:
        std::weak_ptr<LlSegmentStore> _store;
        EventType _type;
        uint64_t _id;
        std::atomic<bool> _attached { true };
    };

    explicit LlSegmentStore(const LlConfig &config);

    const LlConfig &getConfig() const { return _config; }

    // /////////////// 写入接口(推流线程) ///////////////
    // /////////////// Write interfaces (publisher thread) ///////////////

    /**
     * 设置fmp4 init segment(ftyp+moov), 只在首次生成时调用
     * Set the fmp4 init segment (ftyp+moov), called once when first produced
     */
    void setInitSegment(std::string data);

    void setCodecs(std::string codecs);
    void setHaveVideo(bool have_video);

    /**
     * 注册存储事件监听。持有返回对象期间监听有效，析构或detach时自动注销。
     * Register a store event listener. The listener remains active while the returned
     * object is held and is automatically removed on destruction or detach().
     */
    EventListener::Ptr addEventListener(EventType type, EventCallback cb);

    /** 获取指定事件当前版本，用于查询资源后注册监听时避免漏事件。 */
    uint64_t getEventVersion(EventType type) const { return _event_versions[(size_t)type].load(); }
    std::array<uint64_t, kEventCount> getEventVersions() const {
        std::array<uint64_t, kEventCount> versions;
        for (size_t i = 0; i < versions.size(); ++i) {
            versions[i] = _event_versions[i].load();
        }
        return versions;
    }

    /**
     * 通知收到新的 CMAF chunk。Store事件只负责通知，不保存或分发媒体数据。
    * Notify that a CMAF chunk was received. This store event only notifies
    * listeners and does not retain or distribute media data.
    */
    void notifyCmafChunk();

    /**
     * 是否包含视频轨, 用于决定分片边界是否必须落在视频关键帧上
     * Whether the stream contains a video track, used to decide whether segment
     * boundaries must fall on video key frames
     */
    bool haveVideo() const;

    /**
     * 开启一个新的完整分片
     * @param msn 分片序号
     * @param start_stamp 首帧媒体时间戳(毫秒)
     * @param wall_clock_ms 首帧对应的墙钟时间(毫秒)
     * Open a new full segment
     * @param msn Segment sequence number
     * @param start_stamp Media timestamp of the first frame (ms)
     * @param wall_clock_ms Wall clock time of the first frame (ms)
     */
    void beginSegment(uint64_t msn, uint64_t start_stamp, uint64_t wall_clock_ms);

    /**
     * 向当前正在生成的分片追加一个部分分片
     * @param index 分片内序号
     * @param duration_ms 时长(毫秒)
     * @param independent 是否独立可解码
     * @param data fmp4字节
     * Append a partial segment to the segment being built
     */
    void addPart(int index, int duration_ms, bool independent, std::list<LlCmafChunk::Ptr> chunks);

    /**
     * 结束当前正在生成的分片, 将其移入已完成队列并按segNum淘汰旧分片
     * Finish the segment being built, move it into the completed queue and evict old ones
     */
    void endSegment();

    /**
     * 丢弃当前正在生成的分片(尚未产出任何部分分片时)
     * Drop the segment being built (only when it has no partial segment yet)
     */
    void dropBuildingSegment();

    /**
     * 清空全部分片缓存(推流重置时调用)
     * Clear all cached segments (called when the publisher resets)
     */
    void clear();

    // /////////////// 读取接口(HTTP线程) ///////////////

    /**
     * 是否已经开始产出数据
     * Whether the store has started producing data
     */
    bool isStarted() const;

    /**
     * 获取指定msn的分片(已完成的分片或正在生成的分片), 不存在返回nullptr
     * Get the segment of the given msn (completed or being built), nullptr if absent
     */
    LlSegment::Ptr getSegment(uint64_t msn) const;

    /**
     * 获取指定部分分片
     * Get the given partial segment
     */
    LlPart::Ptr getPart(uint64_t msn, int index) const;

    /**
     * 获取init segment
     * Get the init segment
     */
    std::string getInitSegment() const;

    /**
     * 指定msn的分片是否已经完成(或因过旧被淘汰)
     * Whether the given msn is completed (or already evicted)
     */
    bool isMsnCompleted(uint64_t msn) const;

    /**
     * 读取某个分片在offset之后的字节, 用于边生成边流式下发
     * 返回的buffer直接引用chunk内部的内存(必要时以切片形式), 全程无内存拷贝
     * @param seg 分片对象(由getSegment返回, 用于跨线程保活)
     * @param offset 起始偏移
     * @param max_size 最多读取字节数
     * @param buffers 输出: 读到的数据
     * @param total_size 输出: 该分片当前的总字节数
     * @param completed 输出: 该分片当前是否已完成
     * Read bytes after offset of a segment, used for streaming while it is being built.
     * The returned buffers reference the chunk memory directly (as slices when needed), no copy is made.
     * @param seg Segment object (returned by getSegment, keeps it alive across threads)
     * @param offset Start offset
     * @param max_size Maximum bytes to read
     * @param buffers Output: the buffers read
     * @param total_size Output: current total byte size of the segment
     * @param completed Output: whether the segment is currently completed
     * @return 是否读取成功
     */
    bool readSegmentBuffers(const LlSegment::Ptr &seg, size_t offset, size_t max_size,
                            std::list<toolkit::Buffer::Ptr> &buffers, size_t &total_size, bool &completed) const;

    /**
     * 获取已完成分片的原始 Buffer 列表，不拼接复制。
     * Get the original buffers of a completed segment without concatenation.
     */
    bool getCompletedSegmentBuffers(uint64_t msn, std::list<toolkit::Buffer::Ptr> &buffers) const;

    /**
     * 生成LL-HLS播放列表
     * Generate the LL-HLS playlist
     */
    std::string makeHlsPlaylist() const;

    /**
     * 生成LL-DASH MPD
     * Generate the LL-DASH MPD
     */
    std::string makeDashMpd() const;

    /**
     * 解析 _HLS_msn / _HLS_part 阻塞重载参数
     * Parse the _HLS_msn / _HLS_part blocking reload parameters
     * @param url_args url参数字符串
     * @param msn 输出: msn, 未指定时为-1
     * @param part 输出: part, 未指定时为-1
     * @return 是否指定了_HLS_msn
     */
    static bool parseBlockingParams(const std::string &url_args, int64_t &msn, int &part);

    /**
     * 阻塞重载条件是否已满足
     * Whether the blocking reload condition is satisfied
     */
    bool checkBlockingCondition(int64_t msn, int part) const;

private:
    void evictIfNeed();
    void emitEvent(EventType type);
    void removeEventListener(EventType type, uint64_t id);
    void startTimeoutTimer();

private:
    LlConfig _config;
    // HLS PART-TARGET必须覆盖本流实际出现的最长Part时长。
    // PART-TARGET must cover the longest Part duration observed in this stream.
    int _max_part_duration_ms = 0;
    mutable std::mutex _mtx;
    std::mutex _event_mtx;
    std::array<std::unordered_map<uint64_t, EventCallback>, kEventCount> _event_listeners;
    std::atomic<uint64_t> _next_event_listener_id { 0 };
    std::array<std::atomic<uint64_t>, kEventCount> _event_versions {};
    toolkit::Timer::Ptr _timeout_timer;
    std::string _init_segment;
    // 每次流会话生成新的媒体URL查询参数，防止重启后复用MSN时命中浏览器的旧缓存。
    // A per-stream-session query parameter prevents browser cache reuse when MSN values restart.
    std::string _cache_buster;
    bool _have_video = true;
    std::string _codecs;
    // 已完成分片队列, 按msn递增
    // Queue of completed segments, ordered by increasing msn
    std::deque<LlSegment::Ptr> _segments;
    // 正在生成的分片
    // The segment being built
    LlSegment::Ptr _building;
    // 已被淘汰分片的累计时长(毫秒)。用于dash MPD的presentationTimeOffset与媒体时间轴末尾计算,
    // 保持startNumber随窗口滑动时媒体时间轴连续
    // Accumulated duration(ms) of the evicted segments. Used as the dash MPD presentationTimeOffset
    // and in the media timeline end calculation, keeping the media timeline continuous as the
    // startNumber slides with the window
    uint64_t _evicted_duration_ms = 0;
    // dash媒体时间轴原点对应的墙钟时间(毫秒), 即 availabilityStartTime
    // Wall clock time(ms) mapping to the origin of the dash media timeline, i.e. availabilityStartTime
    mutable uint64_t _ast_wall_ms = 0;
    // 上次发布mpd的墙钟时间(毫秒), 用于给availabilityStartTime修正限速
    // Wall clock time(ms) of the last mpd publish, used to rate limit the AST correction
    mutable uint64_t _dash_last_emit_ms = 0;
};

/**
 * 承载 LlSegmentStore 的媒体源, 用于按 app/stream 查找以及观看人数统计
 * Media source holding the LlSegmentStore, used to look up by app/stream and count readers
 */
class LlMediaSource : public MediaSource {
public:
    using Ptr = std::shared_ptr<LlMediaSource>;
    using RingType = toolkit::RingBuffer<LlCmafChunk::Ptr>;

    LlMediaSource(const MediaTuple &tuple, const LlSegmentStore::Ptr &store);

    int readerCount() override { return _ring ? _ring->readerCount() : 0; }

    const RingType::Ptr &getRing() const { return _ring; }

    LlSegmentStore::Ptr getStore() const { return _store; }

    /** 写入一个 CMAF chunk；RingBuffer仅用于播放器统计与detach通知。 */
    void inputCmafChunk(const LlCmafChunk::Ptr &chunk);

    /**
     * 首次产出数据时调用, 创建环形缓冲并完成MediaSource注册(幂等)
     * Called when the first data is produced, creates the ring buffer and registers
     * the MediaSource (idempotent)
     */
    void registRing();

    void getPlayerList(const std::function<void(const std::list<toolkit::Any> &info_list)> &cb,
                       const std::function<toolkit::Any(toolkit::Any &&info)> &on_change) override {
        if (_ring) {
            _ring->getInfoList(cb, on_change);
        } else {
            cb(std::list<toolkit::Any>());
        }
    }

private:
    LlSegmentStore::Ptr _store;
    RingType::Ptr _ring;
};

/**
 * 一个 LL-CMAF 播放会话。RingReader仅用于播放器统计和detach通知；同一player_id
 * 下的HTTP请求通过LlSegmentStore事件等待对应资源生成。
 */
class LlCmafPlayer : public std::enable_shared_from_this<LlCmafPlayer> {
public:
    using Ptr = std::shared_ptr<LlCmafPlayer>;

    explicit LlCmafPlayer(const LlMediaSource::Ptr &source) : _source(source) {}
    ~LlCmafPlayer();

    LlMediaSource::Ptr getSource() const { return _source.lock(); }

    void setId(std::string id) { _id = std::move(id); }
    const std::string &getId() const { return _id; }

    void setSession(const std::shared_ptr<toolkit::Session> &session);
    void setMediaInfo(MediaInfo info) { _info = std::move(info); }
    void addByteUsage(size_t bytes);
    void attach(const toolkit::EventPoller::Ptr &poller);

    /**
    * 等待指定存储事件或超时。回调始终切回调用HTTP请求所属的poller。
     */
    void waitForEvent(LlSegmentStore::EventType type, uint64_t event_version, const toolkit::EventPoller::Ptr &poller,
                      uint64_t timeout_ms, std::function<void()> cb);
    void waitForEvents(const std::array<uint64_t, LlSegmentStore::kEventCount> &event_versions,
                       std::initializer_list<LlSegmentStore::EventType> types, const toolkit::EventPoller::Ptr &poller,
                       uint64_t timeout_ms, std::function<void()> cb);

private:
    struct Waiter {
        toolkit::EventPoller::Ptr poller;
        uint8_t event_mask;
        std::array<uint64_t, LlSegmentStore::kEventCount> event_versions;
        std::shared_ptr<toolkit::Ticker> ticker;
        uint64_t timeout_ms;
        std::function<void()> cb;
    };

    void onEvent(LlSegmentStore::EventType type);
    void onDetach();
    void notifyWaiter(uint64_t id);

private:
    std::weak_ptr<LlMediaSource> _source;
    std::string _id;
    std::atomic<uint64_t> _bytes { 0 };
    MediaInfo _info;
    toolkit::Ticker _ticker;
    std::shared_ptr<toolkit::Session> _sock_info;
    std::mutex _reader_mtx;
    LlMediaSource::RingType::RingReader::Ptr _reader;
    std::array<LlSegmentStore::EventListener::Ptr, LlSegmentStore::kEventCount> _event_listeners;
    std::mutex _waiter_mtx;
    std::unordered_map<uint64_t, Waiter> _waiters;
    std::atomic<uint64_t> _next_waiter_id { 0 };
};

} // namespace mediakit
#endif // ZLMEDIAKIT_LLSEGMENTSTORE_H
