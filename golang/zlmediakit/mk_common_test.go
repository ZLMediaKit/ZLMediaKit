package zlmediakit

import (
	"github.com/stretchr/testify/require"
	"testing"
	"time"
)

func TestCommonEnvInit(t *testing.T) {
	conf := EnvInit(
		0, LTrace, LogConsole|LogCallback|LogFile,
		"log", 1, true, "../../conf/config.ini", false, "../../default.pem", "")

	require.Equal(t, 0, conf.ThreadNum())
	require.Equal(t, LTrace, conf.LogLevel())
	require.Equal(t, LogConsole|LogCallback|LogFile, conf.LogMask())
	require.Equal(t, "log", conf.LogFilePath())
	require.Equal(t, 1, conf.LogFileDays())
	require.Equal(t, true, conf.IniIsPath())
	require.Equal(t, "../../conf/config.ini", conf.Ini())
	require.Equal(t, false, conf.SslIsPath())
	require.Equal(t, "../../default.pem", conf.Ssl())
	require.Equal(t, "", conf.SslPwd())
}

func TestCommonSetLog(t *testing.T) {
	SetLog(1, 1)
}

func TestCommonServer(t *testing.T) {
	p, err := HttpServerStart(1180, false)
	require.Nil(t, err)
	require.Equal(t, uint16(1180), p)

	p, err = RtspServerStart(11935, false)
	require.Nil(t, err)
	require.Equal(t, uint16(11935), p)

	p, err = RtmpServerStart(11554, false)
	require.Nil(t, err)
	require.Equal(t, uint16(11554), p)

	p, err = RtpServerStart(11111)
	require.Nil(t, err)
	require.Equal(t, uint16(11111), p)

	p, err = RtcServerStart(11222) // 未启用webrtc功能
	require.NotNil(t, err)
	//require.Equal(t, uint16(11222), p)

	p, err = SrtServerStart(11333)
	require.Nil(t, err)
	require.Equal(t, uint16(11333), p)

	p, err = ShellServerStart(11444)
	require.Nil(t, err)
	require.Equal(t, uint16(11444), p)

	<-time.After(time.Minute * 5)
	StopAllServer()
}
