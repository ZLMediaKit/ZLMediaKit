/*
 * MIT License
 *
 * Copyright (c) 2016 xiongziliang <771730766@qq.com>
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

#ifndef SRC_COMMON_H_
#define SRC_COMMON_H_

#include <stdint.h>

#if defined(_WIN32)
#if defined(MediaKitWrapper_EXPORTS)
		#define API_EXPORT __declspec(dllexport)
	#else
		#define API_EXPORT __declspec(dllimport)
	#endif

	#define CALLTYPE __cdecl
#else
#define API_EXPORT
#define CALLTYPE
#endif


#ifdef __cplusplus
extern "C" {
#endif



/////////////////////////environment init////////////////////////////////
API_EXPORT void CALLTYPE onAppStart();
API_EXPORT void CALLTYPE onAppExit();

/*
 * 描述:创建Http服务器
 * 参数:port:htt监听端口，推荐80
 * 返回值:0:成功;-1:失败
 */
API_EXPORT int CALLTYPE initHttpServer(unsigned short port);

/*
 * 描述:创建RTSP服务器
 * 参数:port:rtsp监听端口，推荐554
 * 返回值:0:成功;-1:失败
 */
API_EXPORT int CALLTYPE initRtspServer(unsigned short port);

/*
 * 描述:创建RTMP服务器
 * 参数:port:rtmp监听端口，推荐1935
 * 返回值:0:成功;-1:失败
 */
API_EXPORT int CALLTYPE initRtmpServer(unsigned short port);

/*
 * 描述:播放事件回调函数定义
 * 参数:userData:用户数据指针strApp:应用名,strStream:流名称
 */
typedef void (CALLTYPE *onEventPlay)(void *userData,const char *strApp,const char *strStream);

/*
 * 描述:监听事件
 * 参数:cb:回调函数指针,userData:用户数据指针
 * 返回值:无
 */
API_EXPORT void CALLTYPE listenEvent_onPlay(onEventPlay cb,void *userData);

/*
 * 描述:注册RTSP事件
 * 参数:userData:用户数据指针strApp:应用名,strStream:流名称
 */
typedef void (CALLTYPE *onEventRegistMediaSrc)(void *userData,const char *strApp,const char *strStream);

/*
 * 描述:监听事件
 * 参数:cb:回调函数指针,userData:用户数据指针
 * 返回值:无
 */
API_EXPORT void CALLTYPE listenEvent_onRegistRtsp(onEventRegistMediaSrc cb,void *userData);

/*
 * 描述:监听事件
 * 参数:cb:回调函数指针,userData:用户数据指针
 * 返回值:无
 */
API_EXPORT void CALLTYPE listenEvent_onRegistRtmp(onEventRegistMediaSrc cb,void *userData);


/////////////////////////日志////////////////////////////////

typedef enum {
            //日志级别
            LogTrace = 0, LogDebug, LogInfo, LogWarn, LogError, LogFatal,
} LogType;

typedef void(CALLTYPE *onLogOut)(const char *strLog, int iLogLen);

/*
 * 描述:设置Log输出回调
 * 参数:onLogOut：回调函数
 * 返回值:无
 */
API_EXPORT void CALLTYPE log_setOnLogOut(onLogOut);

/*
 * 描述:设在日志显示级别
 * 参数:level:日志级别
 * 返回值:无
 */
API_EXPORT void CALLTYPE log_setLevel(LogType level);

/*
 * 描述:打印日志
 * 参数:level:日志级别；file:文件名称,function：函数名称，line：所在行数，fmt：格式化字符串
 * 返回值:无
 */
API_EXPORT void CALLTYPE log_printf(LogType level, const char* file, const char* function, int line, const char *fmt, ...);


#define log_trace(fmt,...) log_printf(LogTrace,__FILE__,__FUNCTION__,__LINE__,fmt,__VA_ARGS__)
#define log_debug(fmt,...) log_printf(LogDebug,__FILE__,__FUNCTION__,__LINE__,fmt,__VA_ARGS__)
#define log_info(fmt,...) log_printf(LogInfo,__FILE__,__FUNCTION__,__LINE__,fmt,__VA_ARGS__)
#define log_warn(fmt,...) log_printf(LogWarn,__FILE__,__FUNCTION__,__LINE__,fmt,__VA_ARGS__)
#define log_error(fmt,...) log_printf(LogError,__FILE__,__FUNCTION__,__LINE__,fmt,__VA_ARGS__)
#define log_fatal(fmt,...) log_printf(LogFatal,__FILE__,__FUNCTION__,__LINE__,fmt,__VA_ARGS__)


#ifdef __cplusplus
}
#endif


#endif /* SRC_COMMON_H_ */
