/*
 * MediaSource.h
 *
 *  Created on: 2016年8月10日
 *      Author: xzl
 */

#include "RtspMediaSource.h"
#include "MedaiFile/MediaReader.h"

using namespace ZL::MediaFile;

namespace ZL {
namespace Rtsp {

recursive_mutex RtspMediaSource::g_mtxMediaSrc;
unordered_map<string, unordered_map<string, weak_ptr<RtspMediaSource> > > RtspMediaSource::g_mapMediaSrc;

RtspMediaSource::Ptr RtspMediaSource::find(const string &strApp, const string &strId,bool bMake) {
	//查找某一媒体源，找到后返回
	lock_guard<recursive_mutex> lock(g_mtxMediaSrc);
	auto itApp = g_mapMediaSrc.find(strApp);
	if (itApp == g_mapMediaSrc.end()) {
		return bMake ? MediaReader::onMakeRtsp(strApp, strId) : nullptr;
	}
	auto itId = itApp->second.find(strId);
	if (itId == itApp->second.end()) {
		return bMake ? MediaReader::onMakeRtsp(strApp, strId) : nullptr;
	}
	auto ret = itId->second.lock();
	if(ret){
		return ret;
	}
	itApp->second.erase(itId);
	if (itApp->second.size() == 0) {
		g_mapMediaSrc.erase(itApp);
	}
	return bMake ? MediaReader::onMakeRtsp(strApp, strId) : nullptr;
}

} /* namespace Rtsp */
} /* namespace ZL */
