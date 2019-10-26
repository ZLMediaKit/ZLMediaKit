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

#include "FileReader.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/logger.h"
#include "Common/config.h"
#ifndef _WIN32
#include <sys/mman.h>
#endif

#ifndef _WIN32
#define ENABLE_MMAP
#endif

FileReader::FileReader(const std::shared_ptr<FILE> &fp, uint64_t offset, uint64_t max_size) {
    _fp = fp;
    _max_size = max_size;
#ifdef ENABLE_MMAP
    do {
        int fd = fileno(fp.get());
        if (fd < 0) {
            WarnL << "fileno失败:" << get_uv_errmsg(false);
            break;
        }
        auto ptr = (char *) mmap(NULL, max_size, PROT_READ, MAP_SHARED, fd, offset);
        if (!ptr) {
            WarnL << "mmap失败:" << get_uv_errmsg(false);
            break;
        }
        _map_addr.reset(ptr,[max_size](char *ptr){
            munmap(ptr,max_size);
        });
    } while (false);
#endif
    if(!_map_addr){
        //未映射
        fseek(fp.get(), offset, SEEK_SET);
    }
}

FileReader::~FileReader() {

}

class BufferMmap : public Buffer{
public:
    typedef std::shared_ptr<BufferMmap> Ptr;
    BufferMmap(const std::shared_ptr<char> &map_addr,uint64_t offset,int size){
        _map_addr = map_addr;
        _data = map_addr.get() + offset;
        _size = size;
    };
    virtual ~BufferMmap(){};
    //返回数据长度
    char *data() const override {
        return _data;
    }
    uint32_t size() const override{
        return _size;
    }
private:
    std::shared_ptr<char> _map_addr;
    char *_data;
    uint32_t _size;
};
Buffer::Ptr FileReader::read(int size) {
    if(_read_offset >= _max_size){
        //文件读完了
        return nullptr;
    }
    int iReq =  MIN(size,_max_size - _read_offset);

    if(!_map_addr){
        //映射失败，fread模式
        int iRead;
        auto ret = std::make_shared<BufferRaw>(iReq + 1);
        do{
            iRead = fread(ret->data(), 1, iReq, _fp.get());
        }while(-1 == iRead && UV_EINTR == get_uv_error(false));

        if(iRead > 0){
            //读到数据了
            ret->setSize(iRead);
            _read_offset += iRead;
            return std::move(ret);
        }
        //没有剩余数据
        _read_offset = _max_size;
        return nullptr;
    }

    auto ret = std::make_shared<BufferMmap>(_map_addr,_read_offset,iReq);
    _read_offset += iReq;
    return std::move(ret);
}
