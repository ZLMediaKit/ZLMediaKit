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
	if (strcasecmp("rtsp",prefix.data()) == 0) {
		return PlayerBase::Ptr(new RtspPlayerImp());
	}
	if (strcasecmp("rtmp",prefix.data()) == 0) {
		return PlayerBase::Ptr(new RtmpPlayerImp());
	}
	return PlayerBase::Ptr(new RtspPlayerImp());
}

} /* namespace Player */
} /* namespace ZL */
