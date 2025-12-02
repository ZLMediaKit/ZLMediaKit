import mk_logger
import mk_loader

def on_start():
    mk_logger.log_info("on_start")


def on_exit():
    mk_logger.log_info("on_exit")


def on_publish(type: str, info, invoker, sender) -> bool:
    mk_logger.log_info(f"on_publish: {type}")
    # opt 控制转协议，请参考配置文件[protocol]下字段
    opt = {
        "enable_rtmp": "1"
    }
    # 响应推流鉴权结果
    mk_loader.mk_publish_auth_invoker_do(invoker, "", opt);
    # 返回True代表此事件被python拦截
    return True
