/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
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
     */
    MP4Reader(const std::string &vhost, const std::string &app, const std::string &stream_id, const std::string &file_path = "");
    ~MP4Reader() override = default;

    /**
     * 开始解复用MP4文件
     * @param poller 解复用mp4定时器所绑定线程，置空则随机采用一条后台线程
     * @param sample_ms 每次读取文件数据量，单位毫秒，置0时采用配置文件配置
     * @param ref_self 是否让定时器引用此对象本身，如果无其他对象引用本身，在不循环读文件时，读取文件结束后本对象将自动销毁
     * @param file_repeat 是否循环读取文件，如果配置文件设置为循环读文件，此参数无效
     */
    void startReadMP4(const toolkit::EventPoller::Ptr &poller = nullptr, uint64_t sample_ms = 0, bool ref_self = true,  bool file_repeat = false);

    /**
     * 停止解复用MP4定时器
     */
    void stopReadMP4();

    /**
     * 获取mp4解复用器
     */
    const MP4Demuxer::Ptr& getDemuxer() const;

private:
    //MediaSourceEvent override
    bool seekTo(MediaSource &sender,uint32_t stamp) override;
    bool pause(MediaSource &sender, bool pause) override;
    bool speed(MediaSource &sender, float speed) override;

    bool close(MediaSource &sender,bool force) override;
    int totalReaderCount(MediaSource &sender) override;
    MediaOriginType getOriginType(MediaSource &sender) const override;
    std::string getOriginUrl(MediaSource &sender) const override;

    bool readSample();
    bool readNextSample();
    uint32_t getCurrentStamp();
    void setCurrentStamp(uint32_t stamp);
    bool seekTo(uint32_t stamp_seek);

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
    MP4Demuxer::Ptr _demuxer;
    MultiMediaSourceMuxer::Ptr _muxer;
};

} /* namespace mediakit */
#endif //ENABLE_MP4
#endif /* SRC_MEDIAFILE_MEDIAREADER_H_ */
