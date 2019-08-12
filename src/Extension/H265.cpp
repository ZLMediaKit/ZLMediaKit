/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
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

#include "H265.h"
#include "SPSParser.h"
#include "Util/logger.h"

namespace mediakit{

bool getHEVCInfo(const char * vps, int vps_len,const char * sps,int sps_len,int &iVideoWidth, int &iVideoHeight, float  &iVideoFps){
    T_GetBitContext tGetBitBuf;
    T_HEVCSPS tH265SpsInfo;	
    T_HEVCVPS tH265VpsInfo;
	if ( vps_len > 2 ){
	    memset(&tGetBitBuf,0,sizeof(tGetBitBuf));	
	    memset(&tH265VpsInfo,0,sizeof(tH265VpsInfo));
	    tGetBitBuf.pu8Buf = (uint8_t*)vps+2;
	    tGetBitBuf.iBufSize = vps_len-2;
	    if(0 != h265DecVideoParameterSet((void *) &tGetBitBuf, &tH265VpsInfo)){
	        return false;
    	}
	}

	if ( sps_len > 2 ){
	    memset(&tGetBitBuf,0,sizeof(tGetBitBuf));
	    memset(&tH265SpsInfo,0,sizeof(tH265SpsInfo));
	    tGetBitBuf.pu8Buf = (uint8_t*)sps+2;
	    tGetBitBuf.iBufSize = sps_len-2;
	    if(0 != h265DecSeqParameterSet((void *) &tGetBitBuf, &tH265SpsInfo)){
	        return false;
	    }
	}
	else 
		return false;
    h265GetWidthHeight(&tH265SpsInfo, &iVideoWidth, &iVideoHeight);
	iVideoFps = 0;
    h265GeFramerate(&tH265VpsInfo, &tH265SpsInfo, &iVideoFps);
//    ErrorL << iVideoWidth << " " << iVideoHeight << " " << iVideoFps;
    return true;
}

bool getHEVCInfo(const string &strVps, const string &strSps, int &iVideoWidth, int &iVideoHeight, float &iVideoFps) {
	return getHEVCInfo(strVps.data(),strVps.size(),strSps.data(),strSps.size(),iVideoWidth,iVideoHeight,iVideoFps);
}

Sdp::Ptr H265Track::getSdp() {
    if(!ready()){
        WarnL << "H265 Track未准备好";
        return nullptr;
    }
    return std::make_shared<H265Sdp>(getVps(),getSps(),getPps());
}
}//namespace mediakit

