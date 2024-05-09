/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef UTIL_LOGGER_H_
#define UTIL_LOGGER_H_

#include <cstdarg>
#include <set>
#include <map>
#include <fstream>
#include <thread>
#include <memory>
#include <mutex>
#include "util.h"
#include "List.h"
#include "Thread/semaphore.h"

namespace toolkit {

class LogContext;
class LogChannel;
class LogWriter;
class Logger;

using LogContextPtr = std::shared_ptr<LogContext>;

typedef enum {
    LTrace = 0, LDebug, LInfo, LWarn, LError
} LogLevel;

Logger &getLogger();
void setLogger(Logger *logger);

/**
* 日志类
*/
class Logger : public std::enable_shared_from_this<Logger>, public noncopyable {
public:
    friend class AsyncLogWriter;
    using Ptr = std::shared_ptr<Logger>;

    /**
     * 获取日志单例
     * @return
     */
    static Logger &Instance();

    explicit Logger(const std::string &loggerName);
    ~Logger();

    /**
     * 添加日志通道，非线程安全的
     * @param channel log通道
     */
    void add(const std::shared_ptr<LogChannel> &channel);

    /**
     * 删除日志通道，非线程安全的
     * @param name log通道名
     */
    void del(const std::string &name);

    /**
     * 获取日志通道，非线程安全的
     * @param name log通道名
     * @return 线程通道
     */
    std::shared_ptr<LogChannel> get(const std::string &name);

    /**
     * 设置写log器，非线程安全的
     * @param writer 写log器
     */
    void setWriter(const std::shared_ptr<LogWriter> &writer);

    /**
     * 设置所有日志通道的log等级
     * @param level log等级
     */
    void setLevel(LogLevel level);

    /**
     * 获取logger名
     * @return logger名
     */
    const std::string &getName() const;

    /**
     * 写日志
     * @param ctx 日志信息
     */
    void write(const LogContextPtr &ctx);

private:
    /**
     * 写日志到各channel，仅供AsyncLogWriter调用
     * @param ctx 日志信息
     */
    void writeChannels(const LogContextPtr &ctx);
    void writeChannels_l(const LogContextPtr &ctx);

private:
    LogContextPtr _last_log;
    std::string _logger_name;
    std::shared_ptr<LogWriter> _writer;
    std::shared_ptr<LogChannel> _default_channel;
    std::map<std::string, std::shared_ptr<LogChannel> > _channels;
};

///////////////////LogContext///////////////////
/**
* 日志上下文
*/
class LogContext : public std::ostringstream {
public:
    //_file,_function改成string保存，目的是有些情况下，指针可能会失效
    //比如说动态库中打印了一条日志，然后动态库卸载了，那么指向静态数据区的指针就会失效
    LogContext() = default;
    LogContext(LogLevel level, const char *file, const char *function, int line, const char *module_name, const char *flag);
    ~LogContext() = default;

    LogLevel _level;
    int _line;
    int _repeat = 0;
    std::string _file;
    std::string _function;
    std::string _thread_name;
    std::string _module_name;
    std::string _flag;
    struct timeval _tv;

    const std::string &str();

private:
    bool _got_content = false;
    std::string _content;
};

/**
 * 日志上下文捕获器
 */
class LogContextCapture {
public:
    using Ptr = std::shared_ptr<LogContextCapture>;

    LogContextCapture(Logger &logger, LogLevel level, const char *file, const char *function, int line, const char *flag = "");
    LogContextCapture(const LogContextCapture &that);
    ~LogContextCapture();

    /**
     * 输入std::endl(回车符)立即输出日志
     * @param f std::endl(回车符)
     * @return 自身引用
     */
    LogContextCapture &operator<<(std::ostream &(*f)(std::ostream &));

    template<typename T>
    LogContextCapture &operator<<(T &&data) {
        if (!_ctx) {
            return *this;
        }
        (*_ctx) << std::forward<T>(data);
        return *this;
    }

    void clear();

private:
    LogContextPtr _ctx;
    Logger &_logger;
};


///////////////////LogWriter///////////////////
/**
 * 写日志器
 */
class LogWriter : public noncopyable {
public:
    LogWriter() = default;
    virtual ~LogWriter() = default;

