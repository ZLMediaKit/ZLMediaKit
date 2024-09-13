﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HLSMAKERIMP_H
#define HLSMAKERIMP_H

#include <memory>
#include <string>
#include <stdlib.h>
#include "HlsMaker.h"
#include "HlsMediaSource.h"

namespace mediakit {

class HlsMakerImp : public HlsMaker {
public:
    HlsMakerImp(bool is_fmp4, const std::string &m3u8_file, const std::string &params, uint32_t bufSize = 64 * 1024,
                float seg_duration = 5, uint32_t seg_number = 3, bool seg_keep = false);
    ~HlsMakerImp() override;

    /**
     * 设置媒体信息
     * Set media information
     
     * [AUTO-TRANSLATED:d205db9f]
     */
    void setMediaSource(const MediaTuple& tuple);

    /**
     * 获取MediaSource
     * @return
     * Get MediaSource
     * @return
     
     * [AUTO-TRANSLATED:af916433]
     */
    HlsMediaSource::Ptr getMediaSource() const;

     /**
      * 清空缓存
      * Clear cache
      
      
      * [AUTO-TRANSLATED:f872d7e2]
      */
     void clearCache();

protected:
    std::string onOpenSegment(uint64_t index) override ;
    void onDelSegment(uint64_t index) override;
    void onWriteInitSegment(const char *data, size_t len) override;
    void onWriteSegment(const char *data, size_t len) override;
    void onWriteHls(const std::string &data, bool include_delay) override;
    void onFlushLastSegment(uint64_t duration_ms) override;

private:
    std::shared_ptr<FILE> makeFile(const std::string &file,bool setbuf = false);
    void clearCache(bool immediately, bool eof);

private:
    int _buf_size;
    std::string _params;
    std::string _path_hls;
    std::string _path_hls_delay;
    std::string _path_init;
    std::string _path_prefix;
    RecordInfo _info;
    std::shared_ptr<FILE> _file;
    std::shared_ptr<char> _file_buf;
    HlsMediaSource::Ptr _media_src;
    toolkit::EventPoller::Ptr _poller;
    std::map<uint64_t/*index*/,std::string/*file_path*/> _segment_file_paths;
};

}//namespace mediakit
#endif //HLSMAKERIMP_H
