/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FRAME_H
#define ZLMEDIAKIT_FRAME_H

#include <mutex>
#include <functional>
#include "Util/RingBuffer.h"
#include "Network/Socket.h"
#include "Common/Stamp.h"

namespace mediakit {

typedef enum {
    TrackInvalid = -1,
    TrackVideo = 0,
    TrackAudio,
    TrackTitle,
    TrackApplication,
    TrackMax
} TrackType;

#define CODEC_MAP(XX) \
    XX(CodecH264,  TrackVideo, 0, "H264", PSI_STREAM_H264)          \
    XX(CodecH265,  TrackVideo, 1, "H265", PSI_STREAM_H265)          \
    XX(CodecAAC,   TrackAudio, 2, "mpeg4-generic", PSI_STREAM_AAC)  \
    XX(CodecG711A, TrackAudio, 3, "PCMA", PSI_STREAM_AUDIO_G711A)   \
    XX(CodecG711U, TrackAudio, 4, "PCMU", PSI_STREAM_AUDIO_G711U)   \
    XX(CodecOpus,  TrackAudio, 5, "opus", PSI_STREAM_AUDIO_OPUS)    \
    XX(CodecL16,   TrackAudio, 6, "L16", PSI_STREAM_RESERVED)       \
    XX(CodecVP8,   TrackVideo, 7, "VP8", PSI_STREAM_VP8)            \
    XX(CodecVP9,   TrackVideo, 8, "VP9", PSI_STREAM_VP9)            \
    XX(CodecAV1,   TrackVideo, 9, "AV1X", PSI_STREAM_AV1)

typedef enum {
    CodecInvalid = -1,
#define XX(name, type, value, str, mpeg_id) name = value,
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
 * 编码信息的抽象接口
 */
class CodecInfo {
public:
    typedef std::shared_ptr<CodecInfo> Ptr;

    CodecInfo() = default;
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
};

/**
 * 帧类型的抽象接口
 */
class Frame : public toolkit::Buffer, public CodecInfo {
public:
    using Ptr = std::shared_ptr<Frame>;
    virtual ~Frame() = default;

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

/**
 * 一个Frame类中可以有多个帧，他们通过 0x 00 00 01 分隔
 * ZLMediaKit会先把这种复合帧split成单个帧然后再处理
 * 一个复合帧可以通过无内存拷贝的方式切割成多个子Frame
 * 提供该类的目的是切割复合帧时防止内存拷贝，提高性能
 */
template <typename Parent>
class FrameInternal : public Parent {
public:
    typedef std::shared_ptr<FrameInternal> Ptr;
    FrameInternal(const Frame::Ptr &parent_frame, char *ptr, size_t size, size_t prefix_size)
        : Parent(ptr, size, parent_frame->dts(), parent_frame->pts(), prefix_size) {
        _parent_frame = parent_frame;
    }
    bool cacheAble() const override { return _parent_frame->cacheAble(); }

private:
    Frame::Ptr _parent_frame;
};

/**
 * 一个Frame类中可以有多个帧(AAC)，时间戳会变化
 * ZLMediaKit会先把这种复合帧split成单个帧然后再处理
 * 一个复合帧可以通过无内存拷贝的方式切割成多个子Frame
 * 提供该类的目的是切割复合帧时防止内存拷贝，提高性能
 */
template <typename Parent>
class FrameTSInternal : public Parent {
public:
    typedef std::shared_ptr<FrameTSInternal> Ptr;
    FrameTSInternal(
        const Frame::Ptr &parent_frame, char *ptr, size_t size, size_t prefix_size, uint64_t dts, uint64_t pts)
        : Parent(ptr, size, dts, pts, prefix_size) {
        _parent_frame = parent_frame;
    }
    bool cacheAble() const override { return _parent_frame->cacheAble(); }

private:
    Frame::Ptr _parent_frame;
};

/**
 * 写帧接口的抽象接口类
 */
class FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameWriterInterface> Ptr;
    FrameWriterInterface() = default;
    virtual ~FrameWriterInterface() = default;

    /**
     * 写入帧数据
     */
    virtual bool inputFrame(const Frame::Ptr &frame) = 0;
};

/**
 * 写帧接口转function，辅助类
 */
class FrameWriterInterfaceHelper : public FrameWriterInterface {
public:
    typedef std::shared_ptr<FrameWriterInterfaceHelper> Ptr;
    typedef std::function<bool(const Frame::Ptr &frame)> onWriteFrame;

    /**
     * inputFrame后触发onWriteFrame回调
     */
    FrameWriterInterfaceHelper(const onWriteFrame &cb) { _writeCallback = cb; }
    virtual ~FrameWriterInterfaceHelper() = default;

    /**
     * 写入帧数据
     */
    bool inputFrame(const Frame::Ptr &frame) override { return _writeCallback(frame); }

private:
    onWriteFrame _writeCallback;
};

/**
 * 支持代理转发的帧环形缓存
 */
class FrameDispatcher : public FrameWriterInterface {
public:
    using Ptr = std::shared_ptr<FrameDispatcher>;
    FrameDispatcher() = default;
    ~FrameDispatcher() override = default;

    /**
     * 添加代理
     */
    void addDelegate(const FrameWriterInterface::Ptr &delegate) {
        std::lock_guard<std::mutex> lck(_mtx);
        _delegates.emplace(delegate.get(), delegate);
    }

    /**
     * 删除代理
     */
    void delDelegate(FrameWriterInterface *ptr) {
        std::lock_guard<std::mutex> lck(_mtx);
        _delegates.erase(ptr);
    }

    /**
     * 写入帧并派发
     */
    bool inputFrame(const Frame::Ptr &frame) override {
        std::lock_guard<std::mutex> lck(_mtx);
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
        std::lock_guard<std::mutex> lck(_mtx);
        return _delegates.size();
    }

    void clear() {
        std::lock_guard<std::mutex> lck(_mtx);
        _delegates.clear();
    }

private:
    mutable std::mutex _mtx;
    std::map<void *, FrameWriterInterface::Ptr> _delegates;
};

/**
 * 通过Frame接口包装指针，方便使用者把自己的数据快速接入ZLMediaKit
 */
class FrameFromPtr : public Frame {
public:
    typedef std::shared_ptr<FrameFromPtr> Ptr;

    FrameFromPtr(
        CodecId codec_id, char *ptr, size_t size, uint64_t dts, uint64_t pts = 0, size_t prefix_size = 0,
        bool is_key = false)
        : FrameFromPtr(ptr, size, dts, pts, prefix_size, is_key) {
        _codec_id = codec_id;
    }

    FrameFromPtr(char *ptr, size_t size, uint64_t dts, uint64_t pts = 0, size_t prefix_size = 0, bool is_key = false) {
        _ptr = ptr;
        _size = size;
        _dts = dts;
        _pts = pts;
        _prefix_size = prefix_size;
        _is_key = is_key;
    }

    char *data() const override { return _ptr; }
    size_t size() const override { return _size; }
    uint64_t dts() const override { return _dts; }
    uint64_t pts() const override { return _pts ? _pts : dts(); }
    size_t prefixSize() const override { return _prefix_size; }
    bool cacheAble() const override { return false; }
    bool keyFrame() const override { return _is_key; }
    bool configFrame() const override { return false; }
    void setCodecId(CodecId codec_id) { _codec_id = codec_id; }

    CodecId getCodecId() const override {
        if (_codec_id == CodecInvalid) {
            throw std::invalid_argument("FrameFromPtr对象未设置codec类型");
        }
        return _codec_id;
    }

protected:
    FrameFromPtr() = default;

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
 * 该对象的功能是把一个不可缓存的帧转换成可缓存的帧
 */
class FrameCacheAble : public FrameFromPtr {
public:
    typedef std::shared_ptr<FrameCacheAble> Ptr;

    FrameCacheAble(const Frame::Ptr &frame, bool force_key_frame = false) {
        if (frame->cacheAble()) {
            _frame = frame;
            _ptr = frame->data();
        } else {
            _buffer = FrameImp::create();
            _buffer->_buffer.assign(frame->data(), frame->size());
            _ptr = _buffer->data();
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

    ~FrameCacheAble() override = default;

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
    Frame::Ptr _frame;
    FrameImp::Ptr _buffer;
};

//该类实现frame级别的时间戳覆盖
class FrameStamp : public Frame {
public:
    using Ptr = std::shared_ptr<FrameStamp>;
    FrameStamp(Frame::Ptr frame, Stamp &stamp, bool modify_stamp) {
        _frame = std::move(frame);
        //覆盖时间戳
        stamp.revise(_frame->dts(), _frame->pts(), _dts, _pts, modify_stamp);
    }
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
class FrameWrapper : public Parent {
public:
    ~FrameWrapper() = default;

    /**
     * 构造frame
     * @param buf 数据缓存
     * @param dts 解码时间戳
     * @param pts 显示时间戳
     * @param prefix 帧前缀长度
     * @param offset buffer有效数据偏移量
     */
    FrameWrapper(toolkit::Buffer::Ptr buf, uint64_t dts, uint64_t pts, size_t prefix, size_t offset)
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
    FrameWrapper(toolkit::Buffer::Ptr buf, uint64_t dts, uint64_t pts, size_t prefix, size_t offset, CodecId codec)
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
    ~FrameMerger() = default;

    void clear();
    bool inputFrame(const Frame::Ptr &frame, const onOutput &cb, toolkit::BufferLikeString *buffer = nullptr);

private:
    bool willFlush(const Frame::Ptr &frame) const;
    void doMerge(toolkit::BufferLikeString &buffer, const Frame::Ptr &frame) const;

private:
    int _type;
    bool _have_decode_able_frame = false;
    toolkit::List<Frame::Ptr> _frame_cache;
};

} // namespace mediakit
#endif // ZLMEDIAKIT_FRAME_H