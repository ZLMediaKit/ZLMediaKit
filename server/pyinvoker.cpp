#if defined(ENABLE_PYTHON)

#include "pyinvoker.h"

#include <chrono>
#include <cstdlib>
#include <iostream>
#include <string>
#include <type_traits>
#include "WebApi.h"
#include "WebHook.h"
#include "Util/util.h"
#include "Util/File.h"
#include "Common/Parser.h"
#include "Http/HttpSession.h"

using namespace toolkit;
using namespace mediakit;

extern ArgsType make_json(const MediaInfo &args);
extern void fillSockInfo(Json::Value & val, SockInfo* info);
extern ArgsType getRecordInfo(const RecordInfo &info);
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

py::dict to_python(const RecordInfo &info) {
    return jsonToPython(getRecordInfo(info));
}

template <typename T>
std::shared_ptr<T> to_python_ref(const T &t) {
    return std::shared_ptr<T>(const_cast<T *>(&t), py::nodelete());
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

    Json::Value val;
    HttpSession::KeyValue headerOut;
    // http api被python拦截了，再api统一鉴权
    try {
        auto args = getAllArgs(parser);
        auto allArgs = ArgsMap(parser, args);
        GET_CONFIG(bool, legacy_auth , API::kLegacyAuth);
        if (!legacy_auth) {
            // 非传统secret鉴权模式，Python接口强制要求登录鉴权
            CHECK_SECRET();
        }
    } catch (std::exception &ex) {
        auto ex1 = dynamic_cast<ApiRetException *>(&ex);
        if (ex1) {
            val["code"] = ex1->code();
        } else {
            val["code"] = API::Exception;
        }
        val["msg"] = ex.what();
        headerOut["Content-Type"] = "application/json";
        invoker(200, headerOut, val.toStyledString());
        return;
    }

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

class MuxerDelegatePython : public MediaSinkInterface {
public:
    MuxerDelegatePython(py::object object) {
        _py_muxer = std::move(object);
        _input_frame = _py_muxer.attr("inputFrame");
        _add_track = _py_muxer.attr("addTrack");
        _add_track_completed = _py_muxer.attr("addTrackCompleted");
    }

    ~MuxerDelegatePython() override {
        py::gil_scoped_acquire guard;
        try {
            auto destroy = _py_muxer.attr("destroy");
            destroy();
            destroy = py::function();
        } catch (std::exception &ex) {
            ErrorL << "destroy python muxer failed: " << ex.what();
        }
        _input_frame = py::function();
        _add_track = py::function();
        _add_track_completed = py::function();
        _py_muxer = py::function();
    }

    bool addTrack(const Track::Ptr &track) override {
        py::gil_scoped_acquire guard;
        return _add_track ? _add_track(track).cast<bool>() : false;
    }

    void addTrackCompleted() override {
        py::gil_scoped_acquire guard;
        if (_add_track_completed) {
            _add_track_completed();
        }
    }

    bool inputFrame(const Frame::Ptr &frame) override {
        py::gil_scoped_acquire guard;
        return _input_frame ? _input_frame(frame).cast<bool>() : false;
    }

private:
    py::object _py_muxer;
    py::function _input_frame;
    py::function _add_track;
    py::function _add_track_completed;
};

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

    m.def("get_full_path", [](const std::string &path, const std::string &current_path) -> std::string {
        py::gil_scoped_release release;
        return File::absolutePath(path, current_path);
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

    m.def("play_auth_invoker_do", [](const py::capsule &cap, const std::string &err) {
        // 执行c++代码时释放gil锁
        py::gil_scoped_release release;
        auto &invoker = to_native<Broadcast::AuthInvoker>(cap);
        invoker(err);
    });

    m.def("rtsp_get_realm_invoker_do", [](const py::capsule &cap, const std::string &realm) {
        // 执行c++代码时释放gil锁
        py::gil_scoped_release release;
        auto &invoker = to_native<RtspSession::onGetRealm>(cap);
        invoker(realm);
    });

    m.def("rtsp_auth_invoker_do", [](const py::capsule &cap, bool encrypted, const std::string &pwd_or_md5) {
        // 执行c++代码时释放gil锁
        py::gil_scoped_release release;
        auto &invoker = to_native<RtspSession::onAuth>(cap);
        invoker(encrypted, pwd_or_md5);
    });

    m.def("close_player_invoker_do", [](const py::capsule &cap) {
        // 执行c++代码时释放gil锁
        py::gil_scoped_release release;
        auto &invoker = to_native<std::function<void()>>(cap);
        invoker();
    });

    m.def("http_access_invoker_do", [](const py::capsule &cap, const std::string &errMsg,const std::string &accessPath, int cookieLifeSecond) {
        // 执行c++代码时释放gil锁
        py::gil_scoped_release release;
        auto &invoker = to_native<HttpSession::HttpAccessPathInvoker>(cap);
        invoker(errMsg, accessPath, cookieLifeSecond);
    });

    m.def("set_fastapi", [](const py::object &check_route, const py::object &submit_coro) {
        static void *fastapi_tag = nullptr;
        NoticeCenter::Instance().delListener(&fastapi_tag, Broadcast::kBroadcastHttpRequest);
        NoticeCenter::Instance().addListener(&fastapi_tag, Broadcast::kBroadcastHttpRequest, [check_route, submit_coro](BroadcastHttpRequestArgs) {
            handle_http_request(check_route, submit_coro, parser, invoker, consumed, sender);
        });
    });

    py::enum_<TrackType>(m, "TrackType")
        .value("Invalid", TrackInvalid)
        .value("Video", TrackVideo)
        .value("Audio", TrackAudio)
        .value("Title", TrackTitle)
        .value("Application", TrackApplication)
        .export_values();

    py::class_<MediaSource, MediaSource::Ptr>(m, "MediaSource")
        .def("getSchema", &MediaSource::getSchema)
        .def("getUrl", &MediaSource::getUrl)
        .def("getMediaTuple", &MediaSource::getMediaTuple)
        .def("getTimeStamp", &MediaSource::getTimeStamp)
        .def("setTimeStamp", &MediaSource::setTimeStamp)
        .def("getBytesSpeed", &MediaSource::getBytesSpeed)
        .def("getTotalBytes", &MediaSource::getTotalBytes)
        .def("getCreateStamp", &MediaSource::getCreateStamp)
        .def("getAliveSecond", &MediaSource::getAliveSecond)
        .def("readerCount", &MediaSource::readerCount)
        .def("totalReaderCount", &MediaSource::totalReaderCount)
        .def("getOriginType", &MediaSource::getOriginType)
        .def("getOriginUrl", &MediaSource::getOriginUrl)
        .def("getOriginSock", &MediaSource::getOriginSock)
        .def("seekTo", &MediaSource::seekTo)
        .def("pause", &MediaSource::pause)
        .def("speed", &MediaSource::speed)
        .def("close", &MediaSource::close)
        .def("setupRecord", &MediaSource::setupRecord)
        .def("isRecording", &MediaSource::isRecording)
        .def("stopSendRtp", &MediaSource::stopSendRtp)
        .def("getLossRate", &MediaSource::getLossRate)
        .def("getMuxer", &MediaSource::getMuxer);

    py::class_<MediaTuple, std::shared_ptr<MediaTuple>>(m, "MediaTuple")
        .def_readwrite("vhost", &MediaTuple::vhost)
        .def_readwrite("app", &MediaTuple::app)
        .def_readwrite("stream", &MediaTuple::stream)
        .def_readwrite("params", &MediaTuple::params)
        .def("shortUrl", &MediaTuple::shortUrl);

    py::class_<SockException, std::shared_ptr<SockException>>(m, "SockException").def("what", &SockException::what).def("code", &SockException::getErrCode);

    py::class_<Parser, std::shared_ptr<Parser>>(m, "Parser")
        .def("method", &Parser::method)
        .def("url", &Parser::url)
        .def("status", &Parser::status)
        .def("fullUrl", &Parser::fullUrl)
        .def("protocol", &Parser::protocol)
        .def("statusStr", &Parser::statusStr)
        .def("content", &Parser::content)
        .def("params", &Parser::params)
        .def("getHeader", [](Parser *thiz) {
            py::dict ret;
            for (auto &pr : thiz->getHeader()) {
                ret[pr.first.data()] = pr.second;
            }
            return ret;
        });

    py::enum_<Recorder::type>(m, "RecordType")
        .value("hls", Recorder::type_hls)
        .value("mp4", Recorder::type_mp4)
        .value("hls_fmp4", Recorder::type_hls_fmp4)
        .value("fmp4", Recorder::type_fmp4)
        .value("ts", Recorder::type_ts)
        .export_values();

#define OPT(key) .def_readwrite(#key, &ProtocolOption::key)
    py::class_<ProtocolOption, std::shared_ptr<ProtocolOption>>(m, "ProtocolOption") OPT_VALUE(OPT);
#undef OPT

    py::class_<MultiMediaSourceMuxer, std::shared_ptr<MultiMediaSourceMuxer>>(m, "MultiMediaSourceMuxer")
        .def("totalReaderCount", static_cast<int (MultiMediaSourceMuxer::*)() const>(&MultiMediaSourceMuxer::totalReaderCount))
        .def("isEnabled", &MultiMediaSourceMuxer::isEnabled)
        .def("setupRecord", &MultiMediaSourceMuxer::setupRecord)
        .def("startRecord", &MultiMediaSourceMuxer::startRecord)
        .def("isRecording", &MultiMediaSourceMuxer::isRecording)
        .def("startSendRtp", &MultiMediaSourceMuxer::startSendRtp)
        .def("stopSendRtp", &MultiMediaSourceMuxer::stopSendRtp)
        .def("getOption", &MultiMediaSourceMuxer::getOption)
        .def("getMediaTuple", &MultiMediaSourceMuxer::getMediaTuple);

    py::class_<Track, Track::Ptr>(m, "Track")
        .def("getCodecId", &Track::getCodecId)
        .def("getCodecName", &Track::getCodecName)
        .def("getTrackType", &Track::getTrackType)
        .def("getTrackTypeStr", &Track::getTrackTypeStr)
        .def("setIndex", &Track::setIndex)
        .def("getIndex", &Track::getIndex)
        .def("getVideoKeyFrames", &Track::getVideoKeyFrames)
        .def("getFrames", &Track::getFrames)
        .def("getVideoGopSize", &Track::getVideoGopSize)
        .def("getVideoGopInterval", &Track::getVideoGopInterval)
        .def("getDuration", &Track::getDuration)
        .def("ready", &Track::ready)
        .def("update", &Track::update)
        .def("getSdp", &Track::getSdp)
        .def("getExtraData", &Track::getExtraData)
        .def("setExtraData", &Track::setExtraData)
        .def("getBitRate", &Track::getBitRate)
        .def("setBitRate", &Track::setBitRate)
        .def("getVideoHeight",[](Track *thiz) {
            auto ptr = dynamic_cast<VideoTrack *>(thiz);
            return ptr ? ptr->getVideoHeight() : 0;
        })
        .def("getVideoWidth", [](Track *thiz) {
            auto ptr = dynamic_cast<VideoTrack *>(thiz);
            return ptr ? ptr->getVideoWidth() : 0;
        })
        .def("getVideoFps", [](Track *thiz) {
            auto ptr = dynamic_cast<VideoTrack *>(thiz);
            return ptr ? ptr->getVideoFps() : 0;
        })
        .def("getAudioSampleRate",[](Track *thiz) {
            auto ptr = dynamic_cast<AudioTrack *>(thiz);
            return ptr ? ptr->getAudioSampleRate() : 0;
        })
        .def("getAudioSampleBit", [](Track *thiz) {
            auto ptr = dynamic_cast<AudioTrack *>(thiz);
            return ptr ? ptr->getAudioSampleBit() : 0;
        })
        .def("getAudioChannel", [](Track *thiz) {
            auto ptr = dynamic_cast<AudioTrack *>(thiz);
            return ptr ? ptr->getAudioChannel() : 0;
        });

    py::class_<Frame, Frame::Ptr>(m, "Frame")
        .def("data", &Frame::data)
        .def("size", &Frame::size)
        .def("toString", &Frame::toString)
        .def("getCapacity", &Frame::getCapacity)
        .def("getCodecId", &Frame::getCodecId)
        .def("getCodecName", &Frame::getCodecName)
        .def("getTrackType", &Frame::getTrackType)
        .def("getTrackTypeStr", &Frame::getTrackTypeStr)
        .def("setIndex", &Frame::setIndex)
        .def("getIndex", &Frame::getIndex)
        .def("dts", &Frame::dts)
        .def("pts", &Frame::pts)
        .def("prefixSize", &Frame::prefixSize)
        .def("keyFrame", &Frame::keyFrame)
        .def("configFrame", &Frame::configFrame)
        .def("cacheAble", &Frame::cacheAble)
        .def("dropAble", &Frame::dropAble)
        .def("decodeAble", &Frame::decodeAble);
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

static std::shared_ptr<PythonInvoker> g_instance;

PythonInvoker &PythonInvoker::Instance() {
    static toolkit::onceToken s_token([]() {
        g_instance.reset(new PythonInvoker);
    });

    return *g_instance;
}

void PythonInvoker::release() {
    g_instance = nullptr;
}

PythonInvoker::PythonInvoker() {
    // 确保日志一直可用
    _logger = Logger::Instance().shared_from_this();
    set_python_path(); // 确保 PYTHONPATH 在第一次调用时设置
    _interpreter = new py::scoped_interpreter;
    _rel = new py::gil_scoped_release;

    NoticeCenter::Instance().addListener(this, Broadcast::kBroadcastReloadConfig, [this] (BroadcastReloadConfigArgs) {
        py::gil_scoped_acquire guard;
        if (_on_reload_config) {
            _on_reload_config();
        }
    });

    NoticeCenter::Instance().addListener(this, Broadcast::kBroadcastCreateMuxer, [this](BroadcastCreateMuxerArgs) {
        py::gil_scoped_acquire guard;
        if (_on_create_muxer) {
            auto py_muxer = _on_create_muxer(sender);
            if (py_muxer && !py_muxer.is_none()) {
                delegate = std::make_shared<MuxerDelegatePython>(std::move(py_muxer));
            }
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
        _on_exit = py::function();
        _on_publish = py::function();
        _on_play = py::function();
        _on_flow_report = py::function();
        _on_reload_config = py::function();
        _on_media_changed = py::function();
        _on_player_proxy_failed = py::function();
        _on_get_rtsp_realm = py::function();
        _on_rtsp_auth = py::function();
        _on_stream_not_found = py::function();
        _on_record_mp4 = py::function();
        _on_record_ts = py::function();
        _on_stream_none_reader = py::function();
        _on_send_rtp_stopped = py::function();
        _on_http_access = py::function();
        _on_rtp_server_timeout = py::function();
        _on_create_muxer = py::function();
        _module = py::module();
    }
    delete _rel;
    delete _interpreter;
}

#define GET_FUNC(instance, name) \
    if (hasattr(instance, #name)) { \
        _##name = instance.attr(#name); \
    }

void PythonInvoker::load(const std::string &module_name) {
    try {
        py::gil_scoped_acquire gil; // 加锁
        _module = py::module::import(module_name.c_str());
        GET_FUNC(_module, on_exit);
        GET_FUNC(_module, on_publish);
        GET_FUNC(_module, on_play);
        GET_FUNC(_module, on_flow_report);
        GET_FUNC(_module, on_reload_config);
        GET_FUNC(_module, on_media_changed);
        GET_FUNC(_module, on_player_proxy_failed);
        GET_FUNC(_module, on_get_rtsp_realm);
        GET_FUNC(_module, on_rtsp_auth);
        GET_FUNC(_module, on_stream_not_found);
        GET_FUNC(_module, on_record_mp4);
        GET_FUNC(_module, on_record_ts);
        GET_FUNC(_module, on_stream_none_reader);
        GET_FUNC(_module, on_send_rtp_stopped);
        GET_FUNC(_module, on_http_access);
        GET_FUNC(_module, on_rtp_server_timeout);
        GET_FUNC(_module, on_create_muxer);

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

bool PythonInvoker::on_media_changed(BroadcastMediaChangedArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_media_changed) {
        return false;
    }
    return _on_media_changed(bRegist, to_python_ref(sender)).cast<bool>();
}

bool PythonInvoker::on_player_proxy_failed(BroadcastPlayerProxyFailedArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_player_proxy_failed) {
        return false;
    }
    return _on_player_proxy_failed(sender.getUrl(), to_python_ref(sender.getMediaTuple()), to_python_ref(ex)).cast<bool>();
}

bool PythonInvoker::on_get_rtsp_realm(BroadcastOnGetRtspRealmArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_get_rtsp_realm) {
        return false;
    }
    return _on_get_rtsp_realm(to_python(args), to_python(invoker), to_python(sender)).cast<bool>();
}

bool PythonInvoker::on_rtsp_auth(BroadcastOnRtspAuthArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_rtsp_auth) {
        return false;
    }
    return _on_rtsp_auth(to_python(args), realm, user_name, must_no_encrypt, to_python(invoker), to_python(sender)).cast<bool>();
}

bool PythonInvoker::on_stream_not_found(BroadcastNotFoundStreamArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_stream_not_found) {
        return false;
    }
    return _on_stream_not_found(to_python(args), to_python(sender), to_python(closePlayer)).cast<bool>();
}

bool PythonInvoker::on_record_mp4(BroadcastRecordMP4Args) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_record_mp4) {
        return false;
    }
    return _on_record_mp4(to_python(info)).cast<bool>();
}

bool PythonInvoker::on_record_ts(BroadcastRecordTsArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_record_ts) {
        return false;
    }
    return _on_record_ts(to_python(info)).cast<bool>();
}

bool PythonInvoker::on_stream_none_reader(BroadcastStreamNoneReaderArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_stream_none_reader) {
        return false;
    }
    return _on_stream_none_reader(to_python_ref(sender)).cast<bool>();
}

bool PythonInvoker::on_send_rtp_stopped(BroadcastSendRtpStoppedArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_send_rtp_stopped) {
        return false;
    }
    return _on_send_rtp_stopped(to_python_ref(sender), ssrc, to_python_ref(ex)).cast<bool>();
}

bool PythonInvoker::on_http_access(BroadcastHttpAccessArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_http_access) {
        return false;
    }
    return _on_http_access(to_python_ref(parser), path, is_dir, to_python(invoker), to_python(sender)).cast<bool>();
}

bool PythonInvoker::on_rtp_server_timeout(BroadcastRtpServerTimeoutArgs) const {
    py::gil_scoped_acquire gil; // 确保在 Python 调用期间持有 GIL
    if (!_on_rtp_server_timeout) {
        return false;
    }
    return _on_rtp_server_timeout(local_port, to_python_ref(tuple), tcp_mode, re_use_port, ssrc).cast<bool>();
}

} // namespace mediakit

#endif
