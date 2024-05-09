/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UTIL_RINGBUFFER_H_
#define UTIL_RINGBUFFER_H_

#include <assert.h>
#include <atomic>
#include <memory>
#include <mutex>
#include <unordered_map>
#include <condition_variable>
#include <functional>
#include "util.h"
#include "List.h"
#include "Poller/EventPoller.h"

// GOP缓存最大长度下限值
#define RING_MIN_SIZE 32
#define LOCK_GUARD(mtx) std::lock_guard<decltype(mtx)> lck(mtx)

namespace toolkit {

template <typename T>
class RingDelegate {
public:
    using Ptr = std::shared_ptr<RingDelegate>;
    RingDelegate() = default;
    virtual ~RingDelegate() = default;
    virtual void onWrite(T in, bool is_key = true) = 0;
};

template <typename T>
class _RingStorage;

template <typename T>
class _RingReaderDispatcher;

/**
 * 环形缓存读取器
 * 该对象的事件触发都会在绑定的poller线程中执行
 * 所以把锁去掉了
 * 对该对象的一切操作都应该在poller线程中执行
 */
template <typename T>
class _RingReader {
public:
    using Ptr = std::shared_ptr<_RingReader>;
    friend class _RingReaderDispatcher<T>;

    _RingReader(std::shared_ptr<_RingStorage<T>> storage) {
        _storage = std::move(storage);
        setReadCB(nullptr);
        setDetachCB(nullptr);
        setGetInfoCB(nullptr);
        setMessageCB(nullptr);
    }

    ~_RingReader() = default;

    void setReadCB(std::function<void(const T &)> cb) {
        if (!cb) {
            _read_cb = [](const T &) {};
        } else {
            _read_cb = std::move(cb);
            flushGop();
        }
    }

    void setDetachCB(std::function<void()> cb) {
        _detach_cb = cb ? std::move(cb) : []() {};
    }

    void setGetInfoCB(std::function<Any()> cb) {
        _info_cb = cb ? std::move(cb) : []() { return Any(); };
    }

    void setMessageCB(std::function<void(const Any &data)> cb) {
        _msg_cb = cb ? std::move(cb) : [](const Any &data) {};
    }

private:
    void onRead(const T &data, bool /*is_key*/) { _read_cb(data); }
    void onMessage(const Any &data) { _msg_cb(data); }
    void onDetach() const { _detach_cb(); }
    Any getInfo() { return _info_cb(); }

    void flushGop() {
        if (!_storage) {
            return;
        }
        _storage->getCache().for_each([this](const List<std::pair<bool, T>> &lst) {
            lst.for_each([this](const std::pair<bool, T> &pr) { onRead(pr.second, pr.first); });
        });
    }


private:
    std::shared_ptr<_RingStorage<T>> _storage;
    std::function<void(void)> _detach_cb;
    std::function<void(const T &)> _read_cb;
    std::function<Any()> _info_cb;
    std::function<void(const Any &data)> _msg_cb;
};

template <typename T>
class _RingStorage {
public:
    using Ptr = std::shared_ptr<_RingStorage>;
    using GopType = List<List<std::pair<bool, T>>>;
    _RingStorage(size_t max_size, size_t max_gop_size) {
        // gop缓存个数不能小于32
        if (max_size < RING_MIN_SIZE) {
            max_size = RING_MIN_SIZE;
        }
        _max_size = max_size;
        _max_gop_size = max_gop_size;
        clearCache();
    }

    ~_RingStorage() = default;

    /**
     * 写入环形缓存数据
     * @param in 数据
     * @param is_key 是否为关键帧
     * @return 是否触发重置环形缓存大小
     */
    void write(T in, bool is_key = true) {
        if (is_key) {
            _have_idr = true;
            _started = true;
            if (!_data_cache.back().empty()) {
                //当前gop列队还没收到任意缓存
                _data_cache.emplace_back();
            }
            if (_data_cache.size() > _max_gop_size) {
                // GOP个数超过限制，那么移除最早的GOP
                popFrontGop();
            }
        }

        if (!_have_idr && _started) {
            //缓存中没有关键帧，那么gop缓存无效
            return;
        }
        _data_cache.back().emplace_back(std::make_pair(is_key, std::move(in)));
        if (++_size > _max_size) {
            // GOP缓存溢出
            while (_data_cache.size() > 1) {
                //先尝试清除老的GOP缓存
                popFrontGop();
            }
            if (_size > _max_size) {
                //还是大于最大缓冲限制，那么清空所有GOP
                clearCache();
            }
        }
    }

    Ptr clone() const {
        Ptr ret(new _RingStorage());
        ret->_size = _size;
        ret->_have_idr = _have_idr;
        ret->_started = _started;
        ret->_max_size = _max_size;
        ret->_max_gop_size = _max_gop_size;
        ret->_data_cache = _data_cache;
        return ret;
    }

