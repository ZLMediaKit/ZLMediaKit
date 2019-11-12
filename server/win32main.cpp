/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include <map>
#include <signal.h>
#include <stdio.h>
#include <iostream>
#include "Util/MD5.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Util/SSLBox.h"
#include "Util/onceToken.h"
#include "Util/CMD.h"
#include "Network/TcpServer.h"
#include "Poller/EventPoller.h"
#include "Common/config.h"
#include "Rtsp/UDPServer.h"
#include "Rtsp/RtspSession.h"
#include "Rtmp/RtmpSession.h"
#include "Shell/ShellSession.h"
#include "Rtmp/FlvMuxer.h"
#include "Player/PlayerProxy.h"
#include "Http/WebSocketSession.h"
#include "WebApi.h"
#include "WebHook.h"

#include "MiniDumper.h"
#include "MediaServer.h"

using namespace std;
using namespace toolkit;
using namespace mediakit;

namespace mediakit {
typedef enum { rConsole = 0, rServer, rInstall, rUninstall} RunMode;
////////////HTTP配置///////////
namespace Http {
#define HTTP_FIELD "http."
#define HTTP_PORT 10080
const string kPort = HTTP_FIELD"port";
#define HTTPS_PORT 10443
const string kSSLPort = HTTP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = HTTP_PORT;
    mINI::Instance()[kSSLPort] = HTTPS_PORT;
},nullptr);
}//namespace Http

////////////SHELL配置///////////
namespace Shell {
#define SHELL_FIELD "shell."
#define SHELL_PORT 9000
const string kPort = SHELL_FIELD"port";
onceToken token1([](){
    mINI::Instance()[kPort] = SHELL_PORT;
},nullptr);
} //namespace Shell

////////////RTSP服务器配置///////////
namespace Rtsp {
#define RTSP_FIELD "rtsp."
#define RTSP_PORT 10554
#define RTSPS_PORT 322
const string kPort = RTSP_FIELD"port";
const string kSSLPort = RTSP_FIELD"sslport";
onceToken token1([](){
    mINI::Instance()[kPort] = RTSP_PORT;
    mINI::Instance()[kSSLPort] = RTSPS_PORT;
},nullptr);

} //namespace Rtsp

////////////RTMP服务器配置///////////
namespace Rtmp {
#define RTMP_FIELD "rtmp."
#define RTMP_PORT 10935
const string kPort = RTMP_FIELD"port";
onceToken token1([](){
    mINI::Instance()[kPort] = RTMP_PORT;
},nullptr);
} //namespace RTMP
}  // namespace mediakit

class CMD_main;

static MediaServer_ServerState sServerState = MediaServerStartingUpState;
const char* theXMLFilePath = 0;
char* theServerName = "";
static SERVICE_STATUS_HANDLE sServiceStatusHandle = 0;

static void ReportStatus(DWORD inCurrentState, DWORD inExitCode);
static void InstallService(char* inServiceName);
static void RemoveService(char *inServiceName);
static void RunAsService(char* inServiceName);
void WINAPI ServiceControl(DWORD);
void WINAPI ServiceMain(DWORD argc, LPTSTR *argv);

MediaServer_ServerState StartServer(CMD_main& cmd_main);
void RunServer();

static void signalHandler(int signo) {
	std::cerr << "Shutting down" << std::endl;
	sServerState = MediaServerShuttingDownState;
}

