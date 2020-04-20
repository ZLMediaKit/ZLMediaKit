/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
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
        WarnL << getCodecName() << " Track未准备好";
        return nullptr;
    }
    return std::make_shared<H265Sdp>(getVps(),getSps(),getPps());
}
}//namespace mediakit

