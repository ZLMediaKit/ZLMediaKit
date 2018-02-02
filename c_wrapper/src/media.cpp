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

#include "media.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Util/onceToken.h"
#include "Device/Device.h"
#include "cleaner.h"

using namespace std;
using namespace ZL::DEV;
using namespace ZL::Util;

static recursive_mutex s_mtxMapMedia;
static unordered_map<void *, DevChannel::Ptr> s_mapMedia;

static onceToken s_token([](){
	cleaner::Instance().push_front([](){
		lock_guard<recursive_mutex> lck(s_mtxMapMedia);
		s_mapMedia.clear();
		DebugL << "clear media" << endl;
	});
},nullptr);



//////////////////////////Rtsp media///////////////////////////
API_EXPORT MediaContext API_CALL createMedia(const char *appName,const char *mediaName) {
	DevChannel::Ptr ret(new DevChannel(DEFAULT_VHOST,appName,mediaName));
	lock_guard<recursive_mutex> lck(s_mtxMapMedia);
	s_mapMedia.emplace((void *) (ret.get()), ret);
	return ret.get();
}
API_EXPORT void API_CALL releaseMedia(MediaContext ctx) {
	lock_guard<recursive_mutex> lck(s_mtxMapMedia);
	s_mapMedia.erase(ctx);
}

API_EXPORT void API_CALL media_initVideo(MediaContext ctx, int width, int height, int frameRate) {
	DevChannel *ptr = (DevChannel *) ctx;
	VideoInfo info;
	info.iFrameRate = frameRate;
	info.iWidth = width;
	info.iHeight = height;
	ptr->initVideo(info);
}
API_EXPORT void API_CALL media_initAudio(MediaContext ctx, int channel, int sampleBit, int sampleRate) {
	DevChannel *ptr = (DevChannel *) ctx;
	AudioInfo info;
	info.iSampleRate = sampleRate;
	info.iChannel = channel;
	info.iSampleBit = sampleBit;
	ptr->initAudio(info);
}
API_EXPORT void API_CALL media_inputH264(MediaContext ctx, void *data, int len, unsigned long stamp) {
	//TimeTicker();
	DevChannel *ptr = (DevChannel *) ctx;
	ptr->inputH264((char *) data, len, stamp);
}
API_EXPORT void API_CALL media_inputAAC(MediaContext ctx, void *data, int len,unsigned long stamp) {
	//TimeTicker();
	DevChannel *ptr = (DevChannel *) ctx;
	ptr->inputAAC((char *) data, len, stamp);
}

API_EXPORT void API_CALL media_inputAAC1(MediaContext ctx, void *data, int len, unsigned long stamp,void *adts){
    DevChannel *ptr = (DevChannel *) ctx;
    ptr->inputAAC((char *) data, len, stamp,(char *)adts);
}

API_EXPORT void API_CALL media_inputAAC2(MediaContext ctx, void *data, int len, unsigned long stamp,void *aac_cfg){
    DevChannel *ptr = (DevChannel *) ctx;
    AdtsFrame frame;
    makeAdtsHeader((char*)aac_cfg, frame);
    char adts[8];
    writeAdtsHeader(frame, (uint8_t*)adts);
    ptr->inputAAC((char *) data, len, stamp,(char *)adts);
}



