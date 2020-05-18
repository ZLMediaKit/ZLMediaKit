/*
 * Copyright (c) 2020 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <cstdlib>
#include "HlsParser.h"
#include "Util/util.h"
#include "Common/Parser.h"
using namespace toolkit;
namespace mediakit {

bool HlsParser::parse(const string &http_url, const string &m3u8) {
    float extinf_dur = 0;
    ts_segment segment;
    map<int, ts_segment> ts_map;
    _total_dur = 0;
    _is_live = true;
    _is_m3u8_inner = false;
    int index = 0;

    auto lines = split(m3u8, "\n");
    for (auto &line : lines) {
        trim(line);
        if (line.size() < 2) {
            continue;
        }

        if ((_is_m3u8_inner || extinf_dur != 0) && line[0] != '#') {
            segment.duration = extinf_dur;
            if (line.find("http://") == 0 || line.find("https://") == 0) {
                segment.url = line;
            } else {
                if (line.find("/") == 0) {
                    segment.url = http_url.substr(0, http_url.find("/", 8)) + line;
                } else {
                    segment.url = http_url.substr(0, http_url.rfind("/") + 1) + line;
                }
            }
            if (!_is_m3u8_inner) {
                //ts按照先后顺序排序
                ts_map.emplace(index++, segment);
            } else {
                //子m3u8按照带宽排序
                ts_map.emplace(segment.bandwidth, segment);
            }
            extinf_dur = 0;
            continue;
        }

        _is_m3u8_inner = false;
        if (line.find("#EXTINF:") == 0) {
            sscanf(line.data(), "#EXTINF:%f,", &extinf_dur);
            _total_dur += extinf_dur;
            continue;
        }
        static const string s_stream_inf = "#EXT-X-STREAM-INF:";
        if (line.find(s_stream_inf) == 0) {
            _is_m3u8_inner = true;
            auto key_val = Parser::parseArgs(line.substr(s_stream_inf.size()), ",", "=");
            segment.program_id = atoi(key_val["PROGRAM-ID"].data());
            segment.bandwidth = atoi(key_val["BANDWIDTH"].data());
            sscanf(key_val["RESOLUTION"].data(), "%dx%d", &segment.width, &segment.height);
            continue;
        }

        if (line == "#EXTM3U") {
            _is_m3u8 = true;
            continue;
        }

        if (line.find("#EXT-X-ALLOW-CACHE:") == 0) {
            _allow_cache = (line.find(":YES") != string::npos);
            continue;
        }

        if (line.find("#EXT-X-VERSION:") == 0) {
            sscanf(line.data(), "#EXT-X-VERSION:%d", &_version);
            continue;
        }

        if (line.find("#EXT-X-TARGETDURATION:") == 0) {
            sscanf(line.data(), "#EXT-X-TARGETDURATION:%d", &_target_dur);
            continue;
        }

        if (line.find("#EXT-X-MEDIA-SEQUENCE:") == 0) {
            sscanf(line.data(), "#EXT-X-MEDIA-SEQUENCE:%lld", &_sequence);
            continue;
        }

        if (line.find("#EXT-X-ENDLIST") == 0) {
            //点播
            _is_live = false;
            continue;
        }
        continue;
    }

    if (_is_m3u8) {
        onParsed(_is_m3u8_inner, _sequence, ts_map);
    }
    return _is_m3u8;
}

bool HlsParser::isM3u8() const {
    return _is_m3u8;
}

bool HlsParser::isLive() const{
    return _is_live;
}

bool HlsParser::allowCache() const {
    return _allow_cache;
}

int HlsParser::getVersion() const {
    return _version;
}

int HlsParser::getTargetDur() const {
    return _target_dur;
}

int HlsParser::getSequence() const {
    return _sequence;
}

bool HlsParser::isM3u8Inner() const {
    return _is_m3u8_inner;
}

}//namespace mediakit