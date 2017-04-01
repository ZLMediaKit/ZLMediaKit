/*
 * RtspParserTester.cpp
 *
 *  Created on: 2016年9月5日
 *      Author: xzl
 */

#include "RtspPlayerImp.h"

namespace ZL {
    namespace Rtsp {
        
        RtspPlayerImp::RtspPlayerImp() {
        }
        
        RtspPlayerImp::~RtspPlayerImp() {
            DebugL<<endl;
            teardown();
        }
        
        
    } /* namespace Rtsp */
} /* namespace ZL */
