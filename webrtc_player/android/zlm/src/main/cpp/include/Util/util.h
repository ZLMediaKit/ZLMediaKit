/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UTIL_UTIL_H_
#define UTIL_UTIL_H_

#include <ctime>
#include <cstdio>
#include <cstring>
#include <memory>
#include <string>
#include <sstream>
#include <vector>
#include <atomic>
#include <unordered_map>
#include "function_traits.h"
#if defined(_WIN32)
#undef FD_SETSIZE
//修改默认64为1024路
#define FD_SETSIZE 1024
#include <winsock2.h>
#pragma comment (lib,"WS2_32")
#else
#include <unistd.h>
#include <sys/time.h>
#include <sys/types.h>
#include <cstddef>
#endif // defined(_WIN32)

#if defined(__APPLE__)
#include "TargetConditionals.h"
#if TARGET_IPHONE_SIMULATOR
#define OS_IPHONE
#elif TARGET_OS_IPHONE
#define OS_IPHONE
#endif
#endif //__APPLE__

#define INSTANCE_IMP(class_name, ...) \
class_name &class_name::Instance() { \
    static std::shared_ptr<class_name> s_instance(new class_name(__VA_ARGS__)); \
    static class_name &s_instance_ref = *s_instance; \
    return s_instance_ref; \
}

namespace toolkit {

#define StrPrinter ::toolkit::_StrPrinter()
class _StrPrinter : public std::string {
public:
    _StrPrinter() {}

    template<typename T>
    _StrPrinter& operator <<(T && data) {
        _stream << std::forward<T>(data);
        this->std::string::operator=(_stream.str());
        return *this;
    }

    std::string operator <<(std::ostream&(*f)(std::ostream&)) const {
        return *this;
    }

private:
    std::stringstream _stream;
};

//禁止拷贝基类
class noncopyable {
protected:
    noncopyable() {}
    ~noncopyable() {}
private:
    //禁止拷贝
    noncopyable(const noncopyable &that) = delete;
    noncopyable(noncopyable &&that) = delete;
    noncopyable &operator=(const noncopyable &that) = delete;
    noncopyable &operator=(noncopyable &&that) = delete;
};

#ifndef CLASS_FUNC_TRAITS
#define CLASS_FUNC_TRAITS(func_name) \
template<typename T, typename ... ARGS> \
constexpr bool Has_##func_name(decltype(&T::on##func_name) /*unused*/) { \
    using RET = typename function_traits<decltype(&T::on##func_name)>::return_type; \
    using FuncType = RET (T::*)(ARGS...);   \
    return std::is_same<decltype(&T::on ## func_name), FuncType>::value; \
} \
\
template<class T, typename ... ARGS> \
constexpr bool Has_##func_name(...) { \
    return false; \
} \
\
template<typename T, typename ... ARGS> \
static void InvokeFunc_##func_name(typename std::enable_if<!Has_##func_name<T, ARGS...>(nullptr), T>::type &obj, ARGS ...args) {} \
\
template<typename T, typename ... ARGS>\
static typename function_traits<decltype(&T::on##func_name)>::return_type InvokeFunc_##func_name(typename std::enable_if<Has_##func_name<T, ARGS...>(nullptr), T>::type &obj, ARGS ...args) {\
    return obj.on##func_name(std::forward<ARGS>(args)...);\
}
#endif //CLASS_FUNC_TRAITS

#ifndef CLASS_FUNC_INVOKE
#define CLASS_FUNC_INVOKE(T, obj, func_name, ...) InvokeFunc_##func_name<T>(obj, ##__VA_ARGS__)
#endif //CLASS_FUNC_INVOKE

CLASS_FUNC_TRAITS(Destory)
CLASS_FUNC_TRAITS(Create)

/**
 * 对象安全的构建和析构,构建后执行onCreate函数,析构前执行onDestory函数
 * 在函数onCreate和onDestory中可以执行构造或析构中不能调用的方法，比如说shared_from_this或者虚函数
 * @warning onDestory函数确保参数个数为0；否则会被忽略调用
 */
class Creator {
public:
    /**
     * 创建对象，用空参数执行onCreate和onDestory函数
     * @param args 对象构造函数参数列表
     * @return args对象的智能指针
     */
    template<typename C, typename ...ArgsType>
    static std::shared_ptr<C> create(ArgsType &&...args) {
        std::shared_ptr<C> ret(new C(std::forward<ArgsType>(args)...), [](C *ptr) {
            try {
                CLASS_FUNC_INVOKE(C, *ptr, Destory);
            } catch (std::exception &ex){
                onDestoryException(typeid(C), ex);
            }
            delete ptr;
        });
        CLASS_FUNC_INVOKE(C, *ret, Create);
        return ret;
    }

