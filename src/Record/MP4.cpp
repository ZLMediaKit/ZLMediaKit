/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
using namespace std;

namespace mediakit {

static struct mov_buffer_t s_io = {
        [](void *ctx, void *data, uint64_t bytes) {
            MP4FileIO *thiz = (MP4FileIO *) ctx;
            return thiz->onRead(data, bytes);
        },
        [](void *ctx, const void *data, uint64_t bytes) {
            MP4FileIO *thiz = (MP4FileIO *) ctx;
            return thiz->onWrite(data, bytes);
        },
        [](void *ctx, int64_t offset) {
            MP4FileIO *thiz = (MP4FileIO *) ctx;
            return thiz->onSeek(offset);
        },
        [](void *ctx) {
            MP4FileIO *thiz = (MP4FileIO *) ctx;
            return (int64_t)thiz->onTell();
        }
};

MP4FileIO::Writer MP4FileIO::createWriter(int flags, bool is_fmp4){
    Writer writer;
    Ptr self = shared_from_this();
    //保存自己的强引用，防止提前释放
    writer.reset(mp4_writer_create(is_fmp4, &s_io,this, flags),[self](mp4_writer_t *ptr){
        if(ptr){
            mp4_writer_destroy(ptr);
        }
    });
    if(!writer){
        throw std::runtime_error("写入mp4文件失败!");
    }
    return writer;
}

MP4FileIO::Reader MP4FileIO::createReader(){
    Reader reader;
    Ptr self = shared_from_this();
    //保存自己的强引用，防止提前释放
    reader.reset(mov_reader_create(&s_io,this),[self](mov_reader_t *ptr){
        if(ptr){
            mov_reader_destroy(ptr);
        }
    });
    if(!reader){
        throw std::runtime_error("读取mp4文件失败!");
    }
    return reader;
}

/////////////////////////////////////////////////////MP4FileDisk/////////////////////////////////////////////////////////

#if defined(_WIN32) || defined(_WIN64)
    #define fseek64 _fseeki64
    #define ftell64 _ftelli64
#else
    #define fseek64 fseek
    #define ftell64 ftell
#endif

void MP4FileDisk::openFile(const char *file, const char *mode) {
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

void MP4FileDisk::closeFile() {
    _file = nullptr;
}

int MP4FileDisk::onRead(void *data, size_t bytes) {
    if (bytes == fread(data, 1, bytes, _file.get())){
        return 0;
    }
    return 0 != ferror(_file.get()) ? ferror(_file.get()) : -1 /*EOF*/;
}

int MP4FileDisk::onWrite(const void *data, size_t bytes) {
    return bytes == fwrite(data, 1, bytes, _file.get()) ? 0 : ferror(_file.get());
}

int MP4FileDisk::onSeek(uint64_t offset) {
    return fseek64(_file.get(), offset, SEEK_SET);
}

uint64_t MP4FileDisk::onTell() {
    return ftell64(_file.get());
}

/////////////////////////////////////////////////////MP4FileMemory/////////////////////////////////////////////////////////

string MP4FileMemory::getAndClearMemory(){
    string ret;
    ret.swap(_memory);
    _offset = 0;
    return ret;
}

size_t MP4FileMemory::fileSize() const{
    return _memory.size();
}

uint64_t MP4FileMemory::onTell(){
    return _offset;
}

int MP4FileMemory::onSeek(uint64_t offset){
    if (offset > _memory.size()) {
        return -1;
    }
    _offset = offset;
    return 0;
}

int MP4FileMemory::onRead(void *data, size_t bytes){
    if (_offset >= _memory.size()) {
        //EOF
        return -1;
    }
    bytes = MIN(bytes, _memory.size() - _offset);
    memcpy(data, _memory.data(), bytes);
    _offset += bytes;
    return 0;
}

int MP4FileMemory::onWrite(const void *data, size_t bytes){
    if (_offset + bytes > _memory.size()) {
        //需要扩容
        _memory.resize(_offset + bytes);
    }
    memcpy((uint8_t *) _memory.data() + _offset, data, bytes);
    _offset += bytes;
    return 0;
}

}//namespace mediakit
#endif //NABLE_MP4RECORD
