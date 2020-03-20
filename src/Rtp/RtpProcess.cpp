/*
 * MIT License
 *
 * Copyright (c) 2019 Gemfield <gemfield@civilnet.cn>
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

#if defined(ENABLE_RTPPROXY)
#include "mpeg-ps.h"
#include "RtpProcess.h"
#include "Util/File.h"
#include "Extension/H265.h"
#include "Extension/AAC.h"

namespace mediakit{

/**
* 合并一些时间戳相同的frame
*/
class FrameMerger {
public:
    FrameMerger() = default;
    virtual ~FrameMerger() = default;

    void inputFrame(const Frame::Ptr &frame,const function<void(uint32_t dts,uint32_t pts,const Buffer::Ptr &buffer)> &cb){
        if (!_frameCached.empty() && _frameCached.back()->dts() != frame->dts()) {
            Frame::Ptr back = _frameCached.back();
            Buffer::Ptr merged_frame = back;
            if(_frameCached.size() != 1){
                string merged;
                _frameCached.for_each([&](const Frame::Ptr &frame){
                    merged.append(frame->data(),frame->size());
                });
                merged_frame = std::make_shared<BufferString>(std::move(merged));
            }
            cb(back->dts(),back->pts(),merged_frame);
            _frameCached.clear();
        }
        _frameCached.emplace_back(Frame::getCacheAbleFrame(frame));
    }
private:
    List<Frame::Ptr> _frameCached;
};

string printSSRC(uint32_t ui32Ssrc) {
    char tmp[9] = { 0 };
    ui32Ssrc = htonl(ui32Ssrc);
    uint8_t *pSsrc = (uint8_t *) &ui32Ssrc;
    for (int i = 0; i < 4; i++) {
        sprintf(tmp + 2 * i, "%02X", pSsrc[i]);
    }
    return tmp;
}

static string printAddress(const struct sockaddr *addr){
    return StrPrinter << inet_ntoa(((struct sockaddr_in *) addr)->sin_addr) << ":" << ntohs(((struct sockaddr_in *) addr)->sin_port);
}

RtpProcess::RtpProcess(uint32_t ssrc) {
    _ssrc = ssrc;
    _track = std::make_shared<SdpTrack>();
    _track->_interleaved = 0;
    _track->_samplerate = 90000;
    _track->_type = TrackVideo;
    _track->_ssrc = _ssrc;
    DebugL << printSSRC(_ssrc);

    GET_CONFIG(bool,toRtxp,General::kPublishToRtxp);
    GET_CONFIG(bool,toHls,General::kPublishToHls);
    GET_CONFIG(bool,toMP4,General::kPublishToMP4);

    _muxer = std::make_shared<MultiMediaSourceMuxer>(DEFAULT_VHOST,"rtp",printSSRC(_ssrc),0,toRtxp,toRtxp,toHls,toMP4);

    GET_CONFIG(string,dump_dir,RtpProxy::kDumpDir);
    {
        FILE *fp = !dump_dir.empty() ? File::createfile_file(File::absolutePath(printSSRC(_ssrc) + ".rtp",dump_dir).data(),"wb") : nullptr;
        if(fp){
            _save_file_rtp.reset(fp,[](FILE *fp){
                fclose(fp);
            });
        }
    }

    {
        FILE *fp = !dump_dir.empty() ? File::createfile_file(File::absolutePath(printSSRC(_ssrc) + ".mp2",dump_dir).data(),"wb") : nullptr;
        if(fp){
            _save_file_ps.reset(fp,[](FILE *fp){
                fclose(fp);
            });
        }
    }

    {
        FILE *fp = !dump_dir.empty() ? File::createfile_file(File::absolutePath(printSSRC(_ssrc) + ".video",dump_dir).data(),"wb") : nullptr;
        if(fp){
            _save_file_video.reset(fp,[](FILE *fp){
                fclose(fp);
            });
        }
    }
    _merger = std::make_shared<FrameMerger>();
}

RtpProcess::~RtpProcess() {
    if(_addr){
        DebugL << printSSRC(_ssrc) << " " << printAddress(_addr);
        delete _addr;
    }else{
        DebugL << printSSRC(_ssrc);
    }
}

bool RtpProcess::inputRtp(const char *data, int data_len,const struct sockaddr *addr,uint32_t *dts_out) {
    GET_CONFIG(bool,check_source,RtpProxy::kCheckSource);
    //检查源是否合法
    if(!_addr){
        _addr = new struct sockaddr;
        memcpy(_addr,addr, sizeof(struct sockaddr));
        DebugL << "RtpProcess(" << printSSRC(_ssrc) << ") bind to address:" << printAddress(_addr);
    }

    if(check_source && memcmp(_addr,addr,sizeof(struct sockaddr)) != 0){
        DebugL << "RtpProcess(" << printSSRC(_ssrc) << ") address dismatch:" << printAddress(addr) << " != " << printAddress(_addr);
        return false;
    }

    _last_rtp_time.resetTime();
    bool ret = handleOneRtp(0,_track,(unsigned char *)data,data_len);
    if(dts_out){
        *dts_out = _dts;
    }
    return ret;
}

//判断是否为ts负载
static inline bool checkTS(const uint8_t *packet, int bytes){
    return bytes % 188 == 0 && packet[0] == 0x47;
}

void RtpProcess::onRtpSorted(const RtpPacket::Ptr &rtp, int) {
    if(rtp->sequence != _sequence + 1){
        WarnL << rtp->sequence << " != " << _sequence << "+1";
    }
    _sequence = rtp->sequence;
    if(_save_file_rtp){
        uint16_t  size = rtp->size() - 4;
        size = htons(size);
        fwrite((uint8_t *) &size, 2, 1, _save_file_rtp.get());
        fwrite((uint8_t *) rtp->data() + 4, rtp->size() - 4, 1, _save_file_rtp.get());
    }
    decodeRtp(rtp->data() + 4 ,rtp->size() - 4);
}