    /**
     * 创建对象，用指定参数执行onCreate函数
     * @param args 对象onCreate函数参数列表
     * @warning args参数类型和个数必须与onCreate函数类型匹配(不可忽略默认参数)，否则会由于模板匹配失败导致忽略调用
     * @return args对象的智能指针
     */
    template<typename C, typename ...ArgsType>
    static std::shared_ptr<C> create2(ArgsType &&...args) {
        std::shared_ptr<C> ret(new C(), [](C *ptr) {
            try {
                CLASS_FUNC_INVOKE(C, *ptr, Destory);
            } catch (std::exception &ex){
                onDestoryException(typeid(C), ex);
            }
            delete ptr;
        });
        CLASS_FUNC_INVOKE(C, *ret, Create, std::forward<ArgsType>(args)...);
        return ret;
    }

private:
    static void onDestoryException(const std::type_info &info, const std::exception &ex);

private:
    Creator() = default;
    ~Creator() = default;
};

template <class C>
class ObjectStatistic{
public:
    ObjectStatistic(){
        ++getCounter();
    }

    ~ObjectStatistic(){
        --getCounter();
    }

    static size_t count(){
        return getCounter().load();
    }

private:
    static std::atomic<size_t> & getCounter();
};

#define StatisticImp(Type)  \
    template<> \
    std::atomic<size_t>& ObjectStatistic<Type>::getCounter(){ \
        static std::atomic<size_t> instance(0); \
        return instance; \
    }

std::string makeRandStr(int sz, bool printable = true);
std::string hexdump(const void *buf, size_t len);
std::string hexmem(const void* buf, size_t len);
std::string exePath(bool isExe = true);
std::string exeDir(bool isExe = true);
std::string exeName(bool isExe = true);

std::vector<std::string> split(const std::string& s, const char *delim);
//去除前后的空格、回车符、制表符...
std::string& trim(std::string &s,const std::string &chars=" \r\n\t");
std::string trim(std::string &&s,const std::string &chars=" \r\n\t");
// string转小写
std::string &strToLower(std::string &str);
std::string strToLower(std::string &&str);
// string转大写
std::string &strToUpper(std::string &str);
std::string strToUpper(std::string &&str);
//替换子字符串
void replace(std::string &str, const std::string &old_str, const std::string &new_str, std::string::size_type b_pos = 0) ;
//判断是否为ip
bool isIP(const char *str);
//字符串是否以xx开头
bool start_with(const std::string &str, const std::string &substr);
//字符串是否以xx结尾
bool end_with(const std::string &str, const std::string &substr);
//拼接格式字符串
template<typename... Args>
std::string str_format(const std::string &format, Args... args) {

    // Calculate the buffer size
    auto size_buf = snprintf(nullptr, 0, format.c_str(), args ...) + 1;
    // Allocate the buffer
#if __cplusplus >= 201703L
    // C++17
    auto buf = std::make_unique<char[]>(size_buf);
#else
    // C++11
    std:: unique_ptr<char[]> buf(new(std::nothrow) char[size_buf]);
#endif
    // Check if the allocation is successful
    if (buf == nullptr) {
        return {};
    }
    // Fill the buffer with formatted string
    auto result = snprintf(buf.get(), size_buf, format.c_str(), args ...);
    // Return the formatted string
    return std::string(buf.get(), buf.get() + result);
}

#ifndef bzero
#define bzero(ptr,size)  memset((ptr),0,(size));
#endif //bzero

#if defined(ANDROID)
template <typename T>
std::string to_string(T value){
    std::ostringstream os ;
    os <<  std::forward<T>(value);
    return os.str() ;
}
#endif//ANDROID

#if defined(_WIN32)
int gettimeofday(struct timeval *tp, void *tzp);
void usleep(int micro_seconds);
void sleep(int second);
int vasprintf(char **strp, const char *fmt, va_list ap);
int asprintf(char **strp, const char *fmt, ...);
const char *strcasestr(const char *big, const char *little);

#if !defined(strcasecmp)
    #define strcasecmp _stricmp
#endif

#if !defined(strncasecmp)
#define strncasecmp _strnicmp
#endif

#ifndef ssize_t
    #ifdef _WIN64
        #define ssize_t int64_t
    #else
        #define ssize_t int32_t
    #endif
#endif
#endif //WIN32

/**
 * 获取时间差, 返回值单位为秒
 */
long getGMTOff();

/**
 * 获取1970年至今的毫秒数
 * @param system_time 是否为系统时间(系统时间可以回退),否则为程序启动时间(不可回退)
 */
uint64_t getCurrentMillisecond(bool system_time = false);

/**
 * 获取1970年至今的微秒数
 * @param system_time 是否为系统时间(系统时间可以回退),否则为程序启动时间(不可回退)
 */
uint64_t getCurrentMicrosecond(bool system_time = false);

/**
 * 获取时间字符串
 * @param fmt 时间格式，譬如%Y-%m-%d %H:%M:%S
 * @return 时间字符串
 */
std::string getTimeStr(const char *fmt,time_t time = 0);

/**
 * 根据unix时间戳获取本地时间
 * @param sec unix时间戳
 * @return tm结构体
 */
struct tm getLocalTime(time_t sec);

/**
 * 设置线程名
 */
void setThreadName(const char *name);

/**
 * 获取线程名
 */
std::string getThreadName();

/**
 * 设置当前线程cpu亲和性
 * @param i cpu索引，如果为-1，那么取消cpu亲和性
 * @return 是否成功，目前只支持linux
 */
bool setThreadAffinity(int i);

/**
 * 根据typeid(class).name()获取类名
 */
std::string demangle(const char *mangled);

/**
 * 获取环境变量内容，以'$'开头
 */
std::string getEnv(const std::string &key);

// 可以保存任意的对象
class Any {
public:
    using Ptr = std::shared_ptr<Any>;

