import inspect

try:
    import mk_loader
    USE_PLUGIN_LOGGER = True
except ImportError:
    USE_PLUGIN_LOGGER = False

def _do_log(level: int, *args):
    frame_info = inspect.stack()[2]
    filename = frame_info.filename
    lineno = frame_info.lineno
    funcname = frame_info.function

    # 把所有参数转成字符串后用空格拼接
    msg = " ".join(str(arg) for arg in args)

    if USE_PLUGIN_LOGGER:
        mk_loader.log(level, filename, lineno, funcname, msg)
    else:
        print(f"[{filename}:{lineno}] {funcname} | {msg}")

def log_trace(*args): _do_log(0, *args)
def log_debug(*args): _do_log(1, *args)
def log_info(*args):  _do_log(2, *args)
def log_warn(*args):  _do_log(3, *args)
def log_error(*args): _do_log(4, *args)
