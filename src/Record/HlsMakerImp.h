/*
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
#include <map>
#include <string>
#include <stdlib.h>
#include <utility>
#include "HlsMaker.h"
#include "HlsMediaSource.h"

namespace mediakit {

class HlsMakerImp : public HlsMaker {
public:
    HlsMakerImp(bool is_fmp4, const std::string &m3u8_file, const std::string &params, uint32_t bufSize = 64 * 1024,
                float seg_duration = 5, uint32_t seg_number = 3, bool seg_keep = false,
                const std::string &fmp4_seg_ext = ".mp4", float seg_max_duration = 0);
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
    bool onWriteInitSegment(const std::string &candidate_uri, const char *data, size_t len) override;
    void onWriteSegment(const char *data, size_t len) override;
    void onWriteHls(const std::string &data, bool include_delay) override;
    void onFlushLastSegment(uint64_t duration_ms, bool discontinuity, const std::string &init_segment) override;

private:
    struct SegmentFileInfo {
        SegmentFileInfo(std::string path, std::string init)
            : file_path(std::move(path)), init_segment(std::move(init)) {}

        std::string file_path;
        std::string init_segment;
    };

    struct DirSegmentInfo {
        DirSegmentInfo(uint64_t duration, std::string file, bool discontinuous, std::string init)
            : duration_ms(duration)
            , file_name(std::move(file))
            , discontinuity(discontinuous)
            , init_segment(std::move(init)) {}

        uint64_t duration_ms;
        std::string file_name;
        bool discontinuity;
        std::string init_segment;
    };

    std::shared_ptr<FILE> makeFile(const std::string &file,bool setbuf = false);
    void clearCache(bool immediately, bool eof);
    void cleanupPreviousUnusedInitSegment(const std::string &next_init_segment);
    void cleanupUnusedInitSegments(const std::string &protected_init_segment = {});
    void saveCurrentDir();

private:
    int _buf_size;
    std::string _params;
    std::string _fmp4_seg_ext;
    std::string _path_hls;
    std::string _path_hls_delay;
    std::string _path_prefix;
    std::string _current_dir;
    RecordInfo _info;
    std::shared_ptr<FILE> _file;
    std::shared_ptr<char> _file_buf;
    HlsMediaSource::Ptr _media_src;
    toolkit::EventPoller::Ptr _poller;
    std::map<uint64_t/*index*/, SegmentFileInfo> _segment_files;
    std::map<std::string/*uri*/, std::string/*file_path*/> _init_segment_paths;
    std::map<std::string/*uri*/, uint64_t/*last_segment_index*/> _init_segment_last_indexes;
    std::map<std::string/*uri*/, std::string/*data*/> _init_segments;
    // VOD/keep 最近成功写入的 init 及其媒体片引用状态。
    // Most recently written VOD/keep init and whether a media segment references it.
    std::string _last_written_init_segment;
    bool _last_written_init_segment_referenced = false;
    std::deque<DirSegmentInfo> _current_dir_seg_list;
};

}//namespace mediakit
#endif //HLSMAKERIMP_H