    Any() = default;
    ~Any() = default;

    Any(const Any &that) = default;
    Any(Any &&that) {
        _type = that._type;
        _data = std::move(that._data);
    }

    Any &operator=(const Any &that) = default;
    Any &operator=(Any &&that) {
        _type = that._type;
        _data = std::move(that._data);
        return *this;
    }

    template <typename T, typename... ArgsType>
    void set(ArgsType &&...args) {
        _type = &typeid(T);
        _data.reset(new T(std::forward<ArgsType>(args)...), [](void *ptr) { delete (T *)ptr; });
    }

    template <typename T>
    void set(std::shared_ptr<T> data) {
        if (data) {
            _type = &typeid(T);
            _data = std::move(data);
        } else {
            reset();
        }
    }

    template <typename T>
    T &get(bool safe = true) {
        if (!_data) {
            throw std::invalid_argument("Any is empty");
        }
        if (safe && !is<T>()) {
            throw std::invalid_argument("Any::get(): " + demangle(_type->name()) + " unable cast to " + demangle(typeid(T).name()));
        }
        return *((T *)_data.get());
    }

    template <typename T>
    const T &get(bool safe = true) const {
        return const_cast<Any &>(*this).get<T>(safe);
    }

    template <typename T>
    bool is() const {
        return _type && typeid(T) == *_type;
    }

    operator bool() const { return _data.operator bool(); }
    bool empty() const { return !bool(); }

    void reset() {
        _type = nullptr;
        _data = nullptr;
    }

    Any &operator=(std::nullptr_t) {
        reset();
        return *this;
    }

    std::string type_name() const {
        if (!_type) {
            return "";
        }
        return demangle(_type->name());
    }

private:
    const std::type_info* _type = nullptr;
    std::shared_ptr<void> _data;
};

// 用于保存一些外加属性
class AnyStorage : public std::unordered_map<std::string, Any> {
public:
    AnyStorage() = default;
    ~AnyStorage() = default;
    using Ptr = std::shared_ptr<AnyStorage>;
};

}  // namespace toolkit
#endif /* UTIL_UTIL_H_ */