void RtpProcess::onRtpDecode(const uint8_t *packet, int bytes, uint32_t timestamp, int flags) {
    if(_save_file_ps){
        fwrite((uint8_t *)packet,bytes, 1, _save_file_ps.get());
    }

    if(!_decoder){
        //创建解码器
        if(checkTS(packet, bytes)){
            //猜测是ts负载
            InfoL << "judged to be TS: " << printSSRC(_ssrc);
            _decoder = Decoder::createDecoder(Decoder::decoder_ts);
        }else{
            //猜测是ps负载
            InfoL << "judged to be PS: " << printSSRC(_ssrc);
            _decoder = Decoder::createDecoder(Decoder::decoder_ps);
        }
        _decoder->setOnDecode([this](int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,int bytes){
            onDecode(stream,codecid,flags,pts,dts,data,bytes);
        });
    }

    auto ret = _decoder->input((uint8_t *)packet,bytes);
    if(ret != bytes){
        WarnL << ret << " != " << bytes << " " << flags;
    }
}

#define SWITCH_CASE(codec_id) case codec_id : return #codec_id
static const char *getCodecName(int codec_id) {
    switch (codec_id) {
        SWITCH_CASE(STREAM_VIDEO_MPEG4);
        SWITCH_CASE(STREAM_VIDEO_H264);
        SWITCH_CASE(STREAM_VIDEO_H265);
        SWITCH_CASE(STREAM_VIDEO_SVAC);
        SWITCH_CASE(STREAM_AUDIO_MP3);
        SWITCH_CASE(STREAM_AUDIO_AAC);
        SWITCH_CASE(STREAM_AUDIO_G711);
        SWITCH_CASE(STREAM_AUDIO_G722);
        SWITCH_CASE(STREAM_AUDIO_G723);
        SWITCH_CASE(STREAM_AUDIO_G729);
        SWITCH_CASE(STREAM_AUDIO_SVAC);
        default:
            return "unknown codec";
    }
}

void RtpProcess::onDecode(int stream,int codecid,int flags,int64_t pts,int64_t dts,const void *data,int bytes) {
    pts /= 90;
    dts /= 90;
    _stamps[codecid].revise(dts,pts,dts,pts,false);
    _dts = dts;

    switch (codecid) {
        case STREAM_VIDEO_H264: {
            if (!_codecid_video) {
                //获取到视频
                _codecid_video = codecid;
                InfoL << "got video track: H264";
                auto track = std::make_shared<H264Track>();
                _muxer->addTrack(track);
            }

            if (codecid != _codecid_video) {
                WarnL << "video track change to H264 from codecid:" << getCodecName(_codecid_video);
                return;
            }

            if(_save_file_video){
                fwrite((uint8_t *)data,bytes, 1, _save_file_video.get());
            }
            auto frame = std::make_shared<H264FrameNoCacheAble>((char *) data, bytes, dts, pts,0);
            _merger->inputFrame(frame,[this](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer) {
                _muxer->inputFrame(std::make_shared<H264FrameNoCacheAble>(buffer->data(), buffer->size(), dts, pts,4));
            });
            break;
        }

        case STREAM_VIDEO_H265: {
            if (!_codecid_video) {
                //获取到视频
                _codecid_video = codecid;
                InfoL << "got video track: H265";
                auto track = std::make_shared<H265Track>();
                _muxer->addTrack(track);
            }
            if (codecid != _codecid_video) {
                WarnL << "video track change to H265 from codecid:" << getCodecName(_codecid_video);
                return;
            }
            if(_save_file_video){
                fwrite((uint8_t *)data,bytes, 1, _save_file_video.get());
            }
            auto frame = std::make_shared<H265FrameNoCacheAble>((char *) data, bytes, dts, pts, 0);
            _merger->inputFrame(frame,[this](uint32_t dts, uint32_t pts, const Buffer::Ptr &buffer) {
                _muxer->inputFrame(std::make_shared<H265FrameNoCacheAble>(buffer->data(), buffer->size(), dts, pts, 4));
            });
            break;
        }

        case STREAM_AUDIO_AAC: {
            if (!_codecid_audio) {
                //获取到音频
                _codecid_audio = codecid;
                InfoL << "got audio track: AAC";
                auto track = std::make_shared<AACTrack>();
                _muxer->addTrack(track);
            }

            if (codecid != _codecid_audio) {
                WarnL << "audio track change to AAC from codecid:" << getCodecName(_codecid_audio);
                return;
            }
            _muxer->inputFrame(std::make_shared<AACFrameNoCacheAble>((char *) data, bytes, dts, 7));
            break;
        }
        default:
            if(codecid != 0){
                WarnL << "unsupported codec type:" << getCodecName(codecid);
            }
            return;
    }
}

bool RtpProcess::alive() {
    GET_CONFIG(int,timeoutSec,RtpProxy::kTimeoutSec)
    if(_last_rtp_time.elapsedTime() / 1000 < timeoutSec){
        return true;
    }
    return false;
}

string RtpProcess::get_peer_ip() {
    return inet_ntoa(((struct sockaddr_in *) _addr)->sin_addr);
}

uint16_t RtpProcess::get_peer_port() {
    return ntohs(((struct sockaddr_in *) _addr)->sin_port);
}

int RtpProcess::totalReaderCount(){
    return _muxer->totalReaderCount();
}

void RtpProcess::setListener(const std::weak_ptr<MediaSourceEvent> &listener){
    _muxer->setListener(listener);
}


}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)