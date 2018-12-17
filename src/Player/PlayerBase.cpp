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

#include <algorithm>
#include "PlayerBase.h"
#include "Rtsp/Rtsp.h"
#include "Rtsp/RtspPlayerImp.h"
#include "Rtmp/RtmpPlayerImp.h"
using namespace toolkit;

namespace mediakit {

const char PlayerBase::kNetAdapter[] = "net_adapter";
const char PlayerBase::kRtpType[] = "rtp_type";
const char PlayerBase::kRtspUser[] = "rtsp_user" ;
const char PlayerBase::kRtspPwd[] = "rtsp_pwd";
const char PlayerBase::kRtspPwdIsMD5[] = "rtsp_pwd_md5";


PlayerBase::Ptr PlayerBase::createPlayer(const char* strUrl) {
	static auto releasePlayer = [](PlayerBase *ptr){
		onceToken token(nullptr,[&](){
			delete  ptr;
		});
		ptr->teardown();
	};
	string prefix = FindField(strUrl, NULL, "://");
	if (strcasecmp("rtsp",prefix.data()) == 0) {
		return PlayerBase::Ptr(new RtspPlayerImp(),releasePlayer);
	}
	if (strcasecmp("rtmp",prefix.data()) == 0) {
		return PlayerBase::Ptr(new RtmpPlayerImp(),releasePlayer);
	}
	return PlayerBase::Ptr(new RtspPlayerImp(),releasePlayer);
}

///////////////////////////Demuxer//////////////////////////////
bool Demuxer::isInited(int analysisMs) {
	if(_ticker.createdTime() < analysisMs){
		//analysisMs毫秒内判断条件
		//如果音视频都准备好了 ，说明Track全部就绪
		return (_videoTrack &&  _videoTrack->ready() && _audioTrack && _audioTrack->ready());
	}
	//analysisMs毫秒后强制初始化完毕
	return true;
}

vector<Track::Ptr> Demuxer::getTracks(bool trackReady) const {
	vector<Track::Ptr> ret;
	if(_videoTrack){
		if(trackReady){
			if(_videoTrack->ready()){
				ret.emplace_back(_videoTrack);
			}
		}else{
			ret.emplace_back(_videoTrack);
		}
	}
	if(_audioTrack){
		if(trackReady){
			if(_audioTrack->ready()){
				ret.emplace_back(_audioTrack);
			}
		}else{
			ret.emplace_back(_audioTrack);
		}
	}
	return ret;
}

float Demuxer::getDuration() const {
	return _fDuration;
}
} /* namespace mediakit */
