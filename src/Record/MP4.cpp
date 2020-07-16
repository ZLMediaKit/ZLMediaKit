/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifdef ENABLE_MP4
#include "MP4.h"
#include "Util/File.h"
#include "Util/logger.h"
#include "Common/config.h"
using namespace toolkit;
namespace mediakit {

static struct mov_buffer_t s_io = {
        [](void* ctx, void* data, uint64_t bytes) {
            MP4File *thiz = (MP4File *)ctx;
            return thiz->onRead(data,bytes);
        },
        [](void* ctx, const void* data, uint64_t bytes){
            MP4File *thiz = (MP4File *)ctx;
            return thiz->onWrite(data,bytes);
        },
        [](void* ctx, uint64_t offset) {
            MP4File *thiz = (MP4File *)ctx;
            return thiz->onSeek(offset);
        },
        [](void* ctx){
            MP4File *thiz = (MP4File *)ctx;
            return thiz->onTell();
        }
};

MP4File::Writer MP4File::createWriter(){
    GET_CONFIG(bool, mp4FastStart, Record::kFastStart);
    Writer writer;
    writer.reset(mov_writer_create(&s_io,this,mp4FastStart ? MOV_FLAG_FASTSTART : 0),[](mov_writer_t *ptr){
        if(ptr){
            mov_writer_destroy(ptr);
        }
    });
    if(!writer){
        throw std::runtime_error("写入mp4文件失败!");
    }
    return writer;
}

MP4File::Reader MP4File::createReader(){
    Reader reader;
    reader.reset(mov_reader_create(&s_io,this),[](mov_reader_t *ptr){
        if(ptr){
            mov_reader_destroy(ptr);
        }
    });
    if(!reader){
        throw std::runtime_error("读取mp4文件失败!");
    }
    return reader;
}

#if defined(_WIN32) || defined(_WIN64)
    #define fseek64 _fseeki64
#define ftell64 _ftelli64
#else
#define fseek64 fseek
#define ftell64 ftell
#endif

void MP4File::openFile(const char *file,const char *mode) {
    //创建文件
    auto fp = File::create_file(file, mode);
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
        fflush(fp);
        fclose(fp);
    });
}

void MP4File::closeFile() {
    _file = nullptr;
}

int MP4File::onRead(void *data, uint64_t bytes) {
    if (bytes == fread(data, 1, bytes, _file.get())){
        return 0;
    }
    return 0 != ferror(_file.get()) ? ferror(_file.get()) : -1 /*EOF*/;
}

int MP4File::onWrite(const void *data, uint64_t bytes) {
    return bytes == fwrite(data, 1, bytes, _file.get()) ? 0 : ferror(_file.get());
}

int MP4File::onSeek(uint64_t offset) {
    return fseek64(_file.get(), offset, SEEK_SET);
}

uint64_t MP4File::onTell() {
    return ftell64(_file.get());
}

}//namespace mediakit
#endif //NABLE_MP4RECORD
