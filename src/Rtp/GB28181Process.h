/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_GB28181ROCESS_H
#define ZLMEDIAKIT_GB28181ROCESS_H

#if defined(ENABLE_RTPPROXY)

#include "Decoder.h"
#include "ProcessInterface.h"
#include "Rtsp/RtpCodec.h"
#include "Common/MediaSource.h"
namespace mediakit{

class RtpReceiverImp;
/* Rtp-> MediaSink
 本类根据pt解复用同一ssrc的rtp数据包，排序并解析帧，最终回调MediaSink接口
*/
class GB28181Process : public ProcessInterface {
public:
    typedef std::shared_ptr<GB28181Process> Ptr;

    GB28181Process(const MediaInfo &media_info, MediaSinkInterface *sink) 
        : _media_info(media_info), _interface(sink) {}
    ~GB28181Process() override = default;

    /**
     * 输入rtp
     * Rtp数据包进来, 根据pt先进RtpReceiverImp进行排序(onRtpSorted), 再进RtpCodec进行解码(onRtpDecode)，最终回调到MediaSink(inputFrame)
     * @param data rtp数据指针
     * @param data_len rtp数据长度
     * @return 是否解析成功
     */
    bool inputRtp(bool, const char *data, size_t data_len) override;

protected:
    void onRtpSorted(RtpPacket::Ptr rtp);

private:
    void onRtpDecode(const Frame::Ptr &frame);

private:
    MediaInfo _media_info;
    DecoderImp::Ptr _decoder;
    MediaSinkInterface *_interface;
    std::shared_ptr<FILE> _save_file_ps;
    // pt->RtpCodec 负责rtp包解码
    std::unordered_map<uint8_t, std::shared_ptr<RtpCodec> > _rtp_decoder;
    // pt->RtpReceiverImp 负责rtp包排序
    std::unordered_map<uint8_t, std::shared_ptr<RtpReceiverImp> > _rtp_receiver;
};

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
#endif //ZLMEDIAKIT_GB28181ROCESS_H
