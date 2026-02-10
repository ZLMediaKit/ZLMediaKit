import mk_logger
import mk_loader
import asyncio
import threading
from StreamUI.backend.main import app
from starlette.routing import Match

def start_background_loop(loop):
    asyncio.set_event_loop(loop)
    loop.run_forever()

loop = asyncio.new_event_loop()
threading.Thread(target=start_background_loop, args=(loop,), daemon=True).start()

def submit_coro(scope, body, send):
    async def run():
        # 包装 send 函数，确保它总是可等待的
        async def async_send(message):
            # 调用原始的 send 函数，它现在应该返回一个协程
            result = send(message)
            if result is not None:
                await result

        async def receive():
            return {
                "type": "http.request",
                "body": body,
                "more_body": False,
            }

        try:
            await app(scope, receive, async_send)
        except Exception as e:
            mk_logger.log_warn(f"FastAPI failed: {e}")
            # 发送错误响应
            await async_send({
                "type": "http.response.start",
                "status": 500,
                "headers": [(b"content-type", b"text/plain")],
            })
            await async_send({
                "type": "http.response.body",
                "body": b"Internal Server Error",
                "more_body": False,
            })
    return asyncio.run_coroutine_threadsafe(run(), loop)

def check_route(scope) -> bool:
    for route in app.routes:
        if hasattr(route, "matches"):
            match, _ = route.matches(scope)
            if match == Match.FULL:
                return True
    return False

def on_start():
    mk_logger.log_info(f"on_start, secret: {mk_loader.get_config('api.secret')}")
    # mk_loader.set_config('api.secret', "new_secret_from_python")
    # mk_loader.update_config()
    mk_loader.set_fastapi(check_route, submit_coro)

def on_exit():
    mk_logger.log_info("on_exit")

def on_publish(type: str, args: dict, invoker, sender: dict) -> bool:
    mk_logger.log_info(f"type: {type}, args: {args}, sender: {sender}")
    # opt 控制转协议，请参考配置文件[protocol]下字段
    opt = {
        "enable_rtmp": "1"
    }
    # 响应推流鉴权结果
    mk_loader.publish_auth_invoker_do(invoker, "", opt)
    # 返回True代表此事件被python拦截
    return True

def on_play(args: dict, invoker, sender: dict) -> bool:
    mk_logger.log_info(f"args: {args}, sender: {sender}")
    # 响应播放鉴权结果
    mk_loader.play_auth_invoker_do(invoker, "")
    # 返回True代表此事件被python拦截
    return True

def on_flow_report(args: dict, totalBytes: int, totalDuration: int, isPlayer: bool, sender: dict) -> bool:
    mk_logger.log_info(f"args: {args}, totalBytes: {totalBytes}, totalDuration: {totalDuration}, isPlayer: {isPlayer}, sender: {sender}")
    # 返回True代表此事件被python拦截
    return True

def on_media_changed(is_register: bool, sender: mk_loader.MediaSource) -> bool:
    mk_logger.log_info(f"is_register: {is_register}, sender: {sender.getUrl()}")
    # 该事件在c++中也处理下
    return False

def on_player_proxy_failed(url: str, media_tuple: mk_loader.MediaTuple , ex: mk_loader.SockException) -> bool:
    mk_logger.log_info(f"on_player_proxy_failed: {url}, {media_tuple.shortUrl()}, {ex.what()}")
    # 该事件在c++中也处理下
    return False

def on_get_rtsp_realm(args: dict, invoker, sender: dict) -> bool:
    mk_logger.log_info(f"on_get_rtsp_realm, args: {args}, sender: {sender}")
    mk_loader.rtsp_get_realm_invoker_do(invoker, "zlmediakit")
    # 返回True代表此事件被python拦截
    return True

def on_rtsp_auth(args: dict, realm: str, user_name: str, must_no_encrypt: bool, invoker, sender:dict) -> bool:
    mk_logger.log_info(f"on_rtsp_auth, args: {args}, realm: {realm}, user_name: {user_name}, must_no_encrypt: {must_no_encrypt}, sender: {sender}")
    mk_loader.rtsp_auth_invoker_do(invoker, False, "zlmediakit")
    # 返回True代表此事件被python拦截
    return True

def on_stream_not_found(args: dict, sender:dict, invoker) -> bool:
    mk_logger.log_info(f"on_stream_not_found, args: {args}, sender: {sender}")
    # 立即通知播放器流不存在并关闭
    mk_loader.close_player_invoker_do(invoker)
    # 返回True代表此事件被python拦截
    return True

def on_record_mp4(info: dict) -> bool:
    mk_logger.log_info(f"on_record_mp4, info: {info}")
    # 返回True代表此事件被python拦截
    return True
def on_record_ts(info: dict) -> bool:
    mk_logger.log_info(f"on_record_ts, info: {info}")
    # 返回True代表此事件被python拦截
    return True

def on_stream_none_reader(sender: mk_loader.MediaSource) -> bool:
    mk_logger.log_info(f"on_stream_none_reader: {sender.getUrl()}")
    # 无人观看自动关闭
    sender.close(False)
    # 返回True代表此事件被python拦截
    return True

def on_send_rtp_stopped(sender: mk_loader.MultiMediaSourceMuxer, ssrc: str, ex: mk_loader.SockException) -> bool:
    mk_logger.log_info(f"on_send_rtp_stopped, ssrc: {ssrc}, ex: {ex.what()}, url: {sender.getMediaTuple().getUrl()}")
    # 返回True代表此事件被python拦截
    return True

def on_http_access(parser: mk_loader.Parser, path: str, is_dir: bool, invoker, sender: dict) -> bool:
    mk_logger.log_info(f"on_http_access, path: {path}, is_dir: {is_dir}, sender: {sender}, http header: {parser.getHeader()}")
    # 允许访问该文件/目录1小时, cookie有效期内，访问该目录下的文件或路径不再触发该事件
    mk_loader.http_access_invoker_do(invoker, "", path, 60 * 60)
    # 返回True代表此事件被python拦截
    return True

def on_rtp_server_timeout(local_port: int, tuple: mk_loader.MediaTuple, tcp_mode: int, re_use_port: bool, ssrc: int) -> bool:
    mk_logger.log_info(f"on_rtp_server_timeout, local_port: {local_port}, tuple: {tuple.shortUrl()}, tcp_mode: {tcp_mode}, re_use_port: {re_use_port}, ssrc: {ssrc}")
    # 返回True代表此事件被python拦截
    return True

def on_reload_config():
    mk_logger.log_info(f"on_reload_config")

class PyMultiMediaSourceMuxer:
    def __init__(self, sender: mk_loader.MultiMediaSourceMuxer):
        mk_logger.log_info(f"PyMultiMediaSourceMuxer: {sender.getMediaTuple().shortUrl()}")
    def destroy(self):
        mk_logger.log_info(f"~PyMultiMediaSourceMuxer")

    def addTrack(self, track: mk_loader.Track):
        mk_logger.log_info(f"addTrack: {track.getCodecName()}")
        return True
    def addTrackCompleted(self):
        mk_logger.log_info(f"addTrackCompleted")
    def inputFrame(self, frame: mk_loader.Frame):
        mk_logger.log_info(f"inputFrame: {frame.getCodecName()} {frame.dts()}")
        return True
def on_create_muxer(sender: mk_loader.MultiMediaSourceMuxer):
    return PyMultiMediaSourceMuxer(sender)