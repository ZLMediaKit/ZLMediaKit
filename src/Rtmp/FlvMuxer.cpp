/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
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

        int i = 0;
        int size = pkt->size();
        pkt->for_each([&](const RtmpPacket::Ptr &rtmp){
            strongSelf->onWriteRtmp(rtmp, ++i == size);
        });
    });
}

void FlvMuxer::onWriteFlvHeader(const RtmpMediaSource::Ptr &mediaSrc) {
    //发送flv文件头
    char flv_file_header[] = "FLV\x1\x5\x0\x0\x0\x9"; // have audio and have video
    bool is_have_audio = false,is_have_video = false;

    mediaSrc->getConfigFrame([&](const RtmpPacket::Ptr &pkt){
        if(pkt->typeId == MSG_VIDEO){
            is_have_video = true;
        }
        if(pkt->typeId == MSG_AUDIO){
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
    onWrite(std::make_shared<BufferRaw>(flv_file_header, sizeof(flv_file_header) - 1), false);

    auto size = htonl(0);
    //PreviousTagSize0 Always 0
    onWrite(std::make_shared<BufferRaw>((char *)&size,4), false);


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

void FlvMuxer::onWriteFlvTag(const RtmpPacket::Ptr &pkt, uint32_t ui32TimeStamp , bool flush) {
    onWriteFlvTag(pkt->typeId,pkt,ui32TimeStamp, flush);
}

void FlvMuxer::onWriteFlvTag(uint8_t ui8Type, const Buffer::Ptr &buffer, uint32_t ui32TimeStamp, bool flush) {
    RtmpTagHeader header;
    header.type = ui8Type;
    set_be24(header.data_size, buffer->size());
    header.timestamp_ex = (uint8_t) ((ui32TimeStamp >> 24) & 0xff);
    set_be24(header.timestamp,ui32TimeStamp & 0xFFFFFF);
    //tag header
    onWrite(std::make_shared<BufferRaw>((char *)&header, sizeof(header)), false);
    //tag data
    onWrite(buffer, false);
    auto size = htonl((buffer->size() + sizeof(header)));
    //PreviousTagSize
    onWrite(std::make_shared<BufferRaw>((char *)&size,4), flush);
}

void FlvMuxer::onWriteRtmp(const RtmpPacket::Ptr &pkt,bool flush) {
    int64_t dts_out;
    _stamp[pkt->typeId % 2].revise(pkt->timeStamp, 0, dts_out, dts_out);
    onWriteFlvTag(pkt, dts_out,flush);
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