class CMD_main : public CMD {
public:
    CMD_main() {
        _parser.reset(new OptionParser(nullptr));
	
		(*_parser) << Option('r',/*该选项简称，如果是\x00则说明无简称*/
							"runmode",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
							Option::ArgRequired,/*该选项后面必须跟值*/
							to_string(rConsole).data(),/*该选项默认值*/
							true,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
							"控制台运行、服务方式运行、安装到服务中，从服务中卸载（0~3）",/*该选项说明文字*/
							nullptr);
		
        (*_parser) << Option('l',/*该选项简称，如果是\x00则说明无简称*/
                             "level",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(LTrace).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "日志等级,LTrace~LError(0~4)",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('m',/*该选项简称，如果是\x00则说明无简称*/
                             "max_day",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             "30",/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "日志最多保存天数",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('c',/*该选项简称，如果是\x00则说明无简称*/
                             "config",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             (exeDir() + "Config.ini").data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "配置文件路径",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('s',/*该选项简称，如果是\x00则说明无简称*/
                             "ssl",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             (exeDir() + "ssl.p12").data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "ssl证书文件或文件夹,支持p12/pem类型",/*该选项说明文字*/
                             nullptr);

        (*_parser) << Option('t',/*该选项简称，如果是\x00则说明无简称*/
                             "threads",/*该选项全称,每个选项必须有全称；不得为null或空字符串*/
                             Option::ArgRequired,/*该选项后面必须跟值*/
                             to_string(2*thread::hardware_concurrency()).data(),/*该选项默认值*/
                             false,/*该选项是否必须赋值，如果没有默认值且为ArgRequired时用户必须提供该参数否则将抛异常*/
                             "启动事件触发线程数",/*该选项说明文字*/
                             nullptr);
    }

    virtual ~CMD_main() {}
    virtual const char *description() const {
        return "主程序命令参数";
    }
};

//全局变量，在WebApi中用于保存配置文件用
string g_ini_file;
CMD_main cmd_main;

int main(int argc,char *argv[]) {
		bool notAService = false;
       
        try {
            cmd_main.operator()(argc, argv);
        } catch (std::exception &ex) {
            cout << ex.what() << endl;
            return -1;
        }

		g_ini_file = cmd_main["config"];
		RunMode rMode = (RunMode)cmd_main["runmode"].as<int>();
		
		switch (rMode) {
		case rConsole:
			notAService = true;
			break;
		case rServer:
			break;
		case rInstall:
			::InstallService("MediaServer");
			printf("Starting the MediaServer service...\n");
			::RunAsService("MediaServer");
			::exit(0);
			break;
		case rUninstall:
			printf("Removing the MediaServer service...\n");
			::RemoveService("MediaServer");
			::exit(0);
			break;
		default:
			::exit(-1);
			break;

		}
		
		if (notAService) {
			if (signal(SIGINT, signalHandler) == SIG_ERR) {
				std::cerr << "Couldn't install signal handler for SIGINT" << std::endl;
				::exit(-1);
			}
			if (signal(SIGTERM, signalHandler) == SIG_ERR) {
				std::cerr << "Couldn't install signal handler for SIGTERM" << std::endl;
				::exit(-1);
			}

			// If we're running off the command-line, don't do the service initiation crap.^M
			sServerState = ::StartServer(cmd_main); // No stats update interval for now
			if (sServerState != MediaServerFatalErrorState) {
				::RunServer();
				::exit(0);
			}
			::exit(-1);
		}
		SERVICE_TABLE_ENTRY dispatchTable[] =
		{
			{ "", ServiceMain },
			{ NULL, NULL }
		};

		// In case someone runs the server improperly, print out a friendly message.
		printf("MediaServer must either be started from the DOS Console\n");
		printf("using the -r command-line option, or using the Service Control Manager\n\n");
		printf("Waiting for the Service Control Manager to start MediaServer...\n");
		BOOL theErr = ::StartServiceCtrlDispatcher(dispatchTable);
		if (!theErr)
		{
			printf("Fatal Error: Couldn't start Service\n");
			::exit(-1);
		}
		return 0;
}

void __stdcall ServiceMain(DWORD /*argc*/, LPTSTR *argv)
{
	theServerName = argv[0];

	sServiceStatusHandle = ::RegisterServiceCtrlHandler(theServerName, &ServiceControl);
	if (sServiceStatusHandle == 0)
	{
		printf("Failure registering service handler");
		::exit(-1);
	}

	//
	// Report our status
	::ReportStatus(SERVICE_START_PENDING, NO_ERROR);

	// Start & Run the server - no stats update interval for now
	sServerState = ::StartServer(cmd_main);
	if (sServerState != MediaServerFatalErrorState)
	{
		::ReportStatus(SERVICE_RUNNING, NO_ERROR);
		::RunServer(); // This function won't return until the server has died

		//
		// Ok, server is done...
		::ReportStatus(SERVICE_STOPPED, NO_ERROR);
		::exit(0);
	}
	else {
		::ReportStatus(SERVICE_STOPPED, ERROR_BAD_COMMAND); // I dunno... report some error
		::exit(-1);
	}
}

void WINAPI ServiceControl(DWORD inControlCode)
{
	DWORD theStatusReport = SERVICE_START_PENDING;

	switch (inControlCode)
	{
		// Stop the service.
		//
	case SERVICE_CONTROL_STOP:
	case SERVICE_CONTROL_SHUTDOWN:
	{
		if (sServerState == MediaServerStartingUpState)
			break;

		// Signal the server to shut down.
		sServerState = MediaServerShuttingDownState;
		break;
	}
	case SERVICE_CONTROL_PAUSE:
	{
#if 0
		if (sServerState != RunningState)
			break;

		// Signal the server to refuse new connections.
		theState = qtssRefusingConnectionsState;
		if (theServer != NULL)
			theServer->SetValue(qtssSvrState, 0, &theState, sizeof(theState));
#endif
		break;
	}
	case SERVICE_CONTROL_CONTINUE:
	{
#if 0
		if (theState != qtssRefusingConnectionsState)
			break;

		// Signal the server to refuse new connections.
		theState = qtssRefusingConnectionsState;
		if (theServer != NULL)
			theServer->SetValue(qtssSvrState, 0, &theState, sizeof(theState));
#endif
		break;
	}
	case SERVICE_CONTROL_INTERROGATE:
		break; // Just update our status
	default:
		break;
	}




	// Convert a QTSS state to a Win32 Service state^M
	switch (sServerState)
	{
	case MediaServerStartingUpState:           theStatusReport = SERVICE_START_PENDING;    break;
	case MediaServerRunningState:              theStatusReport = SERVICE_RUNNING;          break;
	case MediaServerRefusingConnectionsState:  theStatusReport = SERVICE_PAUSED;           break;
	case MediaServerFatalErrorState:           theStatusReport = SERVICE_STOP_PENDING;     break;
	case MediaServerShuttingDownState:         theStatusReport = SERVICE_STOP_PENDING;     break;
	default:                            theStatusReport = SERVICE_RUNNING;          break;
	}

	printf("Reporting status from ServiceControl function\n");
	::ReportStatus(theStatusReport, NO_ERROR);
}

void ReportStatus(DWORD inCurrentState, DWORD inExitCode)
{
	static bool sFirstTime = true;
	static unsigned long sCheckpoint = 0;
	static SERVICE_STATUS sStatus;

	if (sFirstTime)
	{
		sFirstTime = false;

		// Setup the status structure
		sStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
		sStatus.dwCurrentState = SERVICE_START_PENDING;
		sStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_PAUSE_CONTINUE | SERVICE_ACCEPT_SHUTDOWN;
		//sStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP;
		sStatus.dwWin32ExitCode = 0;
		sStatus.dwServiceSpecificExitCode = 0;
		sStatus.dwCheckPoint = 0;
		sStatus.dwWaitHint = 0;
	}

	if (sStatus.dwCurrentState == SERVICE_START_PENDING)
		sStatus.dwCheckPoint = ++sCheckpoint;
	else
		sStatus.dwCheckPoint = 0;

	sStatus.dwCurrentState = inCurrentState;
	sStatus.dwServiceSpecificExitCode = inExitCode;
	BOOL theErr = SetServiceStatus(sServiceStatusHandle, &sStatus);
	if (theErr == 0)
	{
		DWORD theerrvalue = ::GetLastError();
	}
}

void RunAsService(char* inServiceName)
{
	SC_HANDLE   theService;
	SC_HANDLE   theSCManager;

	theSCManager = ::OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_ALL_ACCESS   // access required
	);
	if (!theSCManager)
		return;

	theService = ::OpenService(
		theSCManager,               // SCManager database
		inServiceName,               // name of service
		SERVICE_ALL_ACCESS);

	SERVICE_STATUS lpServiceStatus;

	if (theService)
	{
		const int kNotRunning = 1062;
		bool stopped = ::ControlService(theService, SERVICE_CONTROL_STOP, &lpServiceStatus);
		if (!stopped && ((int) ::GetLastError() != kNotRunning))
			printf("Stopping Service Error: %d\n", ::GetLastError());

		bool started = ::StartService(theService, 0, NULL);
		if (!started)
			printf("Starting Service Error: %d\n", ::GetLastError());

		::CloseServiceHandle(theService);
	}

	::CloseServiceHandle(theSCManager);
}

void InstallService(char* inServiceName)
{
	SC_HANDLE   theService;
	SC_HANDLE   theSCManager;

	TCHAR thePath[512];
	TCHAR theAppBaseName[512];
	TCHAR theQuotedPath[522];

	BOOL theErr = ::GetModuleFileName(NULL, thePath, 512);
	if (!theErr)
		return;

	char* pSlash = strrchr((char*)thePath, '\\');
	if (pSlash) {
		strcpy((char*)theAppBaseName, (const char*)pSlash + 1);
		*(pSlash + 1) = 0;
	}

	sprintf(theQuotedPath, "\"%s%s\" -r 1 -c \"%s\"", thePath, theAppBaseName, g_ini_file.c_str());

	theSCManager = ::OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_ALL_ACCESS   // access required
	);
	if (!theSCManager)
	{
		printf("Failed to install Service\n");
		return;
	}

	theService = CreateService(
		theSCManager,               // SCManager database
		inServiceName,               // name of service
		inServiceName,               // name to display
		SERVICE_ALL_ACCESS,         // desired access
		SERVICE_WIN32_OWN_PROCESS,  // service type
		SERVICE_AUTO_START,       // start type
		SERVICE_ERROR_NORMAL,       // error control type
		theQuotedPath,               // service's binary
		NULL,                       // no load ordering group
		NULL,                       // no tag identifier
		NULL,       // dependencies
		NULL,                       // LocalSystem account
		NULL);                      // no password

	if (theService)
	{
		SERVICE_DESCRIPTION desc;
		desc.lpDescription = "流媒体";
		if (ChangeServiceConfig2(theService, SERVICE_CONFIG_DESCRIPTION, &desc))
		{
			printf("Added MediaServer Service Description\n");
		}

		SERVICE_DELAYED_AUTO_START_INFO info = { true };

		if (ChangeServiceConfig2(theService, SERVICE_CONFIG_DELAYED_AUTO_START_INFO, &info))
		{
			printf("Added MediaServer Service Delayed Auto Start\n");
		}

		SERVICE_FAILURE_ACTIONS failact = { 0 };
		SC_ACTION act[3];
		act[0].Delay = act[1].Delay = act[2].Delay = 1 * 1000;
		act[0].Type = act[1].Type = act[2].Type = SC_ACTION_RESTART;
		failact.cActions = 3;
		failact.lpsaActions = act;
		failact.dwResetPeriod = 0;
		if (ChangeServiceConfig2(theService, SERVICE_CONFIG_FAILURE_ACTIONS, &failact))
		{
			printf("Seted MediaServer Service failure actions\n");
		}
		::CloseServiceHandle(theService);
		printf("Installed MediaServer Service\n");
	}
	else
		printf("Failed to install MediaServer Service\n");

	::CloseServiceHandle(theSCManager);
}

void RemoveService(char *inServiceName)
{
	SC_HANDLE   theSCManager;
	SC_HANDLE   theService;
	SERVICE_STATUS sStatus;

	theSCManager = ::OpenSCManager(
		NULL,                   // machine (NULL == local)
		NULL,                   // database (NULL == default)
		SC_MANAGER_ALL_ACCESS   // access required
	);
	if (!theSCManager)
	{
		printf("Failed to remove MediaServer Service\n");
		return;
	}

	theService = ::OpenService(theSCManager, inServiceName, SERVICE_ALL_ACCESS);
	if (theService) {
		if (::ControlService(theService, SERVICE_CONTROL_STOP, &sStatus)) {
			printf("Stopping Service: %s", inServiceName);
			Sleep(1000);

			while (::QueryServiceStatus(theService, &sStatus)) {
				if (SERVICE_STOP_PENDING == sStatus.dwCurrentState) {
					printf(".");
					Sleep(1000);
				}
				else
					break;
			}
			if (SERVICE_STOPPED == sStatus.dwCurrentState)
				printf("%s stopped.\n", inServiceName);
			else
				printf("%s failed to stopp.\n", inServiceName);
		}
		if (::DeleteService(theService))
			printf("Removed MediaServer Service!\n");
		else
			printf("Remove MediaServer Service failed!\n");
		::CloseServiceHandle(theService);
	}
	else
		printf("Failed to remove iDAS Service\n");
	::CloseServiceHandle(theSCManager);
}

MediaServer_ServerState StartServer(CMD_main& cmd_main) {
	MediaServer_ServerState theServerState = MediaServerStartingUpState;
	LogLevel logLevel = (LogLevel)cmd_main["level"].as<int>();
	logLevel = MIN(MAX(logLevel, LTrace), LError);
	string ssl_file = cmd_main["ssl"];
	int threads = cmd_main["threads"];

	//设置日志
	Logger::Instance().add(std::make_shared<ConsoleChannel>("ConsoleChannel", logLevel));
#ifndef ANDROID
	auto fileChannel = std::make_shared<FileChannel>("FileChannel", exeDir() + "log/", logLevel);
	//日志最多保存天数
	fileChannel->setMaxDay(cmd_main["max_day"]);
	Logger::Instance().add(fileChannel);
#endif//
	
	//启动异步日志线程
	Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());
	//加载配置文件，如果配置文件不存在就创建一个
	loadIniConfig(g_ini_file.data());

