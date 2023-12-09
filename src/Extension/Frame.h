/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FRAME_H
#define ZLMEDIAKIT_FRAME_H

#include <map>
#include <mutex>
#include <functional>
#include "Util/List.h"
#include "Util/TimeTicker.h"
#include "Common/Stamp.h"
#include "Network/Buffer.h"

namespace mediakit {

class Stamp;

typedef enum {
    TrackInvalid = -1,
    TrackVideo = 0,
    TrackAudio,
    TrackTitle,
    TrackApplication,
    TrackMax
} TrackType;

#define CODEC_MAP(XX) \
    XX(CodecH264,  TrackVideo, 0, "H264", PSI_STREAM_H264, MOV_OBJECT_H264)          \
    XX(CodecH265,  TrackVideo, 1, "H265", PSI_STREAM_H265, MOV_OBJECT_HEVC)          \
    XX(CodecAAC,   TrackAudio, 2, "mpeg4-generic", PSI_STREAM_AAC, MOV_OBJECT_AAC)   \
    XX(CodecG711A, TrackAudio, 3, "PCMA", PSI_STREAM_AUDIO_G711A, MOV_OBJECT_G711a)  \
    XX(CodecG711U, TrackAudio, 4, "PCMU", PSI_STREAM_AUDIO_G711U, MOV_OBJECT_G711u)  \
    XX(CodecOpus,  TrackAudio, 5, "opus", PSI_STREAM_AUDIO_OPUS, MOV_OBJECT_OPUS)    \
    XX(CodecL16,   TrackAudio, 6, "L16", PSI_STREAM_RESERVED, MOV_OBJECT_NONE)       \
    XX(CodecVP8,   TrackVideo, 7, "VP8", PSI_STREAM_VP8, MOV_OBJECT_VP8)             \
    XX(CodecVP9,   TrackVideo, 8, "VP9", PSI_STREAM_VP9, MOV_OBJECT_VP9)             \
    XX(CodecAV1,   TrackVideo, 9, "AV1", PSI_STREAM_AV1, MOV_OBJECT_AV1)             \
    XX(CodecJPEG,  TrackVideo, 10, "JPEG", PSI_STREAM_JPEG_2000, MOV_OBJECT_JPEG)

typedef enum {
    CodecInvalid = -1,
#define XX(name, type, value, str, mpeg_id, mp4_id) name = value,
    CODEC_MAP(XX)
#undef XX
    CodecMax
} CodecId;

/**
 * 字符串转媒体类型转
 */
TrackType getTrackType(const std::string &str);

/**
 * 媒体类型转字符串
 */
const char* getTrackString(TrackType type);

/**
 * 根据SDP中描述获取codec_id
 * @param str
 * @return
 */
CodecId getCodecId(const std::string &str);

/**
 * 获取编码器名称
 */
const char *getCodecName(CodecId codecId);

/**
 * 获取音视频类型
 */
TrackType getTrackType(CodecId codecId);

/**
 * 根据codecid获取mov object id
 */
int getMovIdByCodec(CodecId codecId);

/**
 * 根据mov object id获取CodecId
 */
CodecId getCodecByMovId(int object_id);

/**
 * 根据codecid获取mpeg id
 */
int getMpegIdByCodec(CodecId codec);

/**
 * 根据mpeg id获取CodecId
 */
CodecId getCodecByMpegId(int mpeg_id);

/**
 * 编码信息的抽象接口
 */
class CodecInfo {
public:
    using Ptr = std::shared_ptr<CodecInfo>;

    virtual ~CodecInfo() = default;

    /**
     * 获取编解码器类型
     */
    virtual CodecId getCodecId() const = 0;

    /**
     * 获取编码器名称
     */
    const char *getCodecName() const;

    /**
     * 获取音视频类型
     */
    TrackType getTrackType() const;

    /**
     * 获取音视频类型描述
     */
    std::string getTrackTypeStr() const;

