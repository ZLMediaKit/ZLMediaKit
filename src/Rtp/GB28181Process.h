﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_GB28181ROCESS_H
#define ZLMEDIAKIT_GB28181ROCESS_H

#if defined(ENABLE_RTPPROXY)

#include "Decoder.h"
#include "ProcessInterface.h"
#include "Http/HttpRequestSplitter.h"
#include "Rtsp/RtpCodec.h"
#include "Common/MediaSource.h"

namespace mediakit{

class RtpReceiverImp;
class GB28181Process : public ProcessInterface {
public:
    using Ptr = std::shared_ptr<GB28181Process>;

    GB28181Process(const MediaInfo &media_info, MediaSinkInterface *sink);

    /**
     * 输入rtp
     * @param data rtp数据指针
     * @param data_len rtp数据长度
     * @return 是否解析成功
     */
    bool inputRtp(bool, const char *data, size_t data_len) override;

    /**
     * 刷新输出所有缓存
     */
    void flush() override;

protected:
    void onRtpSorted(RtpPacket::Ptr rtp);

private:
    void onRtpDecode(const Frame::Ptr &frame);

private:
    MediaInfo _media_info;
    DecoderImp::Ptr _decoder;
    MediaSinkInterface *_interface;
    std::shared_ptr<FILE> _save_file_ps;
    std::unordered_map<uint8_t, RtpCodec::Ptr> _rtp_decoder;
    std::unordered_map<uint8_t, std::shared_ptr<RtpReceiverImp> > _rtp_receiver;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_GB28181ROCESS_H
