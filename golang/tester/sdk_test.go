package tester

import (
	"testing"
	"time"
	"zlmediakit/zlmediakit"
)

func TestServer(t *testing.T) {
	zlmediakit.MK_env_init2(0, zlmediakit.LTrace, zlmediakit.LOG_CONSOLE|zlmediakit.LOG_CALLBACK|zlmediakit.LOG_FILE,
		"log", 7, true, "../../conf/config.ini", true, "../../default.pem", "")
	zlmediakit.MK_http_server_start(80, false)
	zlmediakit.MK_http_server_start(443, true)
	time.Sleep(1000 * time.Second)
}
