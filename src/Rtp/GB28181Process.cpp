/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "GB28181Process.h"
#include "Util/File.h"
#include "Http/HttpTSPlayer.h"
#include "Extension/CommonRtp.h"

namespace mediakit{

//判断是否为ts负载
static inline bool checkTS(const uint8_t *packet, int bytes){
    return bytes % TS_PACKET_SIZE == 0 && packet[0] == TS_SYNC_BYTE;
}

GB28181Process::GB28181Process(const MediaInfo &media_info, MediaSinkInterface *interface) {
    assert(interface);
    _media_info = media_info;
    _interface = interface;
}

GB28181Process::~GB28181Process() {}

bool GB28181Process::inputRtp(bool, const char *data, int data_len) {
    return handleOneRtp(0, TrackVideo, 90000, (unsigned char *) data, data_len);
}

void GB28181Process::onRtpSorted(const RtpPacket::Ptr &rtp, int) {
    if (!_rtp_decoder) {
        switch (rtp->PT) {
            case 33:
            case 96: {
                //ts或ps负载
                _rtp_decoder = std::make_shared<CommonRtpDecoder>(CodecInvalid, 256 * 1024);

                //设置dump目录
                GET_CONFIG(string, dump_dir, RtpProxy::kDumpDir);
                if (!dump_dir.empty()) {
                    auto save_path = File::absolutePath(_media_info._streamid + ".mp2", dump_dir);
                    _save_file_ps.reset(File::create_file(save_path.data(), "wb"), [](FILE *fp) {
                        if (fp) {
                            fclose(fp);
                        }
                    });
                }
                break;
            }

            default:
                WarnL << "不支持的rtp负载类型:" << (int) rtp->PT;
                return;
        }

        //设置frame回调
        _rtp_decoder->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([this](const Frame::Ptr &frame) {
            onRtpDecode(frame);
        }));
    }

    //解码rtp
    _rtp_decoder->inputRtp(rtp, false);
}

const char *GB28181Process::onSearchPacketTail(const char *packet,int bytes){
    try {
        auto ret = _decoder->input((uint8_t *) packet, bytes);
        if (ret > 0) {
            return packet + ret;
        }
        return nullptr;
    } catch (std::exception &ex) {
        InfoL << "解析ps或ts异常: bytes=" << bytes
              << " ,exception=" << ex.what()
              << " ,hex=" << hexdump((uint8_t *) packet, bytes);
        if (remainDataSize() > 256 * 1024) {
            //缓存太多数据无法处理则上抛异常
            throw;
        }
        return nullptr;
    }
}

void GB28181Process::onRtpDecode(const Frame::Ptr &frame) {
    //这是TS或PS
    if (_save_file_ps) {
        fwrite(frame->data(), frame->size(), 1, _save_file_ps.get());
    }

    if (!_decoder) {
        //创建解码器
        if (checkTS((uint8_t *) frame->data(), frame->size())) {
            //猜测是ts负载
            InfoL << _media_info._streamid << " judged to be TS";
            _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ts, _interface);
        } else {
            //猜测是ps负载
            InfoL << _media_info._streamid << " judged to be PS";
            _decoder = DecoderImp::createDecoder(DecoderImp::decoder_ps, _interface);
        }
    }

    if (_decoder) {
        HttpRequestSplitter::input(frame->data(), frame->size());
    }
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)