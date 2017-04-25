//
//  H264Parser.h
//  MediaPlayer
//
//  Created by xzl on 2017/1/16.
//  Copyright © 2017年 jizan. All rights reserved.
//

#ifndef H264Parser_h
#define H264Parser_h

#include <stdio.h>
#include <string>
#include "h264_poc.h"
#include "h264_parser.h"

using namespace std;


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