    virtual void write(const LogContextPtr &ctx, Logger &logger) = 0;
};

class AsyncLogWriter : public LogWriter {
public:
    AsyncLogWriter();
    ~AsyncLogWriter();

private:
    void run();
    void flushAll();
    void write(const LogContextPtr &ctx, Logger &logger) override;

private:
    bool _exit_flag;
    semaphore _sem;
    std::mutex _mutex;
    std::shared_ptr<std::thread> _thread;
    List<std::pair<LogContextPtr, Logger *> > _pending;
};

///////////////////LogChannel///////////////////
/**
 * 日志通道
 */
class LogChannel : public noncopyable {
public:
    LogChannel(const std::string &name, LogLevel level = LTrace);
    virtual ~LogChannel();

    virtual void write(const Logger &logger, const LogContextPtr &ctx) = 0;
    const std::string &name() const;
    void setLevel(LogLevel level);
    static std::string printTime(const timeval &tv);

protected:
    /**
    * 打印日志至输出流
    * @param ost 输出流
    * @param enable_color 是否启用颜色
    * @param enable_detail 是否打印细节(函数名、源码文件名、源码行)
    */
    virtual void format(const Logger &logger, std::ostream &ost, const LogContextPtr &ctx, bool enable_color = true, bool enable_detail = true);

protected:
    std::string _name;
    LogLevel _level;
};

/**
 * 输出日至到广播
 */
class EventChannel : public LogChannel {
public:
    //输出日志时的广播名
    static const std::string kBroadcastLogEvent;
    //日志广播参数类型和列表
    #define BroadcastLogEventArgs const Logger &logger, const LogContextPtr &ctx

    EventChannel(const std::string &name = "EventChannel", LogLevel level = LTrace);
    ~EventChannel() override = default;

    void write(const Logger &logger, const LogContextPtr &ctx) override;
};

/**
 * 输出日志至终端，支持输出日志至android logcat
 */
class ConsoleChannel : public LogChannel {
public:
    ConsoleChannel(const std::string &name = "ConsoleChannel", LogLevel level = LTrace);
    ~ConsoleChannel() override = default;

    void write(const Logger &logger, const LogContextPtr &logContext) override;
};

/**
 * 输出日志至文件
 */
class FileChannelBase : public LogChannel {
public:
    FileChannelBase(const std::string &name = "FileChannelBase", const std::string &path = exePath() + ".log", LogLevel level = LTrace);
    ~FileChannelBase() override;

    void write(const Logger &logger, const LogContextPtr &ctx) override;
    bool setPath(const std::string &path);
    const std::string &path() const;

protected:
    virtual bool open();
    virtual void close();
    virtual size_t size();

protected:
    std::string _path;
    std::ofstream _fstream;
};

class Ticker;

/**
 * 自动清理的日志文件通道
 * 默认最多保存30天的日志
 */
class FileChannel : public FileChannelBase {
public:
    FileChannel(const std::string &name = "FileChannel", const std::string &dir = exeDir() + "log/", LogLevel level = LTrace);
    ~FileChannel() override = default;

    /**
     * 写日志时才会触发新建日志文件或者删除老的日志文件
     * @param logger
     * @param stream
     */
    void write(const Logger &logger, const LogContextPtr &ctx) override;

    /**
     * 设置日志最大保存天数
     * @param max_day 天数
     */
    void setMaxDay(size_t max_day);

    /**
     * 设置日志切片文件最大大小
     * @param max_size 单位MB
     */
    void setFileMaxSize(size_t max_size);

    /**
     * 设置日志切片文件最大个数
     * @param max_count 个数
     */
    void setFileMaxCount(size_t max_count);

private:
    /**
     * 删除日志切片文件，条件为超过最大保存天数与最大切片个数
     */
    void clean();

    /**
     * 检查当前日志切片文件大小，如果超过限制，则创建新的日志切片文件
     */
    void checkSize(time_t second);

    /**
     * 创建并切换到下一个日志切片文件
     */
    void changeFile(time_t second);

private:
    bool _can_write = false;
    //默认最多保存30天的日志文件
    size_t _log_max_day = 30;
    //每个日志切片文件最大默认128MB
    size_t _log_max_size = 128;
    //最多默认保持30个日志切片文件
    size_t _log_max_count = 30;
    //当前日志切片文件索引
    size_t _index = 0;
    int64_t _last_day = -1;
    time_t _last_check_time = 0;
    std::string _dir;
    std::set<std::string> _log_file_map;
};

#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) && !defined(ANDROID))
class SysLogChannel : public LogChannel {
public:
    SysLogChannel(const std::string &name = "SysLogChannel", LogLevel level = LTrace);
    ~SysLogChannel() override = default;

