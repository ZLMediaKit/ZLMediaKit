//
// Created by Administrator on 2024/3/18.
//

#ifndef ANLIBRARY_LOGGER_H
#define ANLIBRARY_LOGGER_H


#include "spdlog/spdlog.h"
#include "spdlog/sinks/android_sink.h"
#include "spdlog/cfg/env.h"
#include "spdlog/sinks/basic_file_sink.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"

class logger;

class logger {

public:
    static logger *instance() {
        static logger logger;
        return &logger;
    }

    std::shared_ptr<spdlog::logger> getLogger() {
        return m_logger;
    };


    logger *setSavePath(const char *save_path) {
        this->save_path = save_path;

        return this;
    }

    void init() {
        std::vector<spdlog::sink_ptr> sinks;
#ifdef __ANDROID__
        sinks.push_back(std::make_shared<spdlog::sinks::android_sink_mt>());
#else
        sinks.push_back(std::make_shared<spd::sinks::stdout_sink_mt>());
#endif
        sinks.push_back(
                std::make_shared<spdlog::sinks::rotating_file_sink_mt>(save_path, 1024 * 1024 * 5,
                                                                       5));
        m_logger = std::make_shared<spdlog::logger>("android", sinks.begin(), sinks.end());

        //m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v");
        m_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e][thread %t][%@,%!][%=8l] : %v");
        m_logger->set_level(spdlog::level::trace);
        m_logger->flush_on(spdlog::level::err);


    }

private:
    std::shared_ptr<spdlog::logger> m_logger;
    const char *save_path = "";

private:

    logger() {};


    ~logger() {};

    void *operator new(size_t size);

    logger(const logger &) = delete;

};


//#define LOG_TRACE(...) SPDLOG_LOGGER_CALL(logger::instance()->getLogger().get(), spdlog::level::trace, __VA_ARGS__)
//#define LOG_INFO(...) logger::instance()->getLogger().get()->info(__VA_ARGS__)
//#define LOG_ERROR(...) logger::instance()->getLogger().get()->error(__VA_ARGS__)
//#define LOG_WARN(...) logger::instance()->getLogger().get()->warn(__VA_ARGS__)
//#define LOG_DEBUG(...) logger::instance()->getLogger().get()->debug(__VA_ARGS__)
//#define LOG_CRITICAL(...) logger::instance()->getLogger().get()->critical(__VA_ARGS__)
//#define LOG_TRACE(...) logger::instance()->getLogger().get()->trace(__VA_ARGS__)

#define LOG_TRACE(...) SPDLOG_LOGGER_CALL(logger::instance()->getLogger().get(), spdlog::level::trace, __VA_ARGS__)
#define LOG_INFO(...) SPDLOG_LOGGER_CALL(logger::instance()->getLogger().get(), spdlog::level::info, __VA_ARGS__)
#define LOG_ERROR(...) SPDLOG_LOGGER_CALL(logger::instance()->getLogger().get(), spdlog::level::err, __VA_ARGS__)
#define LOG_WARN(...) SPDLOG_LOGGER_CALL(logger::instance()->getLogger().get(), spdlog::level::warn, __VA_ARGS__)
#define LOG_DEBUG(...) SPDLOG_LOGGER_CALL(logger::instance()->getLogger().get(), spdlog::level::debug, __VA_ARGS__)
#define LOG_CRITICAL(...) SPDLOG_LOGGER_CALL(logger::instance()->getLogger().get(), spdlog::level::critical, __VA_ARGS__)


#endif //ANLIBRARY_LOGGER_H
