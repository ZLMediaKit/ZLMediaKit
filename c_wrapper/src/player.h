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

#ifndef SRC_PLAYER_H_
#define SRC_PLAYER_H_

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif


////////////////////////////////////////RTSP Player/////////////////////////////////////////////////
typedef void* PlayerContext;
typedef void(API_CALL *player_onResult)(void *userData,int errCode,const char *errMsg);
typedef void(API_CALL *player_onGetAAC)(void *userData,void *data,int len,unsigned long timeStamp);
typedef void(API_CALL *player_onGetH264)(void *userData,void *data,int len,unsigned long dts,unsigned long pts);
/*
 * 描述:创建一个Rtsp播放器
 * 参数:无
 * 返回值:Rtsp播放器句柄
 */
API_EXPORT PlayerContext API_CALL createPlayer();

/*
 * 描述:销毁一个播放器
 * 参数:ctx:播放器句柄
 * 返回值:无
 */
API_EXPORT void API_CALL releasePlayer(PlayerContext ctx);


/*
 * 描述:设置播放器配置选项
 * 参数:	ctx:播放器句柄
 *		key:配置项键,例如　rtp_type
 *		val:值,例如　//设置rtp传输类型，可选项有0(tcp，默认)、1(udp)、2(组播)
 * 返回值:无
 */
API_EXPORT void API_CALL player_setOptionInt(PlayerContext ctx,const char* key,int val);
API_EXPORT void API_CALL player_setOptionString(PlayerContext ctx,const char* key,const char *val);


/*
 * 描述:播放rtsp链接（仅支持H264与AAC负载）
 * 参数:	ctx:播放器句柄
 *		url:rtsp链接
 * 返回值:无
 */
API_EXPORT void API_CALL player_play(PlayerContext ctx,const char* url);

/*
 * 描述:暂停播放RTSP
 * 参数:ctx:播放器句柄；pause:1:暂停播放，0：恢复播放
 * 返回值:无
 */
API_EXPORT void API_CALL player_pause(PlayerContext ctx,int pause);

/*
 * 描述:设置播放器异常停止回调函数
 * 参数:ctx:播放器句柄;cb:回调函数指针;userData:用户数据指针
 * 返回值:无
 */
API_EXPORT void API_CALL player_setOnShutdown(PlayerContext ctx,player_onResult cb,void *userData);

/*
 * 描述:设置播放器播放结果回调函数
 * 参数:ctx:播放器句柄,cb:回调函数指针;userData:用户数据指针
 * 返回值:无
 */
API_EXPORT void API_CALL player_setOnPlayResult(PlayerContext ctx,player_onResult cb,void *userData);

/*
 * 描述:设置播放器收到视频帧回调,I帧前为SPS，PPS帧；每帧包含00 00 00 01的帧头
 * 参数:ctx:播放器句柄,cb:回调函数指针;userData:用户数据指针
 * 返回值:无
 */
API_EXPORT void API_CALL player_setOnGetVideo(PlayerContext ctx,player_onGetH264 cb,void *userData);


/*
 * 描述:设置播放器收到音频帧回调,每帧数据包含ADTS头
 * 参数:ctx:播放器句柄,cb:回调函数指针;userData:用户数据指针
 * 返回值:无
 */
API_EXPORT void API_CALL player_setOnGetAudio(PlayerContext ctx,player_onGetAAC cb,void *userData);

/*
 * 描述:获取视频宽度
 * 参数:ctx:播放器句柄
 * 返回值:视频宽度
 */
API_EXPORT int API_CALL player_getVideoWidth(PlayerContext ctx);

/*
 * 描述:获取视频高度
 * 参数:ctx:播放器句柄
 * 返回值:视频高度
 */
API_EXPORT int API_CALL API_CALL player_getVideoHeight(PlayerContext ctx);

/*
 * 描述:获取视频帧率
 * 参数:ctx:播放器句柄
 * 返回值:视频帧率
 */
API_EXPORT int API_CALL player_getVideoFps(PlayerContext ctx);

/*
 * 描述:获取音频采样率
 * 参数:ctx:播放器句柄
 * 返回值:音频采样率
 */
API_EXPORT int API_CALL player_getAudioSampleRate(PlayerContext ctx);

/*
 * 描述:获取音频采样位数（8bit或16bit）
 * 参数:ctx:播放器句柄
 * 返回值:音频采样位数
 */
API_EXPORT int API_CALL player_getAudioSampleBit(PlayerContext ctx);

/*
 * 描述:获取音频通道数（单声道1，双声道2）
 * 参数:ctx:播放器句柄
 * 返回值:音频通道数
 */
API_EXPORT int API_CALL player_getAudioChannel(PlayerContext ctx);


/*
 * 描述:获取H264的PPS帧
 * 参数:ctx:播放器句柄，buf:存放PPS数据的缓存；bufsize：缓存大小
 * 返回值:帧数据长度
 */
API_EXPORT int API_CALL player_getH264PPS(PlayerContext ctx,char *buf,int bufsize);


/*
 * 描述:获取H264的SPS帧
 * 参数:ctx:播放器句柄，buf:存放SPS数据的缓存；bufsize：缓存大小
 * 返回值:帧数据长度
 */
API_EXPORT int API_CALL player_getH264SPS(PlayerContext ctx,char *buf,int bufsize);


/*
 * 描述:获取AAC编码配置信息
 * 参数:ctx:播放器句柄；buf:存放CFG数据的缓存；bufsize：缓存大小
 * 返回值:CFG数据长度
 */
API_EXPORT int API_CALL player_getAacCfg(PlayerContext ctx,char *buf,int bufsize);


/*
 * 描述:是否包含音频数据
 * 参数:ctx:播放器句柄
 * 返回值:1：包含，0：不包含
 */
API_EXPORT int API_CALL player_containAudio(PlayerContext ctx);


/*
 * 描述:是否包含视频数据
 * 参数:ctx:播放器句柄
 * 返回值:1：包含，0：不包含
 */
API_EXPORT int API_CALL player_containVideo(PlayerContext ctx);


/*
 * 描述:是否已经初始化完成（获取完整的播放信息）
 * 参数:ctx:播放器句柄
 * 返回值:1：初始化完成，0：未完成
 */
API_EXPORT int API_CALL player_isInited(PlayerContext ctx);

/*
 * 描述:获取点播的时间长度，单位为秒（小于等于0，说明是直播，否则为点播）
 * 参数:ctx:播放器句柄
 * 返回值:点播的时间长度，单位秒
 */
API_EXPORT float API_CALL player_getDuration(PlayerContext ctx);

/*
 * 描述:获取点播播放进度
 * 参数:ctx:播放器句柄
 * 返回值:点播播放进度，取值范围未 0.0～1.0
 */
API_EXPORT float API_CALL player_getProgress(PlayerContext ctx);

/*
 * 描述:设置点播播放进度
 * 参数:ctx:播放器句柄；fProgress：播放进度，取值范围未 0.0～1.0
 * 返回值:无
 */
API_EXPORT void API_CALL player_seekTo(PlayerContext ctx, float fProgress);

/*
 * 描述:获取丢包率
 * 参数:ctx:播放器句柄；trackId:如果是-1,则返回总丢包率，否则返回视频或者音频的丢包率
 * 返回值:丢包率
 */
API_EXPORT float API_CALL player_getLossRate(PlayerContext ctx,int trackId);


#ifdef __cplusplus
}
#endif

#endif /* SRC_PLAYER_H_ */