    const GopType &getCache() const { return _data_cache; }

    void clearCache() {
        _size = 0;
        _have_idr = false;
        _data_cache.clear();
        _data_cache.emplace_back();
    }

private:
    _RingStorage() = default;

    void popFrontGop() {
        if (!_data_cache.empty()) {
            _size -= _data_cache.front().size();
            _data_cache.pop_front();
            if (_data_cache.empty()) {
                _data_cache.emplace_back();
            }
        }
    }

private:
    bool _started = false;
    bool _have_idr;
    size_t _size;
    size_t _max_size;
    size_t _max_gop_size;
    GopType _data_cache;
};

template <typename T>
class RingBuffer;

/**
 * 环形缓存事件派发器，只能一个poller线程操作它
 * @tparam T
 */
template <typename T>
class _RingReaderDispatcher : public std::enable_shared_from_this<_RingReaderDispatcher<T>> {
public:
    using Ptr = std::shared_ptr<_RingReaderDispatcher>;
    using RingReader = _RingReader<T>;
    using RingStorage = _RingStorage<T>;
    using onChangeInfoCB = std::function<Any(Any &&info)>;

    friend class RingBuffer<T>;

    ~_RingReaderDispatcher() {
        decltype(_reader_map) reader_map;
        reader_map.swap(_reader_map);
        for (auto &pr : reader_map) {
            auto reader = pr.second.lock();
            if (reader) {
                reader->onDetach();
            }
        }
    }

private:
    _RingReaderDispatcher(
        const typename RingStorage::Ptr &storage, std::function<void(int, bool)> onSizeChanged) {
        _reader_size = 0;
        _storage = storage;
        _on_size_changed = std::move(onSizeChanged);
        assert(_on_size_changed);
    }

    void write(T in, bool is_key = true) {
        for (auto it = _reader_map.begin(); it != _reader_map.end();) {
            auto reader = it->second.lock();
            if (!reader) {
                it = _reader_map.erase(it);
                --_reader_size;
                onSizeChanged(false);
                continue;
            }
            reader->onRead(in, is_key);
            ++it;
        }
        _storage->write(std::move(in), is_key);
    }

    void sendMessage(const Any &data) {
        for (auto it = _reader_map.begin(); it != _reader_map.end();) {
            auto reader = it->second.lock();
            if (!reader) {
                it = _reader_map.erase(it);
                --_reader_size;
                onSizeChanged(false);
                continue;
            }
            reader->onMessage(data);
            ++it;
        }
    }

    std::shared_ptr<RingReader> attach(const EventPoller::Ptr &poller, bool use_cache) {
        if (!poller->isCurrentThread()) {
            throw std::runtime_error("You can attach RingBuffer only in it's poller thread");
        }

        std::weak_ptr<_RingReaderDispatcher> weak_self = this->shared_from_this();
        auto on_dealloc = [weak_self, poller](RingReader *ptr) {
            poller->async([weak_self, ptr]() {
                auto strong_self = weak_self.lock();
                if (strong_self && strong_self->_reader_map.erase(ptr)) {
                    --strong_self->_reader_size;
                    strong_self->onSizeChanged(false);
                }
                delete ptr;
            });
        };

        std::shared_ptr<RingReader> reader(new RingReader(use_cache ? _storage : nullptr), on_dealloc);
        _reader_map[reader.get()] = reader;
        ++_reader_size;
        onSizeChanged(true);
        return reader;
    }

    void onSizeChanged(bool add_flag) { _on_size_changed(_reader_size, add_flag); }

    void clearCache() {
        if (_reader_size == 0) {
            _storage->clearCache();
        }
    }

    std::list<Any> getInfoList(const onChangeInfoCB &on_change) {
        std::list<Any> ret;
        for (auto &pr : _reader_map) {
            auto reader = pr.second.lock();
            if (!reader) {
                continue;
            }
            auto info = reader->getInfo();
            if (!info) {
                continue;
            }
            ret.emplace_back(on_change(std::move(info)));
        }
        return ret;
    }

private:
    std::atomic_int _reader_size;
    std::function<void(int, bool)> _on_size_changed;
    typename RingStorage::Ptr _storage;
    std::unordered_map<void *, std::weak_ptr<RingReader>> _reader_map;
};

template <typename T>
class RingBuffer : public std::enable_shared_from_this<RingBuffer<T>> {
public:
    using Ptr = std::shared_ptr<RingBuffer>;
    using RingReader = _RingReader<T>;
    using RingStorage = _RingStorage<T>;
    using RingReaderDispatcher = _RingReaderDispatcher<T>;
    using onReaderChanged = std::function<void(int size)>;
    using onGetInfoCB = std::function<void(std::list<Any> &info_list)>;