    /**
     * 设置track index, 用于支持多track
     */
    void setIndex(int index) { _index = index; }

    /**
     * 获取track index, 用于支持多track
     */
    int getIndex() const { return _index < 0 ? (int)getTrackType() : _index; }

private:
    int _index = -1;
};

/**
 * 帧类型的抽象接口
 */
class Frame : public toolkit::Buffer, public CodecInfo {
public:
    using Ptr = std::shared_ptr<Frame>;

    /**
     * 返回解码时间戳，单位毫秒
     */
    virtual uint64_t dts() const = 0;

    /**
     * 返回显示时间戳，单位毫秒
     */
    virtual uint64_t pts() const { return dts(); }

    /**
     * 前缀长度，譬如264前缀为0x00 00 00 01,那么前缀长度就是4
     * aac前缀则为7个字节
     */
    virtual size_t prefixSize() const = 0;

    /**
     * 返回是否为关键帧
     */
    virtual bool keyFrame() const = 0;

    /**
     * 是否为配置帧，譬如sps pps vps
     */
    virtual bool configFrame() const = 0;

    /**
     * 是否可以缓存
     */
    virtual bool cacheAble() const { return true; }

    /**
     * 该帧是否可以丢弃
     * SEI/AUD帧可以丢弃
     * 默认都不能丢帧
     */
    virtual bool dropAble() const { return false; }

    /**
     * 是否为可解码帧
     * sps pps等帧不能解码
     */
    virtual bool decodeAble() const {
        if (getTrackType() != TrackVideo) {
            //非视频帧都可以解码
            return true;
        }
        //默认非sps pps帧都可以解码
        return !configFrame();
    }

    /**
     * 返回可缓存的frame
     */
    static Ptr getCacheAbleFrame(const Ptr &frame);

private:
    //对象个数统计
    toolkit::ObjectStatistic<Frame> _statistic;
};

class FrameImp : public Frame {
public:
    using Ptr = std::shared_ptr<FrameImp>;

    template <typename C = FrameImp>
    static std::shared_ptr<C> create() {
#if 0
        static ResourcePool<C> packet_pool;
        static onceToken token([]() {
            packet_pool.setSize(1024);
        });
        auto ret = packet_pool.obtain2();
        ret->_buffer.clear();
        ret->_prefix_size = 0;
        ret->_dts = 0;
        ret->_pts = 0;
        return ret;
#else
        return std::shared_ptr<C>(new C());
#endif
    }

    char *data() const override { return (char *)_buffer.data(); }
    size_t size() const override { return _buffer.size(); }
    uint64_t dts() const override { return _dts; }
    uint64_t pts() const override { return _pts ? _pts : _dts; }
    size_t prefixSize() const override { return _prefix_size; }
    CodecId getCodecId() const override { return _codec_id; }
    bool keyFrame() const override { return false; }
    bool configFrame() const override { return false; }

public:
    CodecId _codec_id = CodecInvalid;
    uint64_t _dts = 0;
    uint64_t _pts = 0;
    size_t _prefix_size = 0;
    toolkit::BufferLikeString _buffer;

private:
    //对象个数统计
    toolkit::ObjectStatistic<FrameImp> _statistic;

protected:
    friend class toolkit::ResourcePool_l<FrameImp>;
    FrameImp() = default;
};

// 包装一个指针成不可缓存的frame
class FrameFromPtr : public Frame {
public:
    using Ptr = std::shared_ptr<FrameFromPtr>;

    FrameFromPtr(CodecId codec_id, char *ptr, size_t size, uint64_t dts, uint64_t pts = 0, size_t prefix_size = 0, bool is_key = false)
        : FrameFromPtr(ptr, size, dts, pts, prefix_size, is_key) {
        _codec_id = codec_id;
    }

