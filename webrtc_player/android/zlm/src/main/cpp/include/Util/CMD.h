/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/ZLMediaKit/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_UTIL_CMD_H_
#define SRC_UTIL_CMD_H_

#include <map>
#include <mutex>
#include <string>
#include <memory>
#include <vector>
#include <iostream>
#include <functional>
#include "mini.h"

namespace toolkit{

class Option {
public:
    using OptionHandler = std::function<bool(const std::shared_ptr<std::ostream> &stream, const std::string &arg)>;

    enum ArgType {
        ArgNone = 0,//no_argument,
        ArgRequired = 1,//required_argument,
        ArgOptional = 2,//optional_argument
    };

    Option() = default;

    Option(char short_opt, const char *long_opt, enum ArgType type, const char *default_value, bool must_exist,
           const char *des, const OptionHandler &cb) {
        _short_opt = short_opt;
        _long_opt = long_opt;
        _type = type;
        if (type != ArgNone) {
            if (default_value) {
                _default_value = std::make_shared<std::string>(default_value);
            }
            if (!_default_value && must_exist) {
                _must_exist = true;
            }
        }
        _des = des;
        _cb = cb;
    }

    bool operator()(const std::shared_ptr<std::ostream> &stream, const std::string &arg) {
        return _cb ? _cb(stream, arg) : true;
    }

private:
    friend class OptionParser;
    bool _must_exist = false;
    char _short_opt;
    enum ArgType _type;
    std::string _des;
    std::string _long_opt;
    OptionHandler _cb;
    std::shared_ptr<std::string> _default_value;
};

class OptionParser {
public:
    using OptionCompleted = std::function<void(const std::shared_ptr<std::ostream> &, mINI &)>;

    OptionParser(const OptionCompleted &cb = nullptr, bool enable_empty_args = true) {
        _on_completed = cb;
        _enable_empty_args = enable_empty_args;
        _helper = Option('h', "help", Option::ArgNone, nullptr, false, "打印此信息",
                         [this](const std::shared_ptr<std::ostream> &stream,const std::string &arg)->bool {
             static const char *argsType[] = {"无参", "有参", "选参"};
             static const char *mustExist[] = {"选填", "必填"};
             static std::string defaultPrefix = "默认:";
             static std::string defaultNull = "null";

             std::stringstream printer;
             size_t maxLen_longOpt = 0;
             auto maxLen_default = defaultNull.size();

             for (auto &pr : _map_options) {
                 auto &opt = pr.second;
                 if (opt._long_opt.size() > maxLen_longOpt) {
                     maxLen_longOpt = opt._long_opt.size();
                 }
                 if (opt._default_value) {
                     if (opt._default_value->size() > maxLen_default) {
                         maxLen_default = opt._default_value->size();
                     }
                 }
             }
             for (auto &pr : _map_options) {
                 auto &opt = pr.second;
                 //打印短参和长参名
                 if (opt._short_opt) {
                     printer << "  -" << opt._short_opt << "  --" << opt._long_opt;
                 } else {
                     printer << "   " << " " << "  --" << opt._long_opt;
                 }
                 for (size_t i = 0; i < maxLen_longOpt - opt._long_opt.size(); ++i) {
                     printer << " ";
                 }
                 //打印是否有参
                 printer << "  " << argsType[opt._type];
                 //打印默认参数
                 std::string defaultValue = defaultNull;
                 if (opt._default_value) {
                     defaultValue = *opt._default_value;
                 }
                 printer << "  " << defaultPrefix << defaultValue;
                 for (size_t i = 0; i < maxLen_default - defaultValue.size(); ++i) {
                     printer << " ";
                 }
                 //打印是否必填参数
                 printer << "  " << mustExist[opt._must_exist];
                 //打印描述
                 printer << "  " << opt._des << std::endl;
             }
             throw std::invalid_argument(printer.str());
         });
        (*this) << _helper;
    }

    OptionParser &operator<<(Option &&option) {
        int index = 0xFF + (int) _map_options.size();
        if (option._short_opt) {
            _map_char_index.emplace(option._short_opt, index);
        }
        _map_options.emplace(index, std::forward<Option>(option));
        return *this;
    }

    OptionParser &operator<<(const Option &option) {
        int index = 0xFF + (int) _map_options.size();
        if (option._short_opt) {
            _map_char_index.emplace(option._short_opt, index);
        }
        _map_options.emplace(index, option);
        return *this;
    }

    void delOption(const char *key) {
        for (auto &pr : _map_options) {
            if (pr.second._long_opt == key) {
                if (pr.second._short_opt) {
                    _map_char_index.erase(pr.second._short_opt);
                }
                _map_options.erase(pr.first);
                break;
            }
        }
    }

    void operator ()(mINI &all_args, int argc, char *argv[], const std::shared_ptr<std::ostream> &stream);

private:
    bool _enable_empty_args;
    Option _helper;
    std::map<char, int> _map_char_index;
    std::map<int, Option> _map_options;
    OptionCompleted _on_completed;
};

class CMD : public mINI {
public:
    virtual ~CMD() = default;

    virtual const char *description() const {
        return "description";
    }

    void operator()(int argc, char *argv[], const std::shared_ptr<std::ostream> &stream = nullptr) {
        this->clear();
        std::shared_ptr<std::ostream> coutPtr(&std::cout, [](std::ostream *) {});
        (*_parser)(*this, argc, argv, stream ? stream : coutPtr);
    }

    bool hasKey(const char *key) {
        return this->find(key) != this->end();
    }

