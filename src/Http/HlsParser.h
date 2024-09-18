/*
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
    // url地址  [AUTO-TRANSLATED:64a1b5d1]
    // URL address
    std::string url;
    // ts切片长度  [AUTO-TRANSLATED:9d5545f8]
    // TS segment length
    float duration;

    // ////内嵌m3u8//////  [AUTO-TRANSLATED:c3fabbfd]
    // //// Embedded m3u8 //////
    // 节目id  [AUTO-TRANSLATED:8c6000cc]
    // Program ID
    int program_id;
    // 带宽  [AUTO-TRANSLATED:5f852828]
    // Bandwidth
    int bandwidth;
    // 宽度  [AUTO-TRANSLATED:06ad2724]
    // Width
    int width;
    // 高度  [AUTO-TRANSLATED:87a07641]
    // Height
    int height;
} ts_segment;

class HlsParser {
public:
    bool parse(const std::string &http_url,const std::string &m3u8);

    /**
     * 是否存在#EXTM3U字段，是否为m3u8文件
     * Whether the #EXTM3U field exists, whether it is an m3u8 file
     
     * [AUTO-TRANSLATED:ac1bf089]
     */
    bool isM3u8() const;

    /**
     * #EXT-X-ALLOW-CACHE值，是否允许cache
     * #EXT-X-ALLOW-CACHE value, whether caching is allowed
     
     * [AUTO-TRANSLATED:90e88422]
     */
    bool allowCache() const;

    /**
     * 是否存在#EXT-X-ENDLIST字段，是否为直播
     * Whether the #EXT-X-ENDLIST field exists, whether it is a live stream
     
     * [AUTO-TRANSLATED:f18e3c44]
     */
    bool isLive() const ;

    /**
     * #EXT-X-VERSION值，版本号
     * #EXT-X-VERSION value, version number
     
     * [AUTO-TRANSLATED:89a99b3d]
     */
    int getVersion() const;

    /**
     * #EXT-X-TARGETDURATION字段值
     * #EXT-X-TARGETDURATION field value
     
     * [AUTO-TRANSLATED:6720dc84]
     */
    int getTargetDur() const;

    /**
     * #EXT-X-MEDIA-SEQUENCE字段值，该m3u8序号
     * #EXT-X-MEDIA-SEQUENCE field value, the sequence number of this m3u8
     
     * [AUTO-TRANSLATED:1a75250a]
     */
    int64_t getSequence() const;

    /**
     * 内部是否含有子m3u8
     * Whether it contains sub-m3u8 internally
     
     * [AUTO-TRANSLATED:67b4a20c]
     */
    bool isM3u8Inner() const;

    /**
     * 得到总时间
     * Get the total time
     
     * [AUTO-TRANSLATED:aa5e797b]
     */
    float getTotalDuration() const;

protected:
    /**
     * 解析m3u8文件回调
     * @param is_m3u8_inner 该m3u8文件中是否包含多个hls地址
     * @param sequence ts序号
     * @param ts_list ts地址列表
     * @return 是否解析成功，返回false时，将导致HlsParser::parse返回false
     * Callback for parsing the m3u8 file
     * @param is_m3u8_inner Whether this m3u8 file contains multiple HLS addresses
     * @param sequence TS sequence number
     * @param ts_list TS address list
     * @return Whether the parsing is successful, returning false will cause HlsParser::parse to return false
     
     * [AUTO-TRANSLATED:be34e59f]
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
    // 每部是否有m3u8  [AUTO-TRANSLATED:c0d01536]
    // Whether each part has an m3u8
    bool _is_m3u8_inner = false;
};

}//namespace mediakit
#endif //HTTP_HLSPARSER_H
