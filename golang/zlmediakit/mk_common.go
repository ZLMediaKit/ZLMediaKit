package zlmediakit

//#include "mk_mediakit.h"
//#include "mk_common.h"
import "C"
import (
	"fmt"
	"zlmediakit/helper"
)

type LogMask int
type LogLevel int

const (
	LogConsole  LogMask = 1 << 0
	LogFile     LogMask = 1 << 1
	LogCallback LogMask = 2 << 1
)

const (
	LTrace LogLevel = 0
	LDebug LogLevel = 1
	LInfo  LogLevel = 2
	LWarn  LogLevel = 3
	LError LogLevel = 4
)

type Config struct {
	c *C.mk_config
}

func newConfigFromC(c *C.mk_config) *Config {
	if c == nil {
		return nil
	}
	return &Config{c: c}
}

func (conf *Config) ThreadNum() int {
	return int(conf.c.thread_num)
}

func (conf *Config) SetThreadNum(threadNum int) {
	conf.c.thread_num = C.int(threadNum)
}

func (conf *Config) LogLevel() LogLevel {
	return LogLevel(conf.c.log_level)
}

func (conf *Config) SetLogLevel(logLevel LogLevel) {
	conf.c.log_level = C.int(logLevel)
}

func (conf *Config) LogMask() LogMask {
	return LogMask(conf.c.log_mask)
}

func (conf *Config) SetLogMask(logMask LogMask) {
	conf.c.log_mask = C.int(logMask)
}

func (conf *Config) LogFilePath() string {
	return C.GoString(conf.c.log_file_path)
}

func (conf *Config) SetLogFilePath(logFilePath string) {
	logFilePathC := C.CString(logFilePath)
	conf.c.log_file_path = logFilePathC
}

func (conf *Config) LogFileDays() int {
	return int(conf.c.log_file_days)
}

func (conf *Config) SetLogFileDays(logFileDays int) {
	conf.c.log_file_days = C.int(logFileDays)
}

func (conf *Config) IniIsPath() bool {
	return int(conf.c.ini_is_path) == 1
}

func (conf *Config) SetIniIsPath(iniIsPath bool) {
	conf.c.ini_is_path = C.int(helper.Bool2Int(iniIsPath))
}

func (conf *Config) Ini() string {
	return C.GoString(conf.c.ini)
}

func (conf *Config) SetIni(ini string) {
	iniC := C.CString(ini)
	conf.c.ini = iniC
}

func (conf *Config) Ssl() string {
	return C.GoString(conf.c.ssl)
}

func (conf *Config) SetSsl(ssl string) {
	sslC := C.CString(ssl)
	conf.c.ssl = sslC
}

func (conf *Config) SslIsPath() bool {
	return int(conf.c.ssl_is_path) == 1
}

func (conf *Config) SetSslIsPath(sslIsPath bool) {
	conf.c.ssl_is_path = C.int(helper.Bool2Int(sslIsPath))
}

func (conf *Config) SslPwd() string {
	return C.GoString(conf.c.ssl_pwd)
}

func (conf *Config) SetSslPwd(sslPwd string) {
	sslPwdC := C.CString(sslPwd)
	conf.c.ssl_pwd = sslPwdC
}

// EnvInit 初始化环境，调用该库前需要先调用此函数
//
// threadNum: 线程数
// logLevel: 日志级别,支持0~4
// logMask: 控制日志输出的掩模，请查看LOG_CONSOLE、LOG_FILE、LOG_CALLBACK等宏
// logFilePath: 文件日志保存路径,路径可以不存在(内部可以创建文件夹)，设置为NULL关闭日志输出至文件
// logFileDays: 文件日志保存天数,设置为0关闭日志文件
// iniIsPath: 配置文件是内容还是路径
// ini: 配置文件内容或路径，可以为空,如果该文件不存在，那么将导出默认配置至该文件
// sslIsPath: ssl证书是内容还是路径
// ssl: ssl证书内容或路径，可以为空
// sslPwd: 证书密码，可以为空
func EnvInit(threadNum int, logLevel LogLevel, logMask LogMask, logFilePath string, logFileDays int, iniIsPath bool, ini string, sslIsPath bool, ssl string, sslPwd string) *Config {
	var c C.mk_config
	conf := newConfigFromC(&c)

	conf.SetThreadNum(threadNum)
	conf.SetLogLevel(logLevel)
	conf.SetLogMask(logMask)
	conf.SetLogFilePath(logFilePath)
	conf.SetLogFileDays(logFileDays)
	conf.SetIniIsPath(iniIsPath)
	conf.SetIni(ini)
	conf.SetSsl(ssl)
	conf.SetSslIsPath(sslIsPath)
	conf.SetSslPwd(sslPwd)

	C.mk_env_init(conf.c)
	return conf
}

