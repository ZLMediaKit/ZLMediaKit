/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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

class MP4Info {
public:
    time_t ui64StartedTime; //GMT标准时间，单位秒
    time_t ui64TimeLen;//录像长度，单位秒
    off_t ui64FileSize;//文件大小，单位BYTE
    string strFilePath;//文件路径
    string strFileName;//文件名称
    string strFolder;//文件夹路径
    string strUrl;//播放路径
    string strAppName;//应用名称
    string strStreamId;//流ID
    string strVhost;//vhost
};

#ifdef ENABLE_MP4
class MP4Recorder : public MediaSinkInterface{
public:
    typedef std::shared_ptr<MP4Recorder> Ptr;

    MP4Recorder(const string &strPath,
                const string &strVhost,
                const string &strApp,
                const string &strStreamId);
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
    string _strPath;
    string _strFile;
    string _strFileTmp;
    Ticker _createFileTicker;
    MP4Info _info;
    bool _haveVideo = false;
    MP4Muxer::Ptr _muxer;
    list<Track::Ptr> _tracks;
};

#endif ///ENABLE_MP4

} /* namespace mediakit */

#endif /* MP4MAKER_H_ */