    char *data() const override { return _ptr; }
    size_t size() const override { return _size; }
    uint64_t dts() const override { return _dts; }
    uint64_t pts() const override { return _pts ? _pts : dts(); }
    size_t prefixSize() const override { return _prefix_size; }
    bool cacheAble() const override { return false; }
    bool keyFrame() const override { return _is_key; }
    bool configFrame() const override { return false; }

    CodecId getCodecId() const override {
        if (_codec_id == CodecInvalid) {
            throw std::invalid_argument("Invalid codec type of FrameFromPtr");
        }
        return _codec_id;
    }

protected:
    FrameFromPtr() = default;

    FrameFromPtr(char *ptr, size_t size, uint64_t dts, uint64_t pts = 0, size_t prefix_size = 0, bool is_key = false) {
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _pts = pts;
        _prefix_size = prefix_size;
        _is_key = is_key;
    }

protected:
    bool _is_key;
    char *_ptr;
    uint64_t _dts;
    uint64_t _pts = 0;
    size_t _size;
    size_t _prefix_size;
    CodecId _codec_id = CodecInvalid;
};

/**
 * 一个Frame类中可以有多个帧(AAC)，时间戳会变化
 * ZLMediaKit会先把这种复合帧split成单个帧然后再处理
 * 一个复合帧可以通过无内存拷贝的方式切割成多个子Frame
 * 提供该类的目的是切割复合帧时防止内存拷贝，提高性能
 */
template <typename Parent>
class FrameInternalBase : public Parent {
public:
    using Ptr = std::shared_ptr<FrameInternalBase>;
    FrameInternalBase(Frame::Ptr parent_frame, char *ptr, size_t size, uint64_t dts, uint64_t pts = 0, size_t prefix_size = 0)
        : Parent(parent_frame->getCodecId(), ptr, size, dts, pts, prefix_size) {
        _parent_frame = std::move(parent_frame);
        this->setIndex(_parent_frame->getIndex());
    }

    bool cacheAble() const override { return _parent_frame->cacheAble(); }

private:
    Frame::Ptr _parent_frame;
};

/**
 * 一个Frame类中可以有多个帧，他们通过 0x 00 00 01 分隔
 * ZLMediaKit会先把这种复合帧split成单个帧然后再处理
 * 一个复合帧可以通过无内存拷贝的方式切割成多个子Frame
 * 提供该类的目的是切割复合帧时防止内存拷贝，提高性能
 */
template <typename Parent>
class FrameInternal : public FrameInternalBase<Parent> {
public:
    using Ptr = std::shared_ptr<FrameInternal>;
    FrameInternal(const Frame::Ptr &parent_frame, char *ptr, size_t size, size_t prefix_size)
        : FrameInternalBase<Parent>(parent_frame, ptr, size, parent_frame->dts(), parent_frame->pts(), prefix_size) {}
};

// 管理一个指针生命周期并生产一个frame
class FrameAutoDelete : public FrameFromPtr {
public:
    template <typename... ARGS>
    FrameAutoDelete(ARGS &&...args) : FrameFromPtr(std::forward<ARGS>(args)...) {}

    ~FrameAutoDelete() override { delete[] _ptr; };

    bool cacheAble() const override { return true; }
};

// 把一个不可缓存的frame声明为可缓存的
template <typename Parent>
class FrameToCache : public Parent {
public:
    template<typename ... ARGS>
    FrameToCache(ARGS &&...args) : Parent(std::forward<ARGS>(args)...) {};

    bool cacheAble() const override {
        return true;
    }
};

// 该对象的功能是把一个不可缓存的帧转换成可缓存的帧
class FrameCacheAble : public FrameFromPtr {
public:
    using Ptr = std::shared_ptr<FrameCacheAble>;

