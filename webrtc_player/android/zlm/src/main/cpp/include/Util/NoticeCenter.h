/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_UTIL_NOTICECENTER_H_
#define SRC_UTIL_NOTICECENTER_H_

#include <mutex>
#include <memory>
#include <string>
#include <exception>
#include <functional>
#include <unordered_map>
#include <stdexcept>
#include "util.h"
#include "function_traits.h"

namespace toolkit {

class EventDispatcher {
public:
    friend class NoticeCenter;
    using Ptr = std::shared_ptr<EventDispatcher>;

    ~EventDispatcher() = default;

private:
    using MapType = std::unordered_multimap<void *, Any>;

    EventDispatcher() = default;

    class InterruptException : public std::runtime_error {
    public:
        InterruptException() : std::runtime_error("InterruptException") {}
        ~InterruptException() {}
    };

    template <typename... ArgsType>
    int emitEvent(bool safe, ArgsType &&...args) {
        using stl_func = std::function<void(decltype(std::forward<ArgsType>(args))...)>;
        decltype(_mapListener) copy;
        {
            // 先拷贝(开销比较小)，目的是防止在触发回调时还是上锁状态从而导致交叉互锁
            std::lock_guard<std::recursive_mutex> lck(_mtxListener);
            copy = _mapListener;
        }

        int ret = 0;
        for (auto &pr : copy) {
            try {
                pr.second.get<stl_func>(safe)(std::forward<ArgsType>(args)...);
                ++ret;
            } catch (InterruptException &) {
                ++ret;
                break;
            }
        }
        return ret;
    }

    template <typename FUNC>
    void addListener(void *tag, FUNC &&func) {
        using stl_func = typename function_traits<typename std::remove_reference<FUNC>::type>::stl_function_type;
        Any listener;
        listener.set<stl_func>(std::forward<FUNC>(func));
        std::lock_guard<std::recursive_mutex> lck(_mtxListener);
        _mapListener.emplace(tag, listener);
    }

    void delListener(void *tag, bool &empty) {
        std::lock_guard<std::recursive_mutex> lck(_mtxListener);
        _mapListener.erase(tag);
        empty = _mapListener.empty();
    }

private:
    std::recursive_mutex _mtxListener;
    MapType _mapListener;
};

class NoticeCenter : public std::enable_shared_from_this<NoticeCenter> {
public:
    using Ptr = std::shared_ptr<NoticeCenter>;

    static NoticeCenter &Instance();

    template <typename... ArgsType>
    int emitEvent(const std::string &event, ArgsType &&...args) {
        return emitEvent_l(false, event, std::forward<ArgsType>(args)...);
    }

    template <typename... ArgsType>
    int emitEventSafe(const std::string &event, ArgsType &&...args) {
        return emitEvent_l(true, event, std::forward<ArgsType>(args)...);
    }

    template <typename FUNC>
    void addListener(void *tag, const std::string &event, FUNC &&func) {
        getDispatcher(event, true)->addListener(tag, std::forward<FUNC>(func));
    }

    void delListener(void *tag, const std::string &event) {
        auto dispatcher = getDispatcher(event);
        if (!dispatcher) {
            // 不存在该事件
            return;
        }
        bool empty;
        dispatcher->delListener(tag, empty);
        if (empty) {
            delDispatcher(event, dispatcher);
        }
    }

    // 这个方法性能比较差
    void delListener(void *tag) {
        std::lock_guard<std::recursive_mutex> lck(_mtxListener);
        bool empty;
        for (auto it = _mapListener.begin(); it != _mapListener.end();) {
            it->second->delListener(tag, empty);
            if (empty) {
                it = _mapListener.erase(it);
                continue;
            }
            ++it;
        }
    }

    void clearAll() {
        std::lock_guard<std::recursive_mutex> lck(_mtxListener);
        _mapListener.clear();
    }

private:
    template <typename... ArgsType>
    int emitEvent_l(bool safe, const std::string &event, ArgsType &&...args) {
        auto dispatcher = getDispatcher(event);
        if (!dispatcher) {
            // 该事件无人监听
            return 0;
        }
        return dispatcher->emitEvent(safe, std::forward<ArgsType>(args)...);
    }

    EventDispatcher::Ptr getDispatcher(const std::string &event, bool create = false) {
        std::lock_guard<std::recursive_mutex> lck(_mtxListener);
        auto it = _mapListener.find(event);
        if (it != _mapListener.end()) {
            return it->second;
        }
        if (create) {
            // 如果为空则创建一个
            EventDispatcher::Ptr dispatcher(new EventDispatcher());
            _mapListener.emplace(event, dispatcher);
            return dispatcher;
        }
        return nullptr;
    }

    void delDispatcher(const std::string &event, const EventDispatcher::Ptr &dispatcher) {
        std::lock_guard<std::recursive_mutex> lck(_mtxListener);
        auto it = _mapListener.find(event);
        if (it != _mapListener.end() && dispatcher == it->second) {
            // 两者相同则删除
            _mapListener.erase(it);
        }
    }

private:
    std::recursive_mutex _mtxListener;
    std::unordered_map<std::string, EventDispatcher::Ptr> _mapListener;
};

template <typename T>
struct NoticeHelper;

template <typename RET, typename... Args>
struct NoticeHelper<RET(Args...)> {
public:
    template <typename... ArgsType>
    static int emit(const std::string &event, ArgsType &&...args) {
        return emit(NoticeCenter::Instance(), event, std::forward<ArgsType>(args)...);
    }

    template <typename... ArgsType>
    static int emit(NoticeCenter &center, const std::string &event, ArgsType &&...args) {
        return center.emitEventSafe(event, std::forward<Args>(args)...);
    }
};

#define NOTICE_EMIT(types, ...) NoticeHelper<void(types)>::emit(__VA_ARGS__)

} /* namespace toolkit */
#endif /* SRC_UTIL_NOTICECENTER_H_ */
