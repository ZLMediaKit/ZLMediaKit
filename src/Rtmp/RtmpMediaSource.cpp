/*
 * MediaSource.cpp
 *
 *  Created on: 2016年9月1日
 *      Author: xzl
 */

#include "RtmpMediaSource.h"
#include "MedaiFile/MediaReader.h"
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
