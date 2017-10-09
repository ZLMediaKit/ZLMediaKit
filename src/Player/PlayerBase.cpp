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

using namespace std;
using namespace ZL::Rtmp;
using namespace ZL::Rtsp;

namespace ZL {
namespace Player {


PlayerBase::Ptr PlayerBase::createPlayer(const char* strUrl) {
	string prefix = FindField(strUrl, NULL, "://");
	auto onDestory = [](PlayerBase *ptr){
		ASYNC_TRACE([ptr](){
			delete ptr;
		});
	};
	if (strcasecmp("rtsp",prefix.data()) == 0) {
		return PlayerBase::Ptr(new RtspPlayerImp(),onDestory);
	}
	if (strcasecmp("rtmp",prefix.data()) == 0) {
		return PlayerBase::Ptr(new RtmpPlayerImp(),onDestory);
	}
	return PlayerBase::Ptr(new RtspPlayerImp(),onDestory);
}

} /* namespace Player */
} /* namespace ZL */
