/*
 * PlayerBase.cpp
 *
 *  Created on: 2016年12月1日
 *      Author: xzl
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
