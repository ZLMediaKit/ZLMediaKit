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

using namespace ZL::Util;

H264Parser::H264Parser(){
    
}
H264Parser::~H264Parser(){
    
}
void H264Parser::inputH264(const string &h264,uint32_t dts){
    
    m_parser.SetStream((const uint8_t *)h264.data(), h264.size());
    while (true) {
        if(media::H264Parser::kOk != m_parser.AdvanceToNextNALU(&m_nalu)){
            break;
        }
        
        switch (m_nalu.nal_unit_type) {
            case media::H264NALU::kNonIDRSlice:
            case media::H264NALU::kIDRSlice:{
                if(media::H264Parser::kOk == m_parser.ParseSliceHeader(m_nalu, &m_shdr)){
                    const media::H264SPS *pPps = m_parser.GetSPS(m_shdr.pic_parameter_set_id);
                    if (pPps) {
                        m_poc.ComputePicOrderCnt(pPps, m_shdr, &m_iNowPOC);
                        computePts(dts);
                    }
                }
            }
                break;
            case media::H264NALU::kSPS:{
                int sps_id;
                m_parser.ParseSPS(&sps_id);
            }
                break;
            case media::H264NALU::kPPS:{
                 int pps_id;
                m_parser.ParsePPS(&pps_id);
            }
                break;
            default:
                break;
        }
    }
}

void H264Parser::computePts(uint32_t iNowDTS) {
	auto iPOCInc = m_iNowPOC - m_iLastPOC;
	if (m_shdr.slice_type % 5 == 1) {
		//这是B帧
		m_iNowPTS = m_iLastPTS + m_iMsPerPOC * (iPOCInc);
	} else {
		//这是I帧或者P帧
		m_iNowPTS = iNowDTS;
		//计算每一POC的时间
		if(iPOCInc == 0){
			WarnL << "iPOCInc = 0," << m_iNowPOC << " " << m_iLastPOC;
		}else{
			m_iMsPerPOC = (m_iNowPTS - m_iLastPTS) / iPOCInc;
		}
		m_iLastPTS = m_iNowPTS;
		m_iLastPOC = m_iNowPOC;
	}

    
//	DebugL << m_shdr.slice_type
//			<<"\r\nNOW:"
//			<< m_iNowPOC << " "
//			<< m_iNowPTS << " "
//			<< iNowDTS << " "
//			<< "\r\nLST:"
//			<< m_iLastPOC << " "
//			<< m_iLastPTS << " "
//			<< m_iMsPerPOC << endl;

}
















