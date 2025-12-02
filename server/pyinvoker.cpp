#if defined(ENABLE_PYTHON)

#include "pyinvoker.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include "WebHook.h"

using namespace toolkit;
using namespace mediakit;

template <typename T>
typename std::enable_if<std::is_copy_constructible<T>::value, py::capsule>::type to_python(const T &obj) {
    static auto name_str = toolkit::demangle(typeid(T).name());
    auto p = new toolkit::Any(std::make_shared<T>(obj));
    return py::capsule(p, name_str.data(), [](PyObject *capsule) {
        auto p = reinterpret_cast<toolkit::Any *>(PyCapsule_GetPointer(capsule, name_str.data()));
        delete p;
        TraceL << "delete " << name_str << "(" << p << ")";
    });
}

template <typename T>
typename std::enable_if<!std::is_copy_constructible<T>::value, py::capsule>::type to_python(const T &obj) {
    static auto name_str = toolkit::demangle(typeid(T).name());
    auto p = new toolkit::Any(std::shared_ptr<T>(const_cast<T *>(&obj), [](T *) {}));
    return py::capsule(p, name_str.data(), [](PyObject *capsule) {
        auto p = reinterpret_cast<toolkit::Any *>(PyCapsule_GetPointer(capsule, name_str.data()));
        delete p;
        TraceL << "unref " << name_str << "(" << p << ")";
    });
}

extern ArgsType make_json(const MediaInfo &args);
extern void fillSockInfo(Json::Value & val, SockInfo* info);

static py::dict jsonToPython(const Json::Value &obj) {
    py::dict ret;
    if (obj.isObject()) {
        for (auto it = obj.begin(); it != obj.end(); ++it) {
            if (it->isNull()) {
                // 忽略null，修复wvp传null覆盖Protocol配置的问题
                continue;
            }
            try {
                auto str = (*it).asString();
                ret[it.name().data()] = std::move(str);
            } catch (std::exception &) {
                WarnL << "Json is not convertible to string, key: " << it.name() << ", value: " << (*it);
            }
        }
    }
    return ret;
}

py::dict to_python(const MediaInfo &args) {
    auto json = make_json(args);
    return jsonToPython(json);
}

py::dict to_python(const SockInfo &info) {
    Json::Value json;
    fillSockInfo(json, const_cast<SockInfo *>(&info));
    return jsonToPython(json);
}

template <typename T>
T &to_native(const py::capsule &cap) {
    static auto name_str = toolkit::demangle(typeid(T).name());
    if (std::string(cap.name()) != name_str) {
        throw std::runtime_error("Invalid capsule name!");
    }
    auto any = static_cast<toolkit::Any *>(cap.get_pointer());
    return any->get<T>();
}

mINI to_native(const py::dict &opt) {
    mINI ret;
    for (auto &item : opt) {
        // 转换为字符串（允许 int/float/bool 等）
        ret.emplace(py::str(item.first).cast<std::string>(), py::str(item.second).cast<std::string>());
    }
    return ret;
}

PYBIND11_EMBEDDED_MODULE(mk_loader, m) {
    m.def("log", [](int lev, const char *file, int line, const char *func, const char *content) {
        py::gil_scoped_release release;
        LoggerWrapper::printLog(::toolkit::getLogger(), lev, file, func, line, content);
    });
    m.def("publish_auth_invoker_do", [](const py::capsule &cap, const std::string &err, const py::dict &opt) {
        ProtocolOption option;
        option.load(to_native(opt));
        // 执行c++代码时释放gil锁
        py::gil_scoped_release release;
        auto &invoker = to_native<Broadcast::PublishAuthInvoker>(cap);
        invoker(err, option);
    });
    m.def("auth_invoker_do", [](const py::capsule &cap, const std::string &err) {
        // 执行c++代码时释放gil锁
        py::gil_scoped_release release;
        auto &invoker = to_native<Broadcast::AuthInvoker>(cap);
        invoker(err);
    });
}

namespace mediakit {

inline bool set_env(const char *name, const char *value) {
#if defined(_WIN32)
    std::string env_str = std::string(name) + "=" + value;
    return _putenv(env_str.c_str()) == 0;
#else
    return setenv(name, value, 1) == 0; // overwrite = 1
#endif
}

bool set_python_path() {
    const char *env_var = std::getenv("PYTHONPATH");
    if (env_var && *env_var) {
        PrintI("PYTHONPATH is already set to: %s", env_var);
        return false;
    }
    auto default_path = exeDir() + "/python";
    // 1 表示覆盖已存在的值
    if (!set_env("PYTHONPATH", default_path.data())) {
        PrintW("Failed to set PYTHONPATH");
        return false;
    }
    PrintI("PYTHONPATH was not set. Set to default: %s", default_path.data());
    return true;
}

PythonInvoker &PythonInvoker::Instance() {
    static std::shared_ptr<PythonInvoker> instance(new PythonInvoker);
    return *instance;
}

PythonInvoker::PythonInvoker() {
    // 确保日志一直可用
    _logger = Logger::Instance().shared_from_this();
    set_python_path(); // 确保 PYTHONPATH 在第一次调用时设置
    _interpreter = new py::scoped_interpreter;
    _rel = new py::gil_scoped_release;
}

PythonInvoker::~PythonInvoker() {
    {
        py::gil_scoped_acquire gil; // 加锁
        if (_on_exit) {
            _on_exit();
        }
        _on_exit = py::object();
        _on_publish = py::object();
        _module = py::module();
    }

    delete _rel;
    delete _interpreter;
}

void PythonInvoker::load(const std::string &module_name) {
    try {
        py::gil_scoped_acquire gil; // 加锁
        _module = py::module::import(module_name.c_str());
        if (hasattr(_module, "on_start")) {
            py::object on_start = _module.attr("on_start");
            if (on_start) {
                on_start();
            }
        }
        if (hasattr(_module, "on_exit")) {
            _on_exit = _module.attr("on_exit");
        }
        if (hasattr(_module, "on_publish")) {
            _on_publish = _module.attr("on_publish");
        }
        if (hasattr(_module, "on_play")) {
            _on_play = _module.attr("on_play");
        }
        if (hasattr(_module, "on_flow_report")) {
            _on_flow_report = _module.attr("on_flow_report");
        }
    } catch (py::error_already_set &e) {
        PrintE("Python exception:%s", e.what());
    }
}

bool PythonInvoker::on_publish(BroadcastMediaPublishArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_publish) {
        return false;
    }
    return _on_publish(getOriginTypeString(type), to_python(args), to_python(invoker), to_python(sender)).cast<bool>();
}

bool PythonInvoker::on_play(BroadcastMediaPlayedArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_play) {
        return false;
    }
    return _on_play(to_python(args), to_python(invoker), to_python(sender)).cast<bool>();
}

bool PythonInvoker::on_flow_report(BroadcastFlowReportArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_flow_report) {
        return false;
    }
    return _on_flow_report(to_python(args), totalBytes, totalDuration, isPlayer, to_python(sender)).cast<bool>();
}

} // namespace mediakit

#endif
