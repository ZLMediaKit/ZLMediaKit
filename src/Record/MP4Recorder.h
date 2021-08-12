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
#include "Player/PlayerBase.h"
#include "Util/util.h"
#include "Util/logger.h"
#include "Util/TimeTicker.h"
#include "Util/TimeTicker.h"
#include "Common/MediaSink.h"
#include "MP4Muxer.h"

using namespace toolkit;

namespace mediakit {

#ifdef ENABLE_MP4
class MP4Recorder : public MediaSinkInterface{
public:
    typedef std::shared_ptr<MP4Recorder> Ptr;

    MP4Recorder(const string &strPath,
                const string &strVhost,
                const string &strApp,
                const string &strStreamId,
                size_t max_second);
    virtual ~MP4Recorder();

    /**
     * 重置所有Track
     */
    void resetTracks() override;

    /**
     * 输入frame
     */
    void inputFrame(const Frame::Ptr &frame) override;

    /**
     * 添加ready状态的track
     */
    void addTrack(const Track::Ptr & track) override;
private:
    void createFile();
    void closeFile();
    void asyncClose();
private:
    bool _haveVideo = false;
    size_t _max_second;
    string _strPath;
    string _strFile;
    string _strFileTmp;
    RecordInfo _info;
    MP4Muxer::Ptr _muxer;
    list<Track::Ptr> _tracks;
	uint64_t _baseSec = 0;
};

#endif ///ENABLE_MP4

} /* namespace mediakit */

#endif /* MP4MAKER_H_ */
