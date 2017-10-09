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

#ifndef H264Parser_h
#define H264Parser_h

#include <stdio.h>
#include <string>
#include "h264_poc.h"
#include "h264_parser.h"
using namespace std;

#ifndef INT32_MAX
#define INT32_MAX 0x7FFFFFFF
#endif//INT32_MAX

class H264Parser{
public:
    H264Parser();
    virtual ~H264Parser();
    void inputH264(const string &h264,uint32_t dts);

    int32_t getPOC() const{
        return m_iNowPOC;
    }
    int getSliceType() const{
        return m_shdr.slice_type;
    }
    int getNaluType() const{
        return m_nalu.nal_unit_type;
    }
    uint32_t getPts() const{
    		return m_iNowPTS;
    }
private:
    media::H264Parser m_parser;
    media::H264POC m_poc;
    media::H264NALU m_nalu;
    media::H264SliceHeader m_shdr;

    int32_t m_iNowPOC = INT32_MAX;
    int32_t m_iLastPOC = INT32_MAX;

    uint32_t m_iNowPTS = INT32_MAX;
    uint32_t m_iLastPTS = INT32_MAX;
    int32_t m_iMsPerPOC = 30;

    void computePts(uint32_t dts);


};

#endif /* H264Parser_hpp */
