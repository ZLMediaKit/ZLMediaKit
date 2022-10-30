/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPSELECTOR_H
#define ZLMEDIAKIT_RTPSELECTOR_H

#if defined(ENABLE_RTPPROXY)
#include <stdint.h>
#include <mutex>
#include <unordered_map>
#include "RtpProcess.h"
#include "Common/MediaSource.h"

namespace mediakit{

class RtpSelector;
class RtpProcessHelper : public MediaSourceEvent , public std::enable_shared_from_this<RtpProcessHelper> {
public:
    typedef std::shared_ptr<RtpProcessHelper> Ptr;
    RtpProcessHelper(const std::string &stream_id, const std::weak_ptr<RtpSelector > &parent);
    ~RtpProcessHelper();
    void attachEvent();
    RtpProcess::Ptr & getProcess();

protected:
    // 通知其停止推流
    bool close(MediaSource &sender) override;

private:
    std::string _stream_id;
    RtpProcess::Ptr _process;
    std::weak_ptr<RtpSelector> _parent;
};

class RtpSelector : public std::enable_shared_from_this<RtpSelector>{
public:
    RtpSelector() = default;
    ~RtpSelector() = default;

    static bool getSSRC(const char *data,size_t data_len, uint32_t &ssrc);
    static RtpSelector &Instance();

    /**
     * 清空所有对象
     */
    void clear();

    /**
     * 获取一个rtp处理器
     * @param stream_id 流id
     * @param makeNew 不存在时是否新建, 该参数为true时，必须确保之前未创建同名对象
     * @return rtp处理器
     */
    RtpProcess::Ptr getProcess(const std::string &stream_id, bool makeNew);

    /**
     * 删除rtp处理器
     * @param stream_id 流id
     * @param ptr rtp处理器指针
     */
    void delProcess(const std::string &stream_id, const RtpProcess *ptr);

private:
    void onManager();
    void createTimer();

private:
    toolkit::Timer::Ptr _timer;
    std::recursive_mutex _mtx_map;
    std::unordered_map<std::string,RtpProcessHelper::Ptr> _map_rtp_process;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_RTPSELECTOR_H