	if (!File::is_dir(ssl_file.data())) {
		//不是文件夹，加载证书，证书包含公钥和私钥
		SSL_Initor::Instance().loadCertificate(ssl_file.data());
	}
	else {
		//加载文件夹下的所有证书
		File::scanDir(ssl_file, [](const string &path, bool isDir) {
			if (!isDir) {
				//最后的一个证书会当做默认证书(客户端ssl握手时未指定主机)
				SSL_Initor::Instance().loadCertificate(path.data());
			}
			return true;
		});
	}

	uint16_t shellPort = mINI::Instance()[Shell::kPort];
	uint16_t rtspPort = mINI::Instance()[Rtsp::kPort];
	uint16_t rtspsPort = mINI::Instance()[Rtsp::kSSLPort];
	uint16_t rtmpPort = mINI::Instance()[Rtmp::kPort];
	uint16_t httpPort = mINI::Instance()[Http::kPort];
	uint16_t httpsPort = mINI::Instance()[Http::kSSLPort];

	//设置poller线程数,该函数必须在使用ZLToolKit网络相关对象之前调用才能生效
	EventPollerPool::setPoolSize(threads);

	//简单的telnet服务器，可用于服务器调试，但是不能使用23端口，否则telnet上了莫名其妙的现象
	//测试方法:telnet 127.0.0.1 9000
	TcpServer::Ptr shellSrv(new TcpServer());
	TcpServer::Ptr rtspSrv(new TcpServer());
	TcpServer::Ptr rtmpSrv(new TcpServer());
	TcpServer::Ptr httpSrv(new TcpServer());

