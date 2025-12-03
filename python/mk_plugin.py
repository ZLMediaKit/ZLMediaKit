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
    mk_logger.log_info(f"args: {type}, args: {args}, sender: {sender}")
    # opt 控制转协议，请参考配置文件[protocol]下字段
    opt = {
        "enable_rtmp": "1"
    }
    # 响应推流鉴权结果
    mk_loader.publish_auth_invoker_do(invoker, "", opt);
    # 返回True代表此事件被python拦截
    return True

def on_play(args: dict, invoker, sender: dict) -> bool:
    mk_logger.log_info(f"args: {args}, sender: {sender}")
    # 响应播放鉴权结果
    mk_loader.auth_invoker_do(invoker, "");
    # 返回True代表此事件被python拦截
    return True

def on_flow_report(args: dict, totalBytes: int, totalDuration: int, isPlayer: bool, sender: dict) -> bool:
    mk_logger.log_info(f"args: {args}, totalBytes: {totalBytes}, totalDuration: {totalDuration}, isPlayer: {isPlayer}, sender: {sender}")
    # 返回True代表此事件被python拦截
    return True

def on_reload_config():
    mk_logger.log_info(f"on_reload_config")