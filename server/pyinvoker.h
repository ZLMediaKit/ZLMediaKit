
#ifndef PYINVOKER_H
#define PYINVOKER_H

#if defined(ENABLE_PYTHON)

#include <map>
#include <string>
#include <pybind11/embed.h>
#include <pybind11/numpy.h>
#include "Util/logger.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Player/PlayerProxy.h"

namespace py = pybind11;

namespace mediakit {

class PythonInvoker : public std::enable_shared_from_this<PythonInvoker>{
public:
    ~PythonInvoker();

    static PythonInvoker& Instance();
    static void release();

    void load(const std::string &module_name);
    bool on_publish(BroadcastMediaPublishArgs) const;
    bool on_play(BroadcastMediaPlayedArgs) const;
    bool on_flow_report(BroadcastFlowReportArgs) const;
    bool on_media_changed(BroadcastMediaChangedArgs) const;
    bool on_player_proxy_failed(BroadcastPlayerProxyFailedArgs) const;

private:
    PythonInvoker();

private:
    py::gil_scoped_release *_rel;
    py::scoped_interpreter *_interpreter;
    std::shared_ptr<toolkit::Logger> _logger;
    py::module _module;

    // 程序退出
    py::object _on_exit;
    // 推流鉴权
    py::object _on_publish;
    // 播放鉴权
    py::object _on_play;
    // 流量汇报接口
    py::object _on_flow_report;
    // 配置文件热更新回调
    py::object _on_reload_config;
    // 媒体注册注销
    py::object _on_media_changed;
    // 拉流代理失败
    py::object _on_player_proxy_failed;
};

} // namespace mediakit

#endif
#endif // PYINVOKER_H