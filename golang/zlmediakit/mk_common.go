package zlmediakit

/*
#include "mk_mediakit.h"
#cgo CFLAGS: -I../../api/include
#cgo LDFLAGS: -L../../release/linux/Debug/ -lmk_api
*/
import "C"

type LOG_MASK int
type LOG_LEVEL int

const (
	LOG_CONSOLE  LOG_MASK = 1 << 0
	LOG_FILE     LOG_MASK = 1 << 1
	LOG_CALLBACK LOG_MASK = 2 << 1
)

const (
	LTrace LOG_LEVEL = 0
	LDebug LOG_LEVEL = 1
	LInfo  LOG_LEVEL = 2
	LWarn  LOG_LEVEL = 3
	LError LOG_LEVEL = 4
)

func btoi(b bool) int {
	if b {
		return 1
	}
	return 0
}

func MK_env_init2(thread_num int, log_level LOG_LEVEL, log_mask LOG_MASK, log_file_path string, log_file_days int, ini_is_path bool, ini string, ssl_is_path bool, ssl string, ssl_pwd string) {
	// 调用 C SDK 的函数
	C.mk_env_init2(C.int(thread_num), C.int(log_level), C.int(log_mask), C.CString(log_file_path), C.int(log_file_days), C.int(btoi(ini_is_path)), C.CString(ini), C.int(btoi(ssl_is_path)), C.CString(ssl), C.CString(ssl_pwd))
}

func MK_stop_all_server() {
	// 调用 C SDK 的函数
	C.mk_stop_all_server()
}

func MK_set_log(file_max_size, file_max_count int) {
	// 调用 C SDK 的函数
	C.mk_set_log(C.int(file_max_size), C.int(file_max_count))
}

func MK_http_server_start(port uint16, ssl bool) {
	C.mk_http_server_start(C.ushort(port), C.int(btoi(ssl)))
}
