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

using namespace std;
using namespace toolkit;

namespace mediakit {

FlvMuxer::FlvMuxer(){
    _packet_pool.setSize(64);
}

void FlvMuxer::start(const EventPoller::Ptr &poller, const RtmpMediaSource::Ptr &media, uint32_t start_pts) {
    if (!media) {
        throw std::runtime_error("RtmpMediaSource 无效");
    }
    if (!poller->isCurrentThread()) {
        weak_ptr<FlvMuxer> weakSelf = getSharedPtr();
        //延时两秒启动录制，目的是为了等待config帧收集完毕
        poller->doDelayTask(2000, [weakSelf, poller, media, start_pts]() {
            auto strongSelf = weakSelf.lock();
            if (strongSelf) {
                strongSelf->start(poller, media, start_pts);
            }
            return 0;
        });
        return;
    }

    onWriteFlvHeader(media);

    std::weak_ptr<FlvMuxer> weakSelf = getSharedPtr();
    media->pause(false);
    _ring_reader = media->getRing()->attach(poller);
    _ring_reader->setDetachCB([weakSelf]() {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }
        strongSelf->onDetach();
    });

    bool check = start_pts > 0;
    _ring_reader->setReadCB([weakSelf, start_pts, check](const RtmpMediaSource::RingDataType &pkt) mutable {
        auto strongSelf = weakSelf.lock();
        if (!strongSelf) {
            return;
        }

        size_t i = 0;
        auto size = pkt->size();
        pkt->for_each([&](const RtmpPacket::Ptr &rtmp) {
            if (check) {
                if (rtmp->time_stamp < start_pts) {
                    return;
                }
                check = false;
            }
            strongSelf->onWriteRtmp(rtmp, ++i == size);
        });
    });
}

BufferRaw::Ptr FlvMuxer::obtainBuffer() {
    return _packet_pool.obtain2();
}

BufferRaw::Ptr FlvMuxer::obtainBuffer(const void *data, size_t len) {
    auto buffer = obtainBuffer();
    buffer->assign((const char *) data, len);
    return buffer;
}

void FlvMuxer::onWriteFlvHeader(const RtmpMediaSource::Ptr &src) {
    //发送flv文件头
    auto buffer = obtainBuffer();
    buffer->setCapacity(sizeof(FLVHeader));
    buffer->setSize(sizeof(FLVHeader));

    FLVHeader *header = (FLVHeader *) buffer->data();
    memset(header, 0, sizeof(FLVHeader));
    header->flv[0] = 'F';
    header->flv[1] = 'L';
    header->flv[2] = 'V';
    header->version = 1;
    header->length = htonl(9);
    header->have_video = src->haveVideo();
    header->have_audio = src->haveAudio();

    //flv header
    onWrite(buffer, false);

    //PreviousTagSize0 Always 0
    auto size = htonl(0);
    onWrite(obtainBuffer((char *) &size, 4), false);

    auto &metadata = src->getMetaData();
    if (metadata) {
        //在有metadata的情况下才发送metadata
        //其实metadata没什么用，有些推流器不产生metadata
        AMFEncoder invoke;
        invoke << "onMetaData" << metadata;
        onWriteFlvTag(MSG_DATA, std::make_shared<BufferString>(invoke.data()), 0, false);
    }

    //config frame
    src->getConfigFrame([&](const RtmpPacket::Ptr &pkt) {
        onWriteRtmp(pkt, true);
    });
}

void FlvMuxer::onWriteFlvTag(const RtmpPacket::Ptr &pkt, uint32_t time_stamp, bool flush) {
    onWriteFlvTag(pkt->type_id, pkt, time_stamp, flush);
}

void FlvMuxer::onWriteFlvTag(uint8_t type, const Buffer::Ptr &buffer, uint32_t time_stamp, bool flush) {
    RtmpTagHeader header;
    header.type = type;
    set_be24(header.data_size, (uint32_t) buffer->size());
    header.timestamp_ex = (time_stamp >> 24) & 0xff;
    set_be24(header.timestamp, time_stamp & 0xFFFFFF);

    //tag header
    onWrite(obtainBuffer((char *) &header, sizeof(header)), false);

    //tag data
    onWrite(buffer, false);

    //PreviousTagSize
    uint32_t size = htonl((uint32_t) (buffer->size() + sizeof(header)));
    onWrite(obtainBuffer((char *) &size, 4), flush);
}

void FlvMuxer::onWriteRtmp(const RtmpPacket::Ptr &pkt, bool flush) {
    onWriteFlvTag(pkt, pkt->time_stamp, flush);
}

void FlvMuxer::stop() {
    if (_ring_reader) {
        _ring_reader.reset();
        onDetach();
    }
}

///////////////////////////////////////////////////////FlvRecorder/////////////////////////////////////////////////////

void FlvRecorder::startRecord(const EventPoller::Ptr &poller, const string &vhost, const string &app, const string &stream, const string &file_path) {
    startRecord(poller, dynamic_pointer_cast<RtmpMediaSource>(MediaSource::find(RTMP_SCHEMA, vhost, app, stream)), file_path);
}

void FlvRecorder::startRecord(const EventPoller::Ptr &poller, const RtmpMediaSource::Ptr &media,
                              const string &file_path) {
    stop();
    lock_guard<recursive_mutex> lck(_file_mtx);
    //开辟文件写缓存
    std::shared_ptr<char> fileBuf(new char[FILE_BUF_SIZE], [](char *ptr) {
        if (ptr) {
            delete[] ptr;
        }
    });
    //新建文件
    _file.reset(File::create_file(file_path.data(), "wb"), [fileBuf](FILE *fp) {
        if (fp) {
            fflush(fp);
            fclose(fp);
        }
    });
    if (!_file) {
        throw std::runtime_error(StrPrinter << "打开文件失败:" << file_path);
    }

    //设置文件写缓存
    setvbuf(_file.get(), fileBuf.get(), _IOFBF, FILE_BUF_SIZE);
    start(poller, media);
}

void FlvRecorder::onWrite(const Buffer::Ptr &data, bool flush) {
    lock_guard<recursive_mutex> lck(_file_mtx);
    if (_file) {
        fwrite(data->data(), data->size(), 1, _file.get());
    }
}

void FlvRecorder::onDetach() {
    lock_guard<recursive_mutex> lck(_file_mtx);
    _file.reset();
}

std::shared_ptr<FlvMuxer> FlvRecorder::getSharedPtr() {
    return shared_from_this();
}

}//namespace mediakit
