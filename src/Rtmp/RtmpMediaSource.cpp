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

#include "RtmpMediaSource.h"
#include "MediaFile/MediaReader.h"

using namespace ZL::MediaFile;

namespace ZL {
namespace Rtmp {

recursive_mutex RtmpMediaSource::g_mtxMediaSrc;
unordered_map<string, unordered_map<string,weak_ptr<RtmpMediaSource> > > RtmpMediaSource::g_mapMediaSrc;

RtmpMediaSource::Ptr RtmpMediaSource::find(const string &strApp, const string &strId, bool bMake) {
	//查找某一媒体源，找到后返回
	lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
	auto itApp = g_mapMediaSrc.find(strApp);
	if (itApp == g_mapMediaSrc.end()) {
		return bMake ? MediaReader::onMakeRtmp(strApp, strId) : nullptr;
	}
	auto itId = itApp->second.find(strId);
	if (itId == itApp->second.end()) {
		return bMake ? MediaReader::onMakeRtmp(strApp, strId) : nullptr;
	}
	auto ret = itId->second.lock();
	if (ret) {
		return ret;
	}
	itApp->second.erase(itId);
	if (itApp->second.size() == 0) {
		g_mapMediaSrc.erase(itApp);
	}
	return bMake ? MediaReader::onMakeRtmp(strApp, strId) : nullptr;
}

} /* namespace Rtmp */
} /* namespace ZL */
