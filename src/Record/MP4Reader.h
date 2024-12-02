/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_MEDIAFILE_MEDIAREADER_H_
#define SRC_MEDIAFILE_MEDIAREADER_H_
#ifdef ENABLE_MP4

#include "MP4Demuxer.h"
#include "Common/MultiMediaSourceMuxer.h"

namespace mediakit {

class MP4Reader : public std::enable_shared_from_this<MP4Reader>, public MediaSourceEvent {
public:
    using Ptr = std::shared_ptr<MP4Reader>;

    /**
     * 点播一个mp4文件，使之转换成MediaSource流媒体
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id,置空时,只解复用mp4,但是不生成MediaSource
     * @param file_path 文件路径，如果为空则根据配置文件和上面参数自动生成，否则使用指定的文件
     * Play an mp4 file and convert it to a MediaSource stream
     * @param vhost Virtual host
     * @param app Application name
     * @param stream_id Stream id, if empty, only demultiplex mp4, but not generate MediaSource
     * @param file_path File path, if empty, it will be automatically generated according to the configuration file and the above parameters, otherwise use the specified file
     
     * [AUTO-TRANSLATED:2faeb5db]
     */
    MP4Reader(const MediaTuple &tuple, const std::string &file_path = "", toolkit::EventPoller::Ptr poller = nullptr);

    MP4Reader(const MediaTuple &tuple, const std::string &file_path, const ProtocolOption &option, toolkit::EventPoller::Ptr poller = nullptr);

    /**
     * 开始解复用MP4文件
     * @param sample_ms 每次读取文件数据量，单位毫秒，置0时采用配置文件配置
     * @param ref_self 是否让定时器引用此对象本身，如果无其他对象引用本身，在不循环读文件时，读取文件结束后本对象将自动销毁
     * @param file_repeat 是否循环读取文件，如果配置文件设置为循环读文件，此参数无效
     * Start demultiplexing the MP4 file
     * @param sample_ms The amount of file data read each time, in milliseconds, set to 0 to use the configuration file configuration
     * @param ref_self Whether to let the timer reference this object itself, if there is no other object referencing itself, when not looping to read the file, after reading the file, this object will be automatically destroyed
     * @param file_repeat Whether to loop to read the file, if the configuration file is set to loop to read the file, this parameter is invalid
     
     * [AUTO-TRANSLATED:2164a99d]
     */
    void startReadMP4(uint64_t sample_ms = 0, bool ref_self = true,  bool file_repeat = false);

    /**
     * 停止解复用MP4定时器
     * Stop demultiplexing the MP4 timer
     
     * [AUTO-TRANSLATED:45fb1ef7]
     */
    void stopReadMP4();

    /**
     * 获取mp4解复用器
     * Get the mp4 demultiplexer
     
     
     * [AUTO-TRANSLATED:4f0dfc29]
     */
    const MultiMP4Demuxer::Ptr& getDemuxer() const;

private:
    //MediaSourceEvent override
    bool seekTo(MediaSource &sender,uint32_t stamp) override;
    bool pause(MediaSource &sender, bool pause) override;
    bool speed(MediaSource &sender, float speed) override;

    bool close(MediaSource &sender) override;
    MediaOriginType getOriginType(MediaSource &sender) const override;
    std::string getOriginUrl(MediaSource &sender) const override;
    toolkit::EventPoller::Ptr getOwnerPoller(MediaSource &sender) override;

    bool readSample();
    bool readNextSample();
    uint32_t getCurrentStamp();
    void setCurrentStamp(uint32_t stamp);
    bool seekTo(uint32_t stamp_seek);

    void setup(const MediaTuple &tuple, const std::string &file_path, const ProtocolOption &option, toolkit::EventPoller::Ptr poller);

private:
    bool _file_repeat = false;
    bool _have_video = false;
    bool _paused = false;
    float _speed = 1.0;
    uint32_t _last_dts = 0;
    uint32_t _seek_to = 0;
    std::string _file_path;
    std::recursive_mutex _mtx;
    toolkit::Ticker _seek_ticker;
    toolkit::Timer::Ptr _timer;
    MultiMP4Demuxer::Ptr _demuxer;
    MultiMediaSourceMuxer::Ptr _muxer;
    toolkit::EventPoller::Ptr _poller;
};

} /* namespace mediakit */
#endif //ENABLE_MP4
#endif /* SRC_MEDIAFILE_MEDIAREADER_H_ */
