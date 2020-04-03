/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
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
    uint32_t nextStampForStop();
    void setNextStampForStop(uint32_t ui32Stamp);
    bool seekTo(uint32_t ui32Stamp);
private:
    recursive_mutex _mtx;
    MultiMediaSourceMuxer::Ptr _mediaMuxer;
    uint32_t _seek_to;
    Ticker _seek_ticker;
    Ticker _alive;
    Timer::Ptr _timer;
    EventPoller::Ptr _poller;
    MP4Demuxer::Ptr _demuxer;
    bool _have_video = false;
};

} /* namespace mediakit */

#endif //ENABLE_MP4

namespace mediakit {
/**
 * 自动生成MP4Reader对象然后查找相关的MediaSource对象
 * @param strSchema 协议名
 * @param strVhost 虚拟主机
 * @param strApp 应用名
 * @param strId 流id
 * @param filePath 文件路径，如果为空则根据配置文件和上面参数自动生成，否则使用指定的文件
 * @param checkApp 是否检查app，防止服务器上文件被乱访问
 * @return MediaSource
 */
MediaSource::Ptr onMakeMediaSource(const string &strSchema,
                                   const string &strVhost,
                                   const string &strApp,
                                   const string &strId,
                                   const string &filePath = "",
                                   bool checkApp = true);
} /* namespace mediakit */
#endif /* SRC_MEDIAFILE_MEDIAREADER_H_ */
