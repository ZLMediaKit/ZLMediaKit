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

#ifdef ENABLE_MP4RECORD

#include "MP4Muxer.h"
#include "Util/File.h"
#include "Common/config.h"

namespace mediakit{

#if defined(_WIN32) || defined(_WIN64)
#define fseek64 _fseeki64
#define ftell64 _ftelli64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

void MP4MuxerBase::init(int flags) {
    static struct mov_buffer_t s_io = {
            [](void* ctx, void* data, uint64_t bytes) {
                MP4MuxerBase *thiz = (MP4MuxerBase *)ctx;
                return thiz->onRead(data,bytes);
            },
            [](void* ctx, const void* data, uint64_t bytes){
                MP4MuxerBase *thiz = (MP4MuxerBase *)ctx;
                return thiz->onWrite(data,bytes);
            },
            [](void* ctx, uint64_t offset) {
                MP4MuxerBase *thiz = (MP4MuxerBase *)ctx;
                return thiz->onSeek(offset);
            },
            [](void* ctx){
                MP4MuxerBase *thiz = (MP4MuxerBase *)ctx;
                return thiz->onTell();
            }
    };
    _mov_writter.reset(mov_writer_create(&s_io,this,flags),[](mov_writer_t *ptr){
        if(ptr){
            mov_writer_destroy(ptr);
        }
    });
}

///////////////////////////////////

void MP4Muxer::onTrackFrame(const Frame::Ptr &frame) {
    if(frame->configFrame()){
        //忽略配置帧
        return;
    }
    auto it = _codec_to_trackid.find(frame->getCodecId());
    if(it == _codec_to_trackid.end()){
        //该Track不存在或初始化失败
        return;
    }

    if(!_started){
        //还没开始
        if(frame->getTrackType() != TrackVideo || !frame->keyFrame()){
            //如果首帧是音频或者是视频但是不是i帧，那么不能开始写文件
            return;
        }
        //开始写文件
        _started = true;
    }

    int with_nalu_size ;
    switch (frame->getCodecId()){
        case CodecH264:
        case CodecH265:
            //我们输入264、265是没有头四个字节表明数据长度的
            with_nalu_size = 0;
            break;
        default:
            //aac或其他类型frame不用添加4个nalu_size的字节
            with_nalu_size = 1;
            break;
    }

    //mp4文件时间戳需要从0开始
    auto &track_info = it->second;
    int64_t dts_out, pts_out;
    track_info.stamp.revise(frame->dts(),frame->pts(),dts_out,pts_out);

    mov_writer_write_l(_mov_writter.get(),
                       track_info.track_id,
                       frame->data() + frame->prefixSize(),
                       frame->size() - frame->prefixSize(),
                       pts_out,
                       dts_out,
                       frame->keyFrame() ? MOV_AV_FLAG_KEYFREAME : 0,
                       with_nalu_size);
}

void MP4Muxer::onTrackReady(const Track::Ptr &track) {
    switch (track->getCodecId()) {
        case CodecAAC: {
            auto aac_track = dynamic_pointer_cast<AACTrack>(track);
            if (!aac_track) {
                WarnL << "不是AAC Track";
                return;
            }
            auto track_id = mov_writer_add_audio(_mov_writter.get(),
                                                 MOV_OBJECT_AAC,
                                                 aac_track->getAudioChannel(),
                                                 aac_track->getAudioSampleBit() * aac_track->getAudioChannel(),
                                                 aac_track->getAudioSampleRate(),
                                                 aac_track->getAacCfg().data(), 2);
            if(track_id < 0){
                WarnL << "添加AAC Track失败:" << track_id;
                return;
            }
            track_info info;
            info.track_id = track_id;
            _codec_to_trackid[track->getCodecId()] = info;
        }
            break;
        case CodecH264: {
            auto h264_track = dynamic_pointer_cast<H264Track>(track);
            if (!h264_track) {
                WarnL << "不是H264 Track";
                return;
            }

            struct mpeg4_avc_t avc;
            string sps_pps = string("\x00\x00\x00\x01", 4) + h264_track->getSps() +
                             string("\x00\x00\x00\x01", 4) + h264_track->getPps();
            h264_annexbtomp4(&avc, sps_pps.data(), sps_pps.size(), NULL, 0, NULL);

            uint8_t extra_data[1024];
            int extra_data_size = mpeg4_avc_decoder_configuration_record_save(&avc, extra_data, sizeof(extra_data));
            if (extra_data_size == -1) {
                WarnL << "生成H264 extra_data 失败";
                return;
            }

            auto track_id = mov_writer_add_video(_mov_writter.get(),
                                                 MOV_OBJECT_H264,
                                                 h264_track->getVideoWidth(),
                                                 h264_track->getVideoHeight(),
                                                 extra_data,
                                                 extra_data_size);

            if(track_id < 0){
                WarnL << "添加H264 Track失败:" << track_id;
                return;
            }
            track_info info;
            info.track_id = track_id;
            _codec_to_trackid[track->getCodecId()] = info;
        }
            break;
        case CodecH265: {
            auto h265_track = dynamic_pointer_cast<H265Track>(track);
            if (!h265_track) {
                WarnL << "不是H265 Track";
                return;
            }

            struct mpeg4_hevc_t hevc;
            string vps_sps_pps = string("\x00\x00\x00\x01", 4) + h265_track->getVps() +
                                 string("\x00\x00\x00\x01", 4) + h265_track->getSps() +
                                 string("\x00\x00\x00\x01", 4) + h265_track->getPps();
            h265_annexbtomp4(&hevc, vps_sps_pps.data(), vps_sps_pps.size(), NULL, 0, NULL);

            uint8_t extra_data[1024];
            int extra_data_size = mpeg4_hevc_decoder_configuration_record_save(&hevc, extra_data, sizeof(extra_data));
            if (extra_data_size == -1) {
                WarnL << "生成H265 extra_data 失败";
                return;
            }

            auto track_id = mov_writer_add_video(_mov_writter.get(),
                                                 MOV_OBJECT_HEVC,
                                                 h265_track->getVideoWidth(),
                                                 h265_track->getVideoHeight(),
                                                 extra_data,
                                                 extra_data_size);
            if(track_id < 0){
                WarnL << "添加H265 Track失败:" << track_id;
                return;
            }
            track_info info;
            info.track_id = track_id;
            _codec_to_trackid[track->getCodecId()] = info;
        }
            break;
        default:
            WarnL << "MP4录制不支持该编码格式:" << track->getCodecId();
            break;
    }
}

MP4MuxerFile::MP4MuxerFile(const char *file) {
    //创建文件
    auto fp = File::createfile_file(file,"wb+");
    if(!fp){
        throw std::runtime_error(string("打开文件失败:") + file);
    }

    GET_CONFIG(uint32_t,mp4BufSize,Record::kFileBufSize);

    //新建文件io缓存
    std::shared_ptr<char> file_buf(new char[mp4BufSize],[](char *ptr){
        if(ptr){
            delete [] ptr;
        }
    });

    if(file_buf){
        //设置文件io缓存
        setvbuf(fp, file_buf.get(), _IOFBF, mp4BufSize);
    }

    //创建智能指针
    _file.reset(fp,[file_buf](FILE *fp) {
        fclose(fp);
    });

    init(MOV_FLAG_FASTSTART);
}

MP4MuxerFile::~MP4MuxerFile() {
    _mov_writter = nullptr;
}

int MP4MuxerFile::onRead(void *data, uint64_t bytes) {
    if (bytes == fread(data, 1, bytes, _file.get())){
        return 0;
    }
    return 0 != ferror(_file.get()) ? ferror(_file.get()) : -1 /*EOF*/;
}

int MP4MuxerFile::onWrite(const void *data, uint64_t bytes) {
    return bytes == fwrite(data, 1, bytes, _file.get()) ? 0 : ferror(_file.get());
}


#if defined(_WIN32) || defined(_WIN64)
    #define fseek64 _fseeki64
    #define ftell64 _ftelli64
#else
    #define fseek64 fseek
    #define ftell64 ftell
#endif

int MP4MuxerFile::onSeek(uint64_t offset) {
    return fseek64(_file.get(), offset, SEEK_SET);
}

uint64_t MP4MuxerFile::onTell() {
    return ftell64(_file.get());
}

}//namespace mediakit

#endif//#ifdef ENABLE_MP4RECORD
