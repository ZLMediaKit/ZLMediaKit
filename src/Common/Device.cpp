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
#include <stdio.h>
#include <stdio.h>
#include "Device.h"
#include "Util/logger.h"
#include "Util/util.h"
#include "Util/base64.h"
#include "Util/TimeTicker.h"
#include "Extension/AAC.h"
#include "Extension/H264.h"
#include "Extension/H265.h"

using namespace toolkit;

namespace mediakit {

DevChannel::DevChannel(const string &strVhost,
                       const string &strApp,
                       const string &strId,
                       float fDuration,
                       bool bEanbleRtsp,
                       bool bEanbleRtmp,
                       bool bEanbleHls,
                       bool bEnableMp4) :
        MultiMediaSourceMuxer(strVhost, strApp, strId, fDuration, bEanbleRtsp, bEanbleRtmp, bEanbleHls, bEnableMp4) {}

DevChannel::~DevChannel() {}

#ifdef ENABLE_X264
void DevChannel::inputYUV(char* apcYuv[3], int aiYuvLen[3], uint32_t uiStamp) {
    //TimeTicker1(50);
    if (!_pH264Enc) {
        _pH264Enc.reset(new H264Encoder());
        if (!_pH264Enc->init(_video->iWidth, _video->iHeight, _video->iFrameRate)) {
            _pH264Enc.reset();
            WarnL << "H264Encoder init failed!";
        }
    }
    if (_pH264Enc) {
        H264Encoder::H264Frame *pOut;
        int iFrames = _pH264Enc->inputData(apcYuv, aiYuvLen, uiStamp, &pOut);
        for (int i = 0; i < iFrames; i++) {
            inputH264((char *) pOut[i].pucData, pOut[i].iLength, uiStamp);
        }
    }
}
#endif //ENABLE_X264

#ifdef ENABLE_FAAC
void DevChannel::inputPCM(char* pcData, int iDataLen, uint32_t uiStamp) {
    if (!_pAacEnc) {
        _pAacEnc.reset(new AACEncoder());
        if (!_pAacEnc->init(_audio->iSampleRate, _audio->iChannel, _audio->iSampleBit)) {
            _pAacEnc.reset();
            WarnL << "AACEncoder init failed!";
        }
    }
    if (_pAacEnc) {
        unsigned char *pucOut;
        int iRet = _pAacEnc->inputData(pcData, iDataLen, &pucOut);
        if (iRet > 0) {
            inputAAC((char *) pucOut, iRet, uiStamp);
        }
    }
}
#endif //ENABLE_FAAC

void DevChannel::inputH264(const char* pcData, int iDataLen, uint32_t dts,uint32_t pts) {
    if(dts == 0){
        dts = (uint32_t)_aTicker[0].elapsedTime();
    }
    if(pts == 0){
        pts = dts;
    }
    int prefixeSize;
    if (memcmp("\x00\x00\x00\x01", pcData, 4) == 0) {
        prefixeSize = 4;
    } else if (memcmp("\x00\x00\x01", pcData, 3) == 0) {
        prefixeSize = 3;
    } else {
        prefixeSize = 0;
    }

    H264Frame::Ptr frame = std::make_shared<H264Frame>();
    frame->_dts = dts;
    frame->_pts = pts;
    frame->_buffer.assign("\x00\x00\x00\x01",4);
    frame->_buffer.append(pcData + prefixeSize, iDataLen - prefixeSize);
    frame->_prefix_size = 4;
    inputFrame(frame);
}

void DevChannel::inputH265(const char* pcData, int iDataLen, uint32_t dts,uint32_t pts) {
    if(dts == 0){
        dts = (uint32_t)_aTicker[0].elapsedTime();
    }
    if(pts == 0){
        pts = dts;
    }
    int prefixeSize;
    if (memcmp("\x00\x00\x00\x01", pcData, 4) == 0) {
        prefixeSize = 4;
    } else if (memcmp("\x00\x00\x01", pcData, 3) == 0) {
        prefixeSize = 3;
    } else {
        prefixeSize = 0;
    }

    H265Frame::Ptr frame = std::make_shared<H265Frame>();
    frame->_dts = dts;
    frame->_pts = pts;
    frame->_buffer.assign("\x00\x00\x00\x01",4);
    frame->_buffer.append(pcData + prefixeSize, iDataLen - prefixeSize);
    frame->_prefix_size = 4;
    inputFrame(frame);
}

void DevChannel::inputAAC(const char* pcData, int iDataLen, uint32_t uiStamp,bool withAdtsHeader) {
    if(withAdtsHeader){
        inputAAC(pcData+7,iDataLen-7,uiStamp,pcData);
    } else if(_audio) {
        inputAAC(pcData,iDataLen,uiStamp,(char *)_adtsHeader);
    }
}

void DevChannel::inputAAC(const char *pcDataWithoutAdts,int iDataLen, uint32_t uiStamp,const char *pcAdtsHeader){
    if(uiStamp == 0){
        uiStamp = (uint32_t)_aTicker[1].elapsedTime();
    }
    if(pcAdtsHeader + 7 == pcDataWithoutAdts){
        inputFrame(std::make_shared<AACFrameNoCacheAble>((char *)pcDataWithoutAdts - 7,iDataLen + 7,uiStamp,7));
    } else {
        char *dataWithAdts = new char[iDataLen + 7];
        memcpy(dataWithAdts,pcAdtsHeader,7);
        memcpy(dataWithAdts + 7 , pcDataWithoutAdts , iDataLen);
        inputFrame(std::make_shared<AACFrameNoCacheAble>(dataWithAdts,iDataLen + 7,uiStamp,7));
        delete [] dataWithAdts;
    }
}


void DevChannel::initVideo(const VideoInfo& info) {
    _video = std::make_shared<VideoInfo>(info);
    addTrack(std::make_shared<H264Track>());
}

void DevChannel::initH265Video(const VideoInfo &info){
    _video = std::make_shared<VideoInfo>(info);
    addTrack(std::make_shared<H265Track>());
}

void DevChannel::initAudio(const AudioInfo& info) {
    _audio = std::make_shared<AudioInfo>(info);
    addTrack(std::make_shared<AACTrack>());

    AACFrame adtsHeader;
    adtsHeader.syncword = 0x0FFF;
    adtsHeader.id = 0;
    adtsHeader.layer = 0;
    adtsHeader.protection_absent = 1;
    adtsHeader.profile =  info.iProfile;//audioObjectType - 1;
    int i = 0;
    for(auto rate : samplingFrequencyTable){
        if(rate == info.iSampleRate){
            adtsHeader.sf_index = i;
        };
        ++i;
    }
    adtsHeader.private_bit = 0;
    adtsHeader.channel_configuration = info.iChannel;
    adtsHeader.original = 0;
    adtsHeader.home = 0;
    adtsHeader.copyright_identification_bit = 0;
    adtsHeader.copyright_identification_start = 0;
    adtsHeader.aac_frame_length = 7;
    adtsHeader.adts_buffer_fullness = 2047;
    adtsHeader.no_raw_data_blocks_in_frame = 0;
    writeAdtsHeader(adtsHeader,_adtsHeader);
}

} /* namespace mediakit */

