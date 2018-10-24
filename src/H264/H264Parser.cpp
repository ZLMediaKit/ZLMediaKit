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

#include "H264Parser.h"
#include "Util/logger.h"
using namespace toolkit;


H264Parser::H264Parser(){
    
}
H264Parser::~H264Parser(){
    
}
void H264Parser::inputH264(const string &h264,uint32_t dts){
    
    _parser.SetStream((const uint8_t *)h264.data(), h264.size());
    while (true) {
        if(media::H264Parser::kOk != _parser.AdvanceToNextNALU(&_nalu)){
            break;
        }
        
        switch (_nalu.nal_unit_type) {
            case media::H264NALU::kNonIDRSlice:
            case media::H264NALU::kIDRSlice:{
                if(media::H264Parser::kOk == _parser.ParseSliceHeader(_nalu, &_shdr)){
                    const media::H264SPS *pPps = _parser.GetSPS(_shdr.pic_parameter_set_id);
                    if (pPps) {
                        _poc.ComputePicOrderCnt(pPps, _shdr, &_iNowPOC);
                        computePts(dts);
                    }
                }
            }
                break;
            case media::H264NALU::kSPS:{
                int sps_id;
                _parser.ParseSPS(&sps_id);
            }
                break;
            case media::H264NALU::kPPS:{
                 int pps_id;
                _parser.ParsePPS(&pps_id);
            }
                break;
            default:
                break;
        }
    }
}

void H264Parser::computePts(uint32_t iNowDTS) {
	auto iPOCInc = _iNowPOC - _iLastPOC;
	if (_shdr.slice_type % 5 == 1) {
		//这是B帧
		_iNowPTS = _iLastPTS + _iMsPerPOC * (iPOCInc);
	} else {
		//这是I帧或者P帧
		_iNowPTS = iNowDTS;
		//计算每一POC的时间
		if(iPOCInc == 0){
			WarnL << "iPOCInc = 0," << _iNowPOC << " " << _iLastPOC;
		}else{
			_iMsPerPOC = (_iNowPTS - _iLastPTS) / iPOCInc;
		}
		_iLastPTS = _iNowPTS;
		_iLastPOC = _iNowPOC;
	}

    
//	DebugL << _shdr.slice_type
//			<<"\r\nNOW:"
//			<< _iNowPOC << " "
//			<< _iNowPTS << " "
//			<< iNowDTS << " "
//			<< "\r\nLST:"
//			<< _iLastPOC << " "
//			<< _iLastPTS << " "
//			<< _iMsPerPOC << endl;

}
