    std::vector<variant> splitedVal(const char *key, const char *delim = ":") {
        std::vector<variant> ret;
        auto &val = (*this)[key];
        split(val, delim, ret);
        return ret;
    }

    void delOption(const char *key) {
        if (_parser) {
            _parser->delOption(key);
        }
    }

protected:
    std::shared_ptr<OptionParser> _parser;

private:
    void split(const std::string &s, const char *delim, std::vector<variant> &ret) {
        size_t last = 0;
        auto index = s.find(delim, last);
        while (index != std::string::npos) {
            if (index - last > 0) {
                ret.push_back(s.substr(last, index - last));
            }
            last = index + strlen(delim);
            index = s.find(delim, last);
        }
        if (s.size() - last > 0) {
            ret.push_back(s.substr(last));
        }
    }
};

class CMDRegister {
public:
    static CMDRegister &Instance();

    void clear() {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        _cmd_map.clear();
    }

    void registCMD(const char *name, const std::shared_ptr<CMD> &cmd) {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        _cmd_map.emplace(name, cmd);
    }

    void unregistCMD(const char *name) {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        _cmd_map.erase(name);
    }

    std::shared_ptr<CMD> operator[](const char *name) {
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto it = _cmd_map.find(name);
        if (it == _cmd_map.end()) {
            throw std::invalid_argument(std::string("CMD not existed: ") + name);
        }
        return it->second;
    }

    void operator()(const char *name, int argc, char *argv[], const std::shared_ptr<std::ostream> &stream = nullptr) {
        auto cmd = (*this)[name];
        if (!cmd) {
            throw std::invalid_argument(std::string("CMD not existed: ") + name);
        }
        (*cmd)(argc, argv, stream);
    }

    void printHelp(const std::shared_ptr<std::ostream> &streamTmp = nullptr) {
        auto stream = streamTmp;
        if (!stream) {
            stream.reset(&std::cout, [](std::ostream *) {});
        }

        std::lock_guard<std::recursive_mutex> lck(_mtx);
        size_t maxLen = 0;
        for (auto &pr : _cmd_map) {
            if (pr.first.size() > maxLen) {
                maxLen = pr.first.size();
            }
        }
        for (auto &pr : _cmd_map) {
            (*stream) << "  " << pr.first;
            for (size_t i = 0; i < maxLen - pr.first.size(); ++i) {
                (*stream) << " ";
            }
            (*stream) << "  " << pr.second->description() << std::endl;
        }
    }

    void operator()(const std::string &line, const std::shared_ptr<std::ostream> &stream = nullptr) {
        if (line.empty()) {
            return;
        }
        std::vector<char *> argv;
        size_t argc = getArgs((char *) line.data(), argv);
        if (argc == 0) {
            return;
        }
        std::string cmd = argv[0];
        std::lock_guard<std::recursive_mutex> lck(_mtx);
        auto it = _cmd_map.find(cmd);
        if (it == _cmd_map.end()) {
            std::stringstream ss;
            ss << "  未识别的命令\"" << cmd << "\",输入 \"help\" 获取帮助.";
            throw std::invalid_argument(ss.str());
        }
        (*it->second)((int) argc, &argv[0], stream);
    }

private:
    size_t getArgs(char *buf, std::vector<char *> &argv) {
        size_t argc = 0;
        bool start = false;
        auto len = strlen(buf);
        for (size_t i = 0; i < len; ++i) {
            if (buf[i] != ' ' && buf[i] != '\t' && buf[i] != '\r' && buf[i] != '\n') {
                if (!start) {
                    start = true;
                    if (argv.size() < argc + 1) {
                        argv.resize(argc + 1);
                    }
                    argv[argc++] = buf + i;
                }
            } else {
                buf[i] = '\0';
                start = false;
            }
        }
        return argc;
    }

private:
    std::recursive_mutex _mtx;
    std::map<std::string, std::shared_ptr<CMD> > _cmd_map;
};

//帮助命令(help)，该命令默认已注册
class CMD_help : public CMD {
public:
    CMD_help() {
        _parser = std::make_shared<OptionParser>([](const std::shared_ptr<std::ostream> &stream, mINI &) {
            CMDRegister::Instance().printHelp(stream);
        });
    }

    const char *description() const override {
        return "打印帮助信息";
    }
};

class ExitException : public std::exception {};

//退出程序命令(exit)，该命令默认已注册
class CMD_exit : public CMD {
public:
    CMD_exit() {
        _parser = std::make_shared<OptionParser>([](const std::shared_ptr<std::ostream> &, mINI &) {
            throw ExitException();
        });
    }

    const char *description() const override {
        return "退出shell";
    }
};

//退出程序命令(quit),该命令默认已注册
#define CMD_quit CMD_exit

//清空屏幕信息命令(clear)，该命令默认已注册
class CMD_clear : public CMD {
public:
    CMD_clear() {
        _parser = std::make_shared<OptionParser>([this](const std::shared_ptr<std::ostream> &stream, mINI &args) {
            clear(stream);
        });
    }

    const char *description() const {
        return "清空屏幕输出";
    }

private:
    void clear(const std::shared_ptr<std::ostream> &stream) {
        (*stream) << "\x1b[2J\x1b[H";
        stream->flush();
    }
};

#define GET_CMD(name) (*(CMDRegister::Instance()[name]))
#define CMD_DO(name,...) (*(CMDRegister::Instance()[name]))(__VA_ARGS__)
#define REGIST_CMD(name) CMDRegister::Instance().registCMD(#name,std::make_shared<CMD_##name>());

}//namespace toolkit
#endif /* SRC_UTIL_CMD_H_ */
