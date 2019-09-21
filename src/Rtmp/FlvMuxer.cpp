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
    _ring_reader->setReadCB([weakSelf](const RtmpPacket::Ptr &pkt){
        auto strongSelf = weakSelf.lock();
        if(!strongSelf){
            return;
        }
        strongSelf->onWriteRtmp(pkt);
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
    onWrite(std::make_shared<BufferRaw>(flv_file_header, sizeof(flv_file_header) - 1));

    auto size = htonl(0);
    //PreviousTagSize0 Always 0
    onWrite(std::make_shared<BufferRaw>((char *)&size,4));


    auto &metadata = mediaSrc->getMetaData();
    if(metadata){
        //在有metadata的情况下才发送metadata
        //其实metadata没什么用，有些推流器不产生metadata
        AMFEncoder invoke;
        invoke << "onMetaData" << metadata;
        onWriteFlvTag(MSG_DATA, std::make_shared<BufferString>(invoke.data()), 0);
    }

    //config frame
    mediaSrc->getConfigFrame([&](const RtmpPacket::Ptr &pkt){
        onWriteRtmp(pkt);
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

void FlvMuxer::onWriteFlvTag(const RtmpPacket::Ptr &pkt, uint32_t ui32TimeStamp) {
    onWriteFlvTag(pkt->typeId,pkt,ui32TimeStamp);
}

void FlvMuxer::onWriteFlvTag(uint8_t ui8Type, const Buffer::Ptr &buffer, uint32_t ui32TimeStamp) {
    RtmpTagHeader header;
    header.type = ui8Type;
    set_be24(header.data_size, buffer->size());
    header.timestamp_ex = (uint8_t) ((ui32TimeStamp >> 24) & 0xff);
    set_be24(header.timestamp,ui32TimeStamp & 0xFFFFFF);
    //tag header
    onWrite(std::make_shared<BufferRaw>((char *)&header, sizeof(header)));
    //tag data
    onWrite(buffer);
    auto size = htonl((buffer->size() + sizeof(header)));
    //PreviousTagSize
    onWrite(std::make_shared<BufferRaw>((char *)&size,4));
}

void FlvMuxer::onWriteRtmp(const RtmpPacket::Ptr &pkt) {
    int64_t dts_out;
    _stamp[pkt->typeId % 2].revise(pkt->timeStamp, 0, dts_out, dts_out);
    onWriteFlvTag(pkt, dts_out);
}

void FlvMuxer::stop() {
    if(_ring_reader){
        _ring_reader.reset();
        onDetach();
    }
}

///////////////////////////////////////////////////////FlvRecorder/////////////////////////////////////////////////////
void FlvRecorder::startRecord(const EventPoller::Ptr &poller,const string &vhost, const string &app, const string &stream,const string &file_path) {
    startRecord(poller,dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA,vhost,app,stream,false)),file_path);
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
    _file.reset(File::createfile_file(file_path.data(),"wb"),[fileBuf](FILE *fp){
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

void FlvRecorder::onWrite(const Buffer::Ptr &data) {
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
