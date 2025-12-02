import mk_logger
import mk_loader

def on_start():
    mk_logger.log_info("on_start")

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