// StopAllServer 关闭所有服务器，请在main函数退出时调用
func StopAllServer() {
	C.mk_stop_all_server()
}

// SetLog 设置日志文件
//
// fileMaxSize 单个切片文件大小(MB)
// fileMaxCount 切片文件个数
func SetLog(fileMaxSize, fileMaxCount int) {
	C.mk_set_log(C.int(fileMaxSize), C.int(fileMaxCount))
}

// HttpServerStart 创建http[s]服务器
//
// port htt监听端口，推荐80，传入0则随机分配
// ssl 是否为ssl类型服务器
func HttpServerStart(port uint16, ssl bool) (uint16, error) {
	ret := C.mk_http_server_start(C.ushort(port), C.int(helper.Bool2Int(ssl)))
	i := uint16(ret)
	if i == 0 {
		return 0, fmt.Errorf("http server start fail")
	}
	return i, nil
}

// RtspServerStart 创建rtsp[s]服务器
//
// port rtsp监听端口，推荐554，传入0则随机分配
// ssl 是否为ssl类型服务器
func RtspServerStart(port uint16, ssl bool) (uint16, error) {
	ret := C.mk_rtsp_server_start(C.ushort(port), C.int(helper.Bool2Int(ssl)))
	i := uint16(ret)
	if i == 0 {
		return 0, fmt.Errorf("rtsp server start fail")
	}
	return i, nil
}

// RtmpServerStart 创建rtmp[s]服务器
//
// port rtmp监听端口，推荐1935，传入0则随机分配
// ssl 是否为ssl类型服务器
func RtmpServerStart(port uint16, ssl bool) (uint16, error) {
	ret := C.mk_rtmp_server_start(C.ushort(port), C.int(helper.Bool2Int(ssl)))
	i := uint16(ret)
	if i == 0 {
		return 0, fmt.Errorf("rtmp server start fail")
	}
	return i, nil
}

// RtpServerStart 创建rtp服务器
//
// port rtp监听端口(包括udp/tcp)
func RtpServerStart(port uint16) (uint16, error) {
	ret := C.mk_rtp_server_start(C.ushort(port))
	i := uint16(ret)
	if i == 0 {
		return 0, fmt.Errorf("rtp server start fail")
	}
	return i, nil
}

// RtcServerStart 创建rtc服务器
//
// port rtc监听端口
func RtcServerStart(port uint16) (uint16, error) {
	ret := C.mk_rtc_server_start(C.ushort(port))
	i := uint16(ret)
	if i == 0 {
		return 0, fmt.Errorf("rtc server start fail")
	}
	return i, nil
}

// todo mk_webrtc_get_answer_sdp
// todo mk_webrtc_get_answer_sdp2

// SrtServerStart 创建srt服务器
//
// port srt监听端口
func SrtServerStart(port uint16) (uint16, error) {
	ret := C.mk_srt_server_start(C.ushort(port))
	i := uint16(ret)
	if i == 0 {
		return 0, fmt.Errorf("srt server start fail")
	}
	return i, nil
}

// ShellServerStart 创建shell服务器
//
// port shell监听端口
func ShellServerStart(port uint16) (uint16, error) {
	ret := C.mk_shell_server_start(C.ushort(port))
	i := uint16(ret)
	if i == 0 {
		return 0, fmt.Errorf("shell server start fail")
	}
	return i, nil
}