    FrameCacheAble(const Frame::Ptr &frame, bool force_key_frame = false, toolkit::Buffer::Ptr buf = nullptr) {
        setIndex(frame->getIndex());
        if (frame->cacheAble()) {
            _ptr = frame->data();
            _buffer = frame;
        } else if (buf) {
            _ptr = frame->data();
            _buffer = std::move(buf);
        } else {
            auto buffer = std::make_shared<toolkit::BufferLikeString>();
            buffer->assign(frame->data(), frame->size());
            _ptr = buffer->data();
            _buffer = std::move(buffer);
        }
        _size = frame->size();
        _dts = frame->dts();
        _pts = frame->pts();
        _prefix_size = frame->prefixSize();
        _codec_id = frame->getCodecId();
        _key = force_key_frame ? true : frame->keyFrame();
        _config = frame->configFrame();
        _drop_able = frame->dropAble();
        _decode_able = frame->decodeAble();
    }

    /**
     * 可以被缓存
     */
    bool cacheAble() const override { return true; }
    bool keyFrame() const override { return _key; }
    bool configFrame() const override { return _config; }
    bool dropAble() const override { return _drop_able; }
    bool decodeAble() const override { return _decode_able; }

private:
    bool _key;
    bool _config;
    bool _drop_able;
    bool _decode_able;
    toolkit::Buffer::Ptr _buffer;
};

//该类实现frame级别的时间戳覆盖
class FrameStamp : public Frame {
public:
    using Ptr = std::shared_ptr<FrameStamp>;
    FrameStamp(Frame::Ptr frame, Stamp &stamp, int modify_stamp);
    ~FrameStamp() override {}

    uint64_t dts() const override { return (uint64_t)_dts; }
    uint64_t pts() const override { return (uint64_t)_pts; }
    size_t prefixSize() const override { return _frame->prefixSize(); }
    bool keyFrame() const override { return _frame->keyFrame(); }
    bool configFrame() const override { return _frame->configFrame(); }
    bool cacheAble() const override { return _frame->cacheAble(); }
    bool dropAble() const override { return _frame->dropAble(); }
    bool decodeAble() const override { return _frame->decodeAble(); }
    char *data() const override { return _frame->data(); }
    size_t size() const override { return _frame->size(); }
    CodecId getCodecId() const override { return _frame->getCodecId(); }

private:
    int64_t _dts;
    int64_t _pts;
    Frame::Ptr _frame;
};

/**
 * 该对象可以把Buffer对象转换成可缓存的Frame对象
 */
template <typename Parent>
class FrameFromBuffer : public Parent {
public:
    /**
     * 构造frame
     * @param buf 数据缓存
     * @param dts 解码时间戳
     * @param pts 显示时间戳
     * @param prefix 帧前缀长度
     * @param offset buffer有效数据偏移量
     */
    FrameFromBuffer(toolkit::Buffer::Ptr buf, uint64_t dts, uint64_t pts, size_t prefix = 0, size_t offset = 0)
        : Parent(buf->data() + offset, buf->size() - offset, dts, pts, prefix) {
        _buf = std::move(buf);
    }

    /**
     * 构造frame
     * @param buf 数据缓存
     * @param dts 解码时间戳
     * @param pts 显示时间戳
     * @param prefix 帧前缀长度
     * @param offset buffer有效数据偏移量
     * @param codec 帧类型
     */
    FrameFromBuffer(CodecId codec, toolkit::Buffer::Ptr buf, uint64_t dts, uint64_t pts, size_t prefix = 0, size_t offset = 0)
        : Parent(codec, buf->data() + offset, buf->size() - offset, dts, pts, prefix) {
        _buf = std::move(buf);
    }

    /**
     * 该帧可缓存
     */
    bool cacheAble() const override { return true; }

private:
    toolkit::Buffer::Ptr _buf;
};

/**
 * 合并一些时间戳相同的frame
 */
class FrameMerger {
public:
    using onOutput = std::function<void(uint64_t dts, uint64_t pts, const toolkit::Buffer::Ptr &buffer, bool have_key_frame)>;
    using Ptr = std::shared_ptr<FrameMerger>;
    enum {
        none = 0,
        h264_prefix,
        mp4_nal_size,
    };