    RingBuffer(size_t max_size = 1024, onReaderChanged cb = nullptr, size_t max_gop_size = 1) {
        _storage = std::make_shared<RingStorage>(max_size, max_gop_size);
        _on_reader_changed = cb ? std::move(cb) : [](int size) {};
        //先触发无人观看
        _on_reader_changed(0);
    }

    ~RingBuffer() = default;

    void write(T in, bool is_key = true) {
        if (_delegate) {
            _delegate->onWrite(std::move(in), is_key);
            return;
        }

        LOCK_GUARD(_mtx_map);
        for (auto &pr : _dispatcher_map) {
            auto &second = pr.second;
            //切换线程后触发onRead事件
            pr.first->async([second, in, is_key]() mutable { second->write(std::move(in), is_key); }, false);
        }
        _storage->write(std::move(in), is_key);
    }

    void sendMessage(const Any &data) {
        LOCK_GUARD(_mtx_map);
        for (auto &pr : _dispatcher_map) {
            auto &second = pr.second;
            // 切换线程后触发sendMessage
            pr.first->async([second, data]() { second->sendMessage(data); }, false);
        }
    }

    void setDelegate(const typename RingDelegate<T>::Ptr &delegate) { _delegate = delegate; }

    std::shared_ptr<RingReader> attach(const EventPoller::Ptr &poller, bool use_cache = true) {
        typename RingReaderDispatcher::Ptr dispatcher;
        {
            LOCK_GUARD(_mtx_map);
            auto &ref = _dispatcher_map[poller];
            if (!ref) {
                std::weak_ptr<RingBuffer> weak_self = this->shared_from_this();
                auto onSizeChanged = [weak_self, poller](int size, bool add_flag) {
                    if (auto strong_self = weak_self.lock()) {
                        strong_self->onSizeChanged(poller, size, add_flag);
                    }
                };
                auto onDealloc = [poller](RingReaderDispatcher *ptr) { poller->async([ptr]() { delete ptr; }); };
                ref.reset(new RingReaderDispatcher(_storage->clone(), std::move(onSizeChanged)), std::move(onDealloc));
            }
            dispatcher = ref;
        }

        return dispatcher->attach(poller, use_cache);
    }

    int readerCount() { return _total_count; }

    void clearCache() {
        LOCK_GUARD(_mtx_map);
        _storage->clearCache();
        for (auto &pr : _dispatcher_map) {
            auto &second = pr.second;
            //切换线程后清空缓存
            pr.first->async([second]() { second->clearCache(); }, false);
        }
    }

    void getInfoList(const onGetInfoCB &cb, const typename RingReaderDispatcher::onChangeInfoCB &on_change = nullptr) {
        if (!cb) {
            return;
        }
        if (!on_change) {
            const_cast<typename RingReaderDispatcher::onChangeInfoCB &>(on_change) = [](Any &&info) { return std::move(info); };
        }

        LOCK_GUARD(_mtx_map);

        auto info_vec = std::make_shared<std::vector<std::list<Any>>>();
        // 1、最少确保一个元素
        info_vec->resize(_dispatcher_map.empty() ? 1 : _dispatcher_map.size());
        std::shared_ptr<void> on_finished(nullptr, [cb, info_vec](void *) mutable {
            // 2、防止这里为空
            auto &lst = *info_vec->begin();
            for (auto &item : *info_vec) {
                if (&lst != &item) {
                    lst.insert(lst.end(), item.begin(), item.end());
                }
            }
            cb(lst);
        });

        auto i = 0U;
        for (auto &pr : _dispatcher_map) {
            auto &second = pr.second;
            pr.first->async([second, info_vec, on_finished, i, on_change]() { (*info_vec)[i] = second->getInfoList(on_change); });
            ++i;
        }
    }

private:
    void onSizeChanged(const EventPoller::Ptr &poller, int size, bool add_flag) {
        if (size == 0) {
            LOCK_GUARD(_mtx_map);
            _dispatcher_map.erase(poller);
        }

        if (add_flag) {
            ++_total_count;
        } else {
            --_total_count;
        }
        _on_reader_changed(_total_count);
    }

private:
    struct HashOfPtr {
        std::size_t operator()(const EventPoller::Ptr &key) const { return (std::size_t)key.get(); }
    };

private:
    std::mutex _mtx_map;
    std::atomic_int _total_count { 0 };
    typename RingStorage::Ptr _storage;
    typename RingDelegate<T>::Ptr _delegate;
    onReaderChanged _on_reader_changed;
    std::unordered_map<EventPoller::Ptr, typename RingReaderDispatcher::Ptr, HashOfPtr> _dispatcher_map;
};

} /* namespace toolkit */
#endif /* UTIL_RINGBUFFER_H_ */
