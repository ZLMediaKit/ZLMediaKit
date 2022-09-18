/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef FFMPEG_SOURCE_H
#define FFMPEG_SOURCE_H

#include <mutex>
#include <memory>
#include <functional>
#include "Process.h"
#include "Util/TimeTicker.h"
#include "Network/Socket.h"
#include "Common/MediaSource.h"

namespace FFmpeg {
    extern const std::string kSnap;
}

class FFmpegSnap {
public:
    using onSnap = std::function<void(bool success, const std::string &err_msg)>;
    /// 创建截图
    /// \param play_url 播放url地址，只要FFmpeg支持即可
    /// \param save_path 截图jpeg文件保存路径
    /// \param timeout_sec 生成截图超时时间(防止阻塞太久)
    /// \param cb 生成截图成功与否回调
    static void makeSnap(const std::string &play_url, const std::string &save_path, float timeout_sec, const onSnap &cb);

private:
    FFmpegSnap() = delete;
    ~FFmpegSnap() = delete;
};

class FFmpegSource : public std::enable_shared_from_this<FFmpegSource> , public mediakit::MediaSourceEventInterceptor{
public:
    using Ptr = std::shared_ptr<FFmpegSource>;
    using onPlay = std::function<void(const toolkit::SockException &ex)>;

    FFmpegSource();
    ~FFmpegSource();

    /**
     * 设置主动关闭回调
     */
    void setOnClose(const std::function<void()> &cb);

    /**
     * 开始播放url
     * @param ffmpeg_cmd_key FFmpeg拉流命令配置项key，用户可以在配置文件中同时设置多个命令参数模板
     * @param src_url FFmpeg拉流地址
     * @param dst_url FFmpeg推流地址
     * @param timeout_ms 等待结果超时时间，单位毫秒
     * @param cb 成功与否回调
     */
    void play(const std::string &ffmpeg_cmd_key, const std::string &src_url, const std::string &dst_url, int timeout_ms, const onPlay &cb);

    /**
     * 设置录制
     * @param enable_hls 是否开启hls直播或录制
     * @param enable_mp4 是否录制mp4
     */
    void setupRecordFlag(bool enable_hls, bool enable_mp4);

private:
    void findAsync(int maxWaitMS ,const std::function<void(const mediakit::MediaSource::Ptr &src)> &cb);
    void startTimer(int timeout_ms);
    void onGetMediaSource(const mediakit::MediaSource::Ptr &src);

    ///////MediaSourceEvent override///////
    // 关闭
    bool close(mediakit::MediaSource &sender) override;
    // 获取媒体源类型
    mediakit::MediaOriginType getOriginType(mediakit::MediaSource &sender) const override;
    //获取媒体源url或者文件路径
    std::string getOriginUrl(mediakit::MediaSource &sender) const override;
    // 获取媒体源客户端相关信息
    std::shared_ptr<toolkit::SockInfo> getOriginSock(mediakit::MediaSource &sender) const override;

private:
    bool _enable_hls = false;
    bool _enable_mp4 = false;
    Process _process;
    toolkit::Timer::Ptr _timer;
    toolkit::EventPoller::Ptr _poller;
    mediakit::MediaInfo _media_info;
    std::string _src_url;
    std::string _dst_url;
    std::string _ffmpeg_cmd_key;
    std::function<void()> _onClose;
    toolkit::Ticker _replay_ticker;
};


#endif //FFMPEG_SOURCE_H
