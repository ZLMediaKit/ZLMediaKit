
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
#include "Rtsp/RtspSession.h"

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
    bool on_get_rtsp_realm(BroadcastOnGetRtspRealmArgs) const;
    bool on_rtsp_auth(BroadcastOnRtspAuthArgs) const;
    bool on_stream_not_found(BroadcastNotFoundStreamArgs) const;

private:
    PythonInvoker();

private:
    py::gil_scoped_release *_rel;
    py::scoped_interpreter *_interpreter;
    std::shared_ptr<toolkit::Logger> _logger;
    py::module _module;

    // 程序退出
    py::function _on_exit;
    // 推流鉴权
    py::function _on_publish;
    // 播放鉴权
    py::function _on_play;
    // 流量汇报接口
    py::function _on_flow_report;
    // 配置文件热更新回调
    py::function _on_reload_config;
    // 媒体注册注销
    py::function _on_media_changed;
    // 拉流代理失败
    py::function _on_player_proxy_failed;
    // rtsp播放是否开启专属鉴权
    py::function _on_get_rtsp_realm;
    // rtsp播放或推流鉴权回调
    py::function _on_rtsp_auth;
    // 播放一个不存在的流时触发
    py::function _on_stream_not_found;


};

} // namespace mediakit

#endif
#endif // PYINVOKER_H