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
#include "fmp4-writer.h"

using namespace toolkit;
namespace mediakit {

/////////////////////////////////////////////////mp4_writer_t/////////////////////////////////////////////////

struct mp4_writer_t {
    int is_fmp4;
    union {
        fmp4_writer_t *fmp4;
        mov_writer_t *mov;
    } u;
};

mp4_writer_t* mp4_writer_create(int is_fmp4, const struct mov_buffer_t *buffer, void* param, int flags){
    mp4_writer_t *mp4 = (mp4_writer_t *) malloc(sizeof(mp4_writer_t));
    mp4->is_fmp4 = is_fmp4;
    if (is_fmp4) {
        mp4->u.fmp4 = fmp4_writer_create(buffer, param, flags);
    } else {
        mp4->u.mov = mov_writer_create(buffer, param, flags);
    }
    return mp4;
}

void mp4_writer_destroy(mp4_writer_t* mp4){
    if (mp4->is_fmp4) {
        fmp4_writer_destroy(mp4->u.fmp4);
    } else {
        mov_writer_destroy(mp4->u.mov);
    }
    free(mp4);
}

int mp4_writer_add_audio(mp4_writer_t* mp4, uint8_t object, int channel_count, int bits_per_sample, int sample_rate, const void* extra_data, size_t extra_data_size){
    if (mp4->is_fmp4) {
        return fmp4_writer_add_audio(mp4->u.fmp4, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);
    } else {
        return mov_writer_add_audio(mp4->u.mov, object, channel_count, bits_per_sample, sample_rate, extra_data, extra_data_size);
    }
}

int mp4_writer_add_video(mp4_writer_t* mp4, uint8_t object, int width, int height, const void* extra_data, size_t extra_data_size){
    if (mp4->is_fmp4) {
        return fmp4_writer_add_video(mp4->u.fmp4, object, width, height, extra_data, extra_data_size);
    } else {
        return mov_writer_add_video(mp4->u.mov, object, width, height, extra_data, extra_data_size);
    }
}

int mp4_writer_add_subtitle(mp4_writer_t* mp4, uint8_t object, const void* extra_data, size_t extra_data_size){
    if (mp4->is_fmp4) {
        return fmp4_writer_add_subtitle(mp4->u.fmp4, object, extra_data, extra_data_size);
    } else {
        return mov_writer_add_subtitle(mp4->u.mov, object, extra_data, extra_data_size);
    }
}

int mp4_writer_write(mp4_writer_t* mp4, int track, const void* data, size_t bytes, int64_t pts, int64_t dts, int flags){
    if (mp4->is_fmp4) {
        return fmp4_writer_write(mp4->u.fmp4, track, data, bytes, pts, dts, flags);
    } else {
        return mov_writer_write(mp4->u.mov, track, data, bytes, pts, dts, flags);
    }
}

int mp4_writer_save_segment(mp4_writer_t* mp4){
    if (mp4->is_fmp4) {
        return fmp4_writer_save_segment(mp4->u.fmp4);
    } else {
        return -1;
    }
}

int mp4_writer_init_segment(mp4_writer_t* mp4){
    if (mp4->is_fmp4) {
        return fmp4_writer_init_segment(mp4->u.fmp4);
    } else {
        return -1;
    }
}

/////////////////////////////////////////////////MP4FileIO/////////////////////////////////////////////////

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