	shellSrv->start<ShellSession>(shellPort);
	rtspSrv->start<RtspSession>(rtspPort);//默认10554
	rtmpSrv->start<RtmpSession>(rtmpPort);//默认10935
	//http服务器
	httpSrv->start<HttpSession>(httpPort);//默认80

	//如果支持ssl，还可以开启https服务器
	TcpServer::Ptr httpsSrv(new TcpServer());
	//https服务器,支持websocket
	httpsSrv->start<HttpsSession>(httpsPort);//默认443

	//支持ssl加密的rtsp服务器，可用于诸如亚马逊echo show这样的设备访问
	TcpServer::Ptr rtspSSLSrv(new TcpServer());
	rtspSSLSrv->start<RtspSessionWithSSL>(rtspsPort);//默认322

	installWebApi();
	InfoL << "已启动http api 接口";
	installWebHook();
	InfoL << "已启动http hook 接口";
	theServerState = MediaServerRunningState;

	return theServerState;
}

void RunServer() {

#ifdef WIN32
	CMiniDumper _miniDumper(true);
#endif

	while ((sServerState != MediaServerShuttingDownState) &&
		(sServerState != MediaServerFatalErrorState)) {
#ifdef WIN32
		Sleep(1000);
#else
		usleep(1000 * 1000);
#endif
	}
	
	unInstallWebApi();
	unInstallWebHook();
	//休眠1秒再退出，防止资源释放顺序错误
	InfoL << "程序退出中,请等待...";
	sleep(1);
	InfoL << "程序退出完毕!";
}