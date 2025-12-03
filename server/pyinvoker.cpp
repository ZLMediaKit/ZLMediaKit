#if defined(ENABLE_PYTHON)

#include "pyinvoker.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include "WebHook.h"
#include "Common/Parser.h"
#include "Http/HttpSession.h"

using namespace toolkit;
using namespace mediakit;

extern ArgsType make_json(const MediaInfo &args);
extern void fillSockInfo(Json::Value & val, SockInfo* info);
extern std::string g_ini_file;

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

void handle_http_request(const py::object &check_route, const py::object &submit_coro, const Parser &parser, const HttpSession::HttpResponseInvoker &invoker, bool &consumed, toolkit::SockInfo &sender) {
    py::gil_scoped_acquire guard;

    py::dict scope;
    scope["type"] = "http";
    scope["http_version"] = "1.1";
    scope["method"] = parser.method();
    scope["path"] = parser.url();
    scope["query_string"] = parser.params();
    py::list hdrs;
    for (auto &kv : parser.getHeader()) {
        hdrs.append(py::make_tuple(py::bytes(kv.first), py::bytes(kv.second)));
    }
    scope["headers"] = hdrs;

    bool ok = check_route(scope).cast<bool>();
    if (!ok) {
        return;
    }
    consumed = true;

    StrCaseMap resp_headers;
    std::string resp_body;
    int status = 500;
    auto send = py::cpp_function([invoker, status, resp_body, resp_headers](const py::dict &msg) mutable {
        auto type = msg["type"].cast<std::string>();
        if (type == "http.response.start") {
            status = msg["status"].cast<int>();
            for (auto tup : msg["headers"].cast<py::list>()) {
                auto t = tup.cast<py::tuple>();
                resp_headers[t[0].cast<std::string>()] = t[1].cast<std::string>();
            }
            return;
        }

        if (type == "http.response.body") {
            resp_body += msg["body"].cast<std::string>();
            // 💥 只在 more_body=False 时回调
            bool more = msg.contains("more_body") && msg["more_body"].cast<bool>();
            if (!more) {
                invoker(status, resp_headers, resp_body);
            }
        }
    });

    submit_coro(scope, py::bytes(parser.content()), send);
}


PYBIND11_EMBEDDED_MODULE(mk_loader, m) {
    m.def("log", [](int lev, const char *file, int line, const char *func, const char *content) {
        py::gil_scoped_release release;
        LoggerWrapper::printLog(::toolkit::getLogger(), lev, file, func, line, content);
    });

    m.def("get_config", [](const std::string &key) -> std::string {
        py::gil_scoped_release release;
        const auto it = mINI::Instance().find(key);
        if (it != mINI::Instance().end()) {
            return it->second;
        }
        return "";
    });
    m.def("set_config", [](const std::string &key, const std::string &value) -> bool {
        py::gil_scoped_release release;
        mINI::Instance()[key]= value;
        return true;
    });
    m.def("update_config", []() {
        NOTICE_EMIT(BroadcastReloadConfigArgs, Broadcast::kBroadcastReloadConfig);
        mINI::Instance().dumpFile(g_ini_file);
        return true;
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

    m.def("set_fastapi", [](const py::object &check_route, const py::object &submit_coro) {
        static void *fastapi_tag = nullptr;
        NoticeCenter::Instance().delListener(&fastapi_tag, Broadcast::kBroadcastHttpRequest);
        NoticeCenter::Instance().addListener(&fastapi_tag, Broadcast::kBroadcastHttpRequest, [check_route, submit_coro](BroadcastHttpRequestArgs) {
            handle_http_request(check_route, submit_coro, parser, invoker, consumed, sender);
        });
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

    NoticeCenter::Instance().addListener(this, Broadcast::kBroadcastReloadConfig, [this] (BroadcastReloadConfigArgs) {
        if (_on_reload_config) {
            _on_reload_config();
        }
    });
}

PythonInvoker::~PythonInvoker() {
    NoticeCenter::Instance().delListener(this, Broadcast::kBroadcastReloadConfig);
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
        if (hasattr(_module, "on_reload_config")) {
            _on_reload_config = _module.attr("on_reload_config");
        }
        if (hasattr(_module, "on_start")) {
            py::object on_start = _module.attr("on_start");
            if (on_start) {
                on_start();
            }
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
