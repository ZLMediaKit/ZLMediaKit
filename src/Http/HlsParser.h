﻿/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HTTP_HLSPARSER_H
#define HTTP_HLSPARSER_H

#include <string>
#include <list>
#include <map>

namespace mediakit {

typedef struct{
    //url地址
    std::string url;
    //ts切片长度
    float duration;

    //////内嵌m3u8//////
    //节目id
    int program_id;
    //带宽
    int bandwidth;
    //宽度
    int width;
    //高度
    int height;
} ts_segment;

class HlsParser {
public:
    bool parse(const std::string &http_url,const std::string &m3u8);

    /**
     * 是否存在#EXTM3U字段，是否为m3u8文件
     */
    bool isM3u8() const;

    /**
     * #EXT-X-ALLOW-CACHE值，是否允许cache
     */
    bool allowCache() const;

    /**
     * 是否存在#EXT-X-ENDLIST字段，是否为直播
     */
    bool isLive() const ;

    /**
     * #EXT-X-VERSION值，版本号
     */
    int getVersion() const;

    /**
     * #EXT-X-TARGETDURATION字段值
     */
    int getTargetDur() const;

    /**
     * #EXT-X-MEDIA-SEQUENCE字段值，该m3u8序号
     */
    int64_t getSequence() const;

    /**
     * 内部是否含有子m3u8
     */
    bool isM3u8Inner() const;

    /**
     * 得到总时间
     */
    float getTotalDuration() const;

protected:
    /**
     * 解析m3u8文件回调
     * @param is_m3u8_inner 该m3u8文件中是否包含多个hls地址
     * @param sequence ts序号
     * @param ts_list ts地址列表
     * @return 是否解析成功，返回false时，将导致HlsParser::parse返回false
     */
    virtual bool onParsed(bool is_m3u8_inner, int64_t sequence, const std::map<int, ts_segment> &ts_list) = 0;

private:
    bool _is_m3u8 = false;
    bool _allow_cache = false;
    bool _is_live = true;
    int _version = 0;
    int _target_dur = 0;
    float _total_dur = 0;
    int64_t _sequence = 0;
    //每部是否有m3u8
    bool _is_m3u8_inner = false;
};

}//namespace mediakit
#endif //HTTP_HLSPARSER_H
