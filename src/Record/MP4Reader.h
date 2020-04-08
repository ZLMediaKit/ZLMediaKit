/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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
using namespace toolkit;
namespace mediakit {

class MP4Reader : public std::enable_shared_from_this<MP4Reader> ,public MediaSourceEvent{
public:
    typedef std::shared_ptr<MP4Reader> Ptr;
    virtual ~MP4Reader() = default;

    /**
     * 流化一个mp4文件，使之转换成RtspMediaSource和RtmpMediaSource
     * @param strVhost 虚拟主机
     * @param strApp 应用名
     * @param strId 流id
     * @param filePath 文件路径，如果为空则根据配置文件和上面参数自动生成，否则使用指定的文件
     */
    MP4Reader(const string &strVhost,const string &strApp, const string &strId,const string &filePath = "");

    /**
     * 开始流化MP4文件，需要指出的是，MP4Reader对象一经过调用startReadMP4方法，它的强引用会自持有，
     * 意思是在文件流化结束之前或中断之前,MP4Reader对象是不会被销毁的(不管有没有被外部对象持有)
     */
    void startReadMP4();
private:
    //MediaSourceEvent override
    bool seekTo(MediaSource &sender,uint32_t ui32Stamp) override;
    bool close(MediaSource &sender,bool force) override;
    int totalReaderCount(MediaSource &sender) override;

    bool readSample();
    uint32_t getCurrentStamp();
    void setCurrentStamp(uint32_t ui32Stamp);
    bool seekTo(uint32_t ui32Stamp);
private:
    recursive_mutex _mtx;
    MultiMediaSourceMuxer::Ptr _mediaMuxer;
    uint32_t _seek_to;
    Ticker _seek_ticker;
    Timer::Ptr _timer;
    EventPoller::Ptr _poller;
    MP4Demuxer::Ptr _demuxer;
    bool _have_video = false;
};

} /* namespace mediakit */
#endif //ENABLE_MP4
#endif /* SRC_MEDIAFILE_MEDIAREADER_H_ */
