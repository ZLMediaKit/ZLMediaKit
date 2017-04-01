/*
 * RtmpPlayerImp.cpp
 *
 *  Created on: 2016年12月1日
 *      Author: xzl
 */

#include "RtmpPlayerImp.h"

namespace ZL {
namespace Rtmp {

RtmpPlayerImp::RtmpPlayerImp() {

}

RtmpPlayerImp::~RtmpPlayerImp() {
    DebugL<<endl;
    teardown();
}

} /* namespace Rtmp */
} /* namespace ZL */