    void write(const Logger &logger, const LogContextPtr &logContext) override;
};

#endif//#if defined(__MACH__) || ((defined(__linux) || defined(__linux__)) &&  !defined(ANDROID))

class BaseLogFlagInterface {
protected:
    virtual ~BaseLogFlagInterface() {}
    // 获得日志标记Flag
    const char* getLogFlag(){
        return _log_flag;
    }
    void setLogFlag(const char *flag) { _log_flag = flag; }
private:
    const char *_log_flag = "";
};

class LoggerWrapper {
public:
    template<typename First, typename ...ARGS>
    static inline void printLogArray(Logger &logger, LogLevel level, const char *file, const char *function, int line, First &&first, ARGS &&...args) {
        LogContextCapture log(logger, level, file, function, line);
        log << std::forward<First>(first);
        appendLog(log, std::forward<ARGS>(args)...);
    }

    static inline void printLogArray(Logger &logger, LogLevel level, const char *file, const char *function, int line) {
        LogContextCapture log(logger, level, file, function, line);
    }

    template<typename Log, typename First, typename ...ARGS>
    static inline void appendLog(Log &out, First &&first, ARGS &&...args) {
        out << std::forward<First>(first);
        appendLog(out, std::forward<ARGS>(args)...);
    }

    template<typename Log>
    static inline void appendLog(Log &out) {}

    //printf样式的日志打印
    static void printLog(Logger &logger, int level, const char *file, const char *function, int line, const char *fmt, ...);
    static void printLogV(Logger &logger, int level, const char *file, const char *function, int line, const char *fmt, va_list ap);
};

//可重置默认值
extern Logger *g_defaultLogger;

//用法: DebugL << 1 << "+" << 2 << '=' << 3;
#define WriteL(level) ::toolkit::LogContextCapture(::toolkit::getLogger(), level, __FILE__, __FUNCTION__, __LINE__)
#define TraceL WriteL(::toolkit::LTrace)
#define DebugL WriteL(::toolkit::LDebug)
#define InfoL WriteL(::toolkit::LInfo)
#define WarnL WriteL(::toolkit::LWarn)
#define ErrorL WriteL(::toolkit::LError)

//只能在虚继承BaseLogFlagInterface的类中使用
#define WriteF(level) ::toolkit::LogContextCapture(::toolkit::getLogger(), level, __FILE__, __FUNCTION__, __LINE__, getLogFlag())
#define TraceF WriteF(::toolkit::LTrace)
#define DebugF WriteF(::toolkit::LDebug)
#define InfoF WriteF(::toolkit::LInfo)
#define WarnF WriteF(::toolkit::LWarn)
#define ErrorF WriteF(::toolkit::LError)

//用法: PrintD("%d + %s = %c", 1 "2", 'c');
#define PrintLog(level, ...) ::toolkit::LoggerWrapper::printLog(::toolkit::getLogger(), level, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define PrintT(...) PrintLog(::toolkit::LTrace, ##__VA_ARGS__)
#define PrintD(...) PrintLog(::toolkit::LDebug, ##__VA_ARGS__)
#define PrintI(...) PrintLog(::toolkit::LInfo, ##__VA_ARGS__)
#define PrintW(...) PrintLog(::toolkit::LWarn, ##__VA_ARGS__)
#define PrintE(...) PrintLog(::toolkit::LError, ##__VA_ARGS__)

//用法: LogD(1, "+", "2", '=', 3);
//用于模板实例化的原因，如果每次打印参数个数和类型不一致，可能会导致二进制代码膨胀
#define LogL(level, ...) ::toolkit::LoggerWrapper::printLogArray(::toolkit::getLogger(), (LogLevel)level, __FILE__, __FUNCTION__, __LINE__, ##__VA_ARGS__)
#define LogT(...) LogL(::toolkit::LTrace, ##__VA_ARGS__)
#define LogD(...) LogL(::toolkit::LDebug, ##__VA_ARGS__)
#define LogI(...) LogL(::toolkit::LInfo, ##__VA_ARGS__)
#define LogW(...) LogL(::toolkit::LWarn, ##__VA_ARGS__)
#define LogE(...) LogL(::toolkit::LError, ##__VA_ARGS__)

} /* namespace toolkit */
#endif /* UTIL_LOGGER_H_ */
