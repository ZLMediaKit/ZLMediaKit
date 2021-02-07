/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "FlvMuxer.h"
#include "Util/File.h"
#include "Rtmp/utils.h"

#define FILE_BUF_SIZE (64 * 1024)

namespace mediakit {


FlvMuxer::FlvMuxer() {
}
FlvMuxer::~FlvMuxer() {
}

void FlvMuxer::start(const EventPoller::Ptr &poller,const RtmpMediaSource::Ptr &media) {
    if(!media){
        throw std::runtime_error("RtmpMediaSource 无效");
    }
    if(!poller->isCurrentThread()){
        weak_ptr<FlvMuxer> weakSelf = getSharedPtr();
        //延时两秒启动录制，目的是为了等待config帧收集完毕
        poller->doDelayTask(2000,[weakSelf,poller,media](){
            auto strongSelf = weakSelf.lock();
            if(strongSelf){
                strongSelf->start(poller,media);
            }
            return 0;
        });
        return;
    }

    onWriteFlvHeader(media);

    std::weak_ptr<FlvMuxer> weakSelf = getSharedPtr();
    _ring_reader = media->getRing()->attach(poller);
    _ring_reader->setDetachCB([weakSelf](){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        strongSelf->onDetach();
    });

    //音频同步于视频
    _stamp[0].syncTo(_stamp[1]);
    _ring_reader->setReadCB([weakSelf](const RtmpMediaSource::RingDataType &pkt){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }

        size_t i = 0;
        auto size = pkt->size();
        pkt->for_each([&](const RtmpPacket::Ptr &rtmp){
            strongSelf->onWriteRtmp(rtmp, ++i == size);
        });
    });
}

BufferRaw::Ptr FlvMuxer::obtainBuffer(const void *data, size_t len) {
    auto buffer = BufferRaw::create();
    buffer->assign((const char *) data, len);
    return buffer;
}

void FlvMuxer::onWriteFlvHeader(const RtmpMediaSource::Ptr &mediaSrc) {
    //发送flv文件头
    char flv_file_header[] = "FLV\x1\x5\x0\x0\x0\x9"; // have audio and have video
    bool is_have_audio = false,is_have_video = false;

    mediaSrc->getConfigFrame([&](const RtmpPacket::Ptr &pkt){
        if(pkt->type_id == MSG_VIDEO){
            is_have_video = true;
        }
        if(pkt->type_id == MSG_AUDIO){
            is_have_audio = true;
        }
    });

    if (is_have_audio && is_have_video) {
        flv_file_header[4] = 0x05;
    } else if (is_have_audio && !is_have_video) {
        flv_file_header[4] = 0x04;
    } else if (!is_have_audio && is_have_video) {
        flv_file_header[4] = 0x01;
    } else {
        flv_file_header[4] = 0x00;
    }

    //flv header
    onWrite(obtainBuffer(flv_file_header, sizeof(flv_file_header) - 1), false);

    auto size = htonl(0);
    //PreviousTagSize0 Always 0
    onWrite(obtainBuffer((char *)&size,4), false);


    auto &metadata = mediaSrc->getMetaData();
    if(metadata){
        //在有metadata的情况下才发送metadata
        //其实metadata没什么用，有些推流器不产生metadata
        AMFEncoder invoke;
        invoke << "onMetaData" << metadata;
        onWriteFlvTag(MSG_DATA, std::make_shared<BufferString>(invoke.data()), 0, false);
    }

    //config frame
    mediaSrc->getConfigFrame([&](const RtmpPacket::Ptr &pkt){
        onWriteRtmp(pkt, true);
    });
}



#if defined(_WIN32)
#pragma pack(push, 1)
#endif // defined(_WIN32)

class RtmpTagHeader {
public:
    uint8_t type = 0;
    uint8_t data_size[3] = {0};
    uint8_t timestamp[3] = {0};
    uint8_t timestamp_ex = 0;
    uint8_t streamid[3] = {0}; /* Always 0. */
}PACKED;

#if defined(_WIN32)
#pragma pack(pop)
#endif // defined(_WIN32)

void FlvMuxer::onWriteFlvTag(const RtmpPacket::Ptr &pkt, uint32_t time_stamp , bool flush) {
    onWriteFlvTag(pkt->type_id, pkt, time_stamp, flush);
}

void FlvMuxer::onWriteFlvTag(uint8_t type, const Buffer::Ptr &buffer, uint32_t time_stamp, bool flush) {
    RtmpTagHeader header;
    header.type = type;
    set_be24(header.data_size, (uint32_t)buffer->size());
    header.timestamp_ex = (uint8_t) ((time_stamp >> 24) & 0xff);
    set_be24(header.timestamp, time_stamp & 0xFFFFFF);
    //tag header
    onWrite(obtainBuffer((char *)&header, sizeof(header)), false);
    //tag data
    onWrite(buffer, false);
    uint32_t size = htonl((uint32_t)(buffer->size() + sizeof(header)));
    //PreviousTagSize
    onWrite(obtainBuffer((char *)&size,4), flush);
}

void FlvMuxer::onWriteRtmp(const RtmpPacket::Ptr &pkt,bool flush) {
    int64_t dts_out;
    _stamp[pkt->type_id % 2].revise(pkt->time_stamp, 0, dts_out, dts_out);
    onWriteFlvTag(pkt, (uint32_t)dts_out,flush);
}

void FlvMuxer::stop() {
    if(_ring_reader){
        _ring_reader.reset();
        onDetach();
    }
}

///////////////////////////////////////////////////////FlvRecorder/////////////////////////////////////////////////////
void FlvRecorder::startRecord(const EventPoller::Ptr &poller,const string &vhost, const string &app, const string &stream,const string &file_path) {
    startRecord(poller,dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA,vhost,app,stream)),file_path);
}

void FlvRecorder::startRecord(const EventPoller::Ptr &poller,const RtmpMediaSource::Ptr &media, const string &file_path) {
    stop();
    lock_guard<recursive_mutex> lck(_file_mtx);
    //开辟文件写缓存
    std::shared_ptr<char> fileBuf(new char[FILE_BUF_SIZE],[](char *ptr){
        if(ptr){
            delete [] ptr;
        }
    });
    //新建文件
    _file.reset(File::create_file(file_path.data(), "wb"), [fileBuf](FILE *fp){
        if(fp){
            fflush(fp);
            fclose(fp);
        }
    });
    if (!_file){
        throw std::runtime_error( StrPrinter << "打开文件失败:" << file_path);
    }

    //设置文件写缓存
    setvbuf( _file.get(), fileBuf.get(),_IOFBF, FILE_BUF_SIZE);
    start(poller,media);
}

void FlvRecorder::onWrite(const Buffer::Ptr &data, bool flush) {
    lock_guard<recursive_mutex> lck(_file_mtx);
    if(_file){
        fwrite(data->data(),data->size(),1,_file.get());
    }
}

void FlvRecorder::onDetach() {
    lock_guard<recursive_mutex> lck(_file_mtx);
    _file.reset();
}

std::shared_ptr<FlvMuxer> FlvRecorder::getSharedPtr() {
    return  shared_from_this();
}

FlvRecorder::FlvRecorder() {
}

FlvRecorder::~FlvRecorder() {
}


}//namespace mediakit
