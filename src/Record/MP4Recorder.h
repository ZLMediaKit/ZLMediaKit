/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef MP4MAKER_H_
#define MP4MAKER_H_

#include <mutex>
#include <memory>
#include "Common/MediaSink.h"
#include "Record/Recorder.h"
#include "MP4Muxer.h"

namespace mediakit {

#ifdef ENABLE_MP4
class MP4Muxer;

class MP4Recorder final : public MediaSinkInterface {
public:
    using Ptr = std::shared_ptr<MP4Recorder>;

    MP4Recorder(const std::string &path, const std::string &vhost, const std::string &app, const std::string &stream_id, size_t max_second);
    ~MP4Recorder() override;

    /**
     * 重置所有Track
     */
    void resetTracks() override;

    /**
     * 输入frame
     */
    bool inputFrame(const Frame::Ptr &frame) override;

    /**
     * 刷新输出所有frame缓存
     */
    void flush() override;

    /**
     * 添加ready状态的track
     */
    bool addTrack(const Track::Ptr & track) override;

private:
    void createFile();
    void closeFile();
    void asyncClose();

private:
    bool _have_video = false;
    size_t _max_second;
    uint64_t _last_dts = 0;
    uint64_t _file_index = 0;
    std::string _folder_path;
    std::string _full_path;
    std::string _full_path_tmp;
    RecordInfo _info;
    MP4Muxer::Ptr _muxer;
    std::list<Track::Ptr> _tracks;
};

#endif ///ENABLE_MP4

} /* namespace mediakit */

#endif /* MP4MAKER_H_ */