    FrameMerger(int type);

    /**
     * 刷新输出缓冲，注意此时会调用FrameMerger::inputFrame传入的onOutput回调
     * 请注意回调捕获参数此时是否有效
     */
    void flush();
    void clear();
    bool inputFrame(const Frame::Ptr &frame, onOutput cb, toolkit::BufferLikeString *buffer = nullptr);

private:
    bool willFlush(const Frame::Ptr &frame) const;
    void doMerge(toolkit::BufferLikeString &buffer, const Frame::Ptr &frame) const;

private:
    int _type;
    bool _have_decode_able_frame = false;
    onOutput _cb;
    toolkit::List<Frame::Ptr> _frame_cache;
};

/**
 * 写帧接口的抽象接口类
 */
class FrameWriterInterface {
public:
    using Ptr = std::shared_ptr<FrameWriterInterface>;
    virtual ~FrameWriterInterface() = default;

    /**
     * 写入帧数据
     */
    virtual bool inputFrame(const Frame::Ptr &frame) = 0;

    /**
     * 刷新输出所有frame缓存
     */
    virtual void flush() {};
};

/**
 * 支持代理转发的帧环形缓存
 */
class FrameDispatcher : public FrameWriterInterface {
public:
    using Ptr = std::shared_ptr<FrameDispatcher>;

    /**
     * 添加代理
     */
    FrameWriterInterface* addDelegate(FrameWriterInterface::Ptr delegate) {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        return _delegates.emplace(delegate.get(), std::move(delegate)).first->second.get();
    }

    FrameWriterInterface* addDelegate(std::function<bool(const Frame::Ptr &frame)> cb);

    /**
     * 删除代理
     */
    void delDelegate(FrameWriterInterface *ptr) {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        _delegates.erase(ptr);
    }

    /**
     * 写入帧并派发
     */
    bool inputFrame(const Frame::Ptr &frame) override {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        doStatistics(frame);
        bool ret = false;
        for (auto &pr : _delegates) {
            if (pr.second->inputFrame(frame)) {
                ret = true;
            }
        }
        return ret;
    }

    /**
     * 返回代理个数
     */
    size_t size() const {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        return _delegates.size();
    }

    void clear() {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        _delegates.clear();
    }

    /**
     * 获取累计关键帧数
     */
    uint64_t getVideoKeyFrames() const {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        return _video_key_frames;
    }

    /**
     *  获取帧数
     */
    uint64_t getFrames() const {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        return _frames;
    }

    size_t getVideoGopSize() const {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        return _gop_size;
    }

    size_t getVideoGopInterval() const {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        return _gop_interval_ms;
    }

    int64_t getDuration() const {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        return _stamp.getRelativeStamp();
    }

private:
    void doStatistics(const Frame::Ptr &frame) {
        if (!frame->configFrame() && !frame->dropAble()) {
            // 忽略配置帧与可丢弃的帧
            ++_frames;
            int64_t out;
            _stamp.revise(frame->dts(), frame->pts(), out, out);
            if (frame->keyFrame() && frame->getTrackType() == TrackVideo) {
                // 遇视频关键帧时统计
                ++_video_key_frames;
                _gop_size = _frames - _last_frames;
                _gop_interval_ms = _ticker.elapsedTime();
                _last_frames = _frames;
                _ticker.resetTime();
            }
        }
    }

private:
    toolkit::Ticker _ticker;
    size_t _gop_interval_ms = 0;
    size_t _gop_size = 0;
    uint64_t _last_frames = 0;
    uint64_t _frames = 0;
    uint64_t _video_key_frames = 0;
    Stamp _stamp;
    mutable std::recursive_mutex _mtx;
    std::map<void *, FrameWriterInterface::Ptr> _delegates;
};

} // namespace mediakit
#endif // ZLMEDIAKIT_FRAME_H