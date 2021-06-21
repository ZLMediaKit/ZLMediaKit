/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
#include "Extension/H264Rtp.h"
#include "Extension/Factory.h"

namespace mediakit{

//判断是否为ts负载
static inline bool checkTS(const uint8_t *packet, size_t bytes){
    return bytes % TS_PACKET_SIZE == 0 && packet[0] == TS_SYNC_BYTE;
}

class RtpReceiverImp : public RtpReceiver {
public:
    using Ptr = std::shared_ptr<RtpReceiverImp>;
    RtpReceiverImp( function<void(RtpPacket::Ptr rtp)> cb,  function<void(const RtpPacket::Ptr &rtp)> cb_before = nullptr){
        _on_sort = std::move(cb);
        _on_before_sort = std::move(cb_before);
    }

    ~RtpReceiverImp() override = default;

    bool inputRtp(TrackType type, int samplerate, uint8_t *ptr, size_t len){
        return handleOneRtp((int) type, type, samplerate, ptr, len);
    }

protected:
    void onRtpSorted(RtpPacket::Ptr rtp, int track_index) override {
        _on_sort(std::move(rtp));
    }

    void onBeforeRtpSorted(const RtpPacket::Ptr &rtp, int track_index) override {
        if (_on_before_sort) {
            _on_before_sort(rtp);
        }
    }

private:
    function<void(RtpPacket::Ptr rtp)> _on_sort;
    function<void(const RtpPacket::Ptr &rtp)> _on_before_sort;
};

///////////////////////////////////////////////////////////////////////////////////////////

GB28181Process::GB28181Process(const MediaInfo &media_info, MediaSinkInterface *interface) {
    assert(interface);
    _media_info = media_info;
    _interface = interface;
}

GB28181Process::~GB28181Process() {}

bool GB28181Process::inputRtp(bool, const char *data, size_t data_len) {
    RtpHeader *header = (RtpHeader *) data;
    auto &ref = _rtp_receiver[header->pt];
    if (!ref) {
        ref = std::make_shared<RtpReceiverImp>([this](RtpPacket::Ptr rtp) {
            onRtpSorted(std::move(rtp));
        });
    }
    return ref->inputRtp(TrackVideo, RtpPayload::getClockRate(header->pt), (unsigned char *) data, data_len);
}

void GB28181Process::onRtpSorted(RtpPacket::Ptr rtp) {
    auto pt = rtp->getHeader()->pt;
    auto &ref = _rtp_decoder[pt];
    if (!ref) {
        switch (pt) {
            case 99: {
                //H265负载
                auto track = std::make_shared<H265Track>();
                _interface->addTrack(track);
                ref = Factory::getRtpDecoderByTrack(track);
                break;
            }
            case 98: {
                //H264负载
                auto track = std::make_shared<H264Track>();
                _interface->addTrack(track);
                ref = Factory::getRtpDecoderByTrack(track);
                break;
            }
            case 0: {
                //CodecG711U
                auto track = std::make_shared<G711Track>(CodecG711U, 8000, 1, 16);
                _interface->addTrack(track);
                ref = Factory::getRtpDecoderByTrack(track);
                break;
            }

            case 8: {
                //CodecG711A
                auto track = std::make_shared<G711Track>(CodecG711A, 8000, 1, 16);
                _interface->addTrack(track);
                ref = Factory::getRtpDecoderByTrack(track);
                break;
            }

            default: {
                if (pt != 33 && pt != 96) {
                    WarnL << "rtp payload type未识别(" << (int) pt << "),已按ts或ps负载处理";
                }
                //ts或ps负载
                ref = std::make_shared<CommonRtpDecoder>(CodecInvalid, 32 * 1024);
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
        }

        //设置frame回调
        ref->addDelegate(std::make_shared<FrameWriterInterfaceHelper>([this](const Frame::Ptr &frame) {
            onRtpDecode(frame);
        }));
    }

    //解码rtp
    ref->inputRtp(rtp, false);
}

const char *GB28181Process::onSearchPacketTail(const char *packet,size_t bytes){
    try {
        auto ret = _decoder->input((uint8_t *) packet, bytes);
        if (ret >= 0) {
            //解析成功全部或部分
            return packet + ret;
        }
        //解析失败，丢弃所有数据
        return packet + bytes;
    } catch (std::exception &ex) {
        InfoL << "解析ps或ts异常: bytes=" << bytes
              << " ,exception=" << ex.what()
              << " ,hex=" << hexdump((uint8_t *) packet, MIN(bytes,32));
        if (remainDataSize() > 256 * 1024) {
            //缓存太多数据无法处理则上抛异常
            throw;
        }
        return nullptr;
    }
}

void GB28181Process::onRtpDecode(const Frame::Ptr &frame) {
    if (frame->getCodecId() != CodecInvalid) {
        //这里不是ps或ts
        _interface->inputFrame(frame);
        return;
    }

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
