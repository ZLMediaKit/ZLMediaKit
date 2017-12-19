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

#ifndef SRC_MEDIA_H_
#define SRC_MEDIA_H_

#include "common.h"

#ifdef __cplusplus
extern "C" {
#endif


////////////////////////////////Rtsp media////////////////////////////////////////////

typedef void* MediaContext;
/*
 * 描述:创建一个媒体源
 * 参数:mediaName:媒体名称,url地址的一部分
 * 返回值:媒体源句柄
 */
API_EXPORT MediaContext API_CALL createMedia(const char *appName,const char *mediaName);

/*
 * 描述:销毁媒体源
 * 参数:ctx:媒体源句柄
 * 返回值:无
 */
API_EXPORT void API_CALL releaseMedia(MediaContext ctx);

/*
 * 描述:初始化媒体源的视频信息
 * 参数:ctx:媒体源句柄;width:视频宽度;height:视频高度;frameRate:视频fps
 * 返回值:无
 */
API_EXPORT void API_CALL media_initVideo(MediaContext ctx, int width, int height, int frameRate);

/*
 * 描述:初始化媒体源的音频信息
 * 参数:ctx:媒体源句柄;channel:声道数;sampleBit:音频采样位数,支持16bit;sampleRate:音频采样率
 * 返回值:无
 */
API_EXPORT void API_CALL media_initAudio(MediaContext ctx, int channel, int sampleBit, int sampleRate);

/*
 * 描述:输入单帧H264视频，需要输入SPS和PPS帧,帧起始字节00 00 01,00 00 00 01均可
 * 参数:ctx:媒体源句柄;data:单帧H264数据;len:单帧H264数据字节数;stamp:时间戳，毫秒
 * 返回值:无
 */
API_EXPORT void API_CALL media_inputH264(MediaContext ctx, void *data, int len, unsigned long stamp);

/*
 * 描述:输入单帧AAC音频(有adts头)
 * 参数:ctx:媒体源句柄;data:单帧AAC数据;len:单帧AAC数据字节数;stamp:时间戳，毫秒
 * 返回值:无
 */
API_EXPORT void API_CALL media_inputAAC(MediaContext ctx, void *data, int len, unsigned long stamp);

    
/*
 * 描述:输入单帧AAC音频(单独指定adts头)
 * 参数:ctx:媒体源句柄;data:单帧AAC数据;len:单帧AAC数据字节数;stamp:时间戳，毫秒;adts:adts头指针
 * 返回值:无
 */
API_EXPORT void API_CALL media_inputAAC1(MediaContext ctx, void *data, int len, unsigned long stamp,void *adts);


/*
 * 描述:输入单帧AAC音频(指定2个字节的aac配置)
 * 参数:ctx:媒体源句柄;data:单帧AAC数据;len:单帧AAC数据字节数;stamp:时间戳，毫秒;adts:adts头指针;aac_cfg:aac配置
 * 返回值:无
 */
API_EXPORT void API_CALL media_inputAAC2(MediaContext ctx, void *data, int len, unsigned long stamp,void *aac_cfg);

    
#ifdef __cplusplus
}
#endif

#endif /* SRC_MEDIA_H_ */
