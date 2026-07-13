
#ifndef PYINVOKER_H
#define PYINVOKER_H

#if defined(ENABLE_PYTHON)

#include <map>
#include <string>
#include "Util/logger.h"
#include "Common/config.h"
#include "Common/MediaSource.h"
#include "Player/PlayerProxy.h"
#include "Rtsp/RtspSession.h"
#include "Http/HttpSession.h"

namespace mediakit {
// 内部实现前置声明
struct PythonInvokerImpl;

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
    bool on_record_mp4(BroadcastRecordMP4Args) const;
    bool on_record_ts(BroadcastRecordTsArgs) const;
    bool on_stream_none_reader(BroadcastStreamNoneReaderArgs) const;
    bool on_send_rtp_stopped(BroadcastSendRtpStoppedArgs) const;
    bool on_http_access(BroadcastHttpAccessArgs) const;
    bool on_rtp_server_timeout(BroadcastRtpServerTimeoutArgs) const;

private:
    PythonInvoker();
    std::unique_ptr<PythonInvokerImpl> _impl;
};

} // namespace mediakit

#endif
#endif // PYINVOKER_H
