/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpBody.h"
#include "Util/util.h"
#include "Util/uv_errno.h"
#include "Util/logger.h"
#include "HttpClient.h"
#ifndef _WIN32
#include <sys/mman.h>
#endif

#ifndef _WIN32
#define ENABLE_MMAP
#endif

namespace mediakit {

HttpStringBody::HttpStringBody(const string &str){
    _str = str;
}
uint64_t HttpStringBody::remainSize() {
    return _str.size() - _offset;
}

Buffer::Ptr HttpStringBody::readData(uint32_t size) {
    size = MIN(remainSize(),size);
    if(!size){
        //没有剩余字节了
        return nullptr;
    }
    auto ret = std::make_shared<BufferString>(_str,_offset,size);
    _offset += size;
    return ret;
}

//////////////////////////////////////////////////////////////////
HttpFileBody::HttpFileBody(const string &filePath){
    std::shared_ptr<FILE> fp(fopen(filePath.data(), "rb"), [](FILE *fp) {
        if(fp){
            fclose(fp);
        }
    });
    if(!fp){
        init(fp,0,0);
    }else{
        init(fp,0,HttpMultiFormBody::fileSize(fp.get()));
    }
}

HttpFileBody::HttpFileBody(const std::shared_ptr<FILE> &fp, uint64_t offset, uint64_t max_size) {
    init(fp,offset,max_size);
}

void HttpFileBody::init(const std::shared_ptr<FILE> &fp,uint64_t offset,uint64_t max_size){
    _fp = fp;
    _max_size = max_size;
#ifdef ENABLE_MMAP
    do {
        if(!_fp){
            //文件不存在
            break;
        }
        int fd = fileno(fp.get());
        if (fd < 0) {
            WarnL << "fileno failed:" << get_uv_errmsg(false);
            break;
        }
        auto ptr = (char *) mmap(NULL, max_size, PROT_READ, MAP_SHARED, fd, offset);
        if (ptr == MAP_FAILED) {
            WarnL << "mmap failed:" << get_uv_errmsg(false);
            break;
        }
        _map_addr.reset(ptr,[max_size,fp](char *ptr){
            munmap(ptr,max_size);
        });
    } while (false);
#endif
    if(!_map_addr && offset && fp.get()){
        //未映射,那么fseek设置偏移量
        fseek(fp.get(), offset, SEEK_SET);
    }
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

uint64_t HttpFileBody::remainSize() {
    return _max_size - _offset;
}

Buffer::Ptr HttpFileBody::readData(uint32_t size) {
    size = MIN(remainSize(),size);
    if(!size){
        //没有剩余字节了
        return nullptr;
    }
    if(!_map_addr){
        //fread模式
        int iRead;
        auto ret = _pool.obtain();
        ret->setCapacity(size + 1);
        do{
            iRead = fread(ret->data(), 1, size, _fp.get());
        }while(-1 == iRead && UV_EINTR == get_uv_error(false));

        if(iRead > 0){
            //读到数据了
            ret->setSize(iRead);
            _offset += iRead;
            return std::move(ret);
        }
        //读取文件异常，文件真实长度小于声明长度
        _offset = _max_size;
        WarnL << "read file err:" << get_uv_errmsg();
        return nullptr;
    }

    //mmap模式
    auto ret = std::make_shared<BufferMmap>(_map_addr,_offset,size);
    _offset += size;
    return std::move(ret);
}

//////////////////////////////////////////////////////////////////
HttpMultiFormBody::HttpMultiFormBody(const HttpArgs &args,const string &filePath,const string &boundary){
    std::shared_ptr<FILE> fp(fopen(filePath.data(), "rb"), [](FILE *fp) {
        if(fp){
            fclose(fp);
        }
    });
    if(!fp){
        throw std::invalid_argument(StrPrinter << "open file failed：" << filePath << " " << get_uv_errmsg());
    }
    _fileBody = std::make_shared<HttpFileBody>(fp, 0, fileSize(fp.get()));

    auto fileName = filePath;
    auto pos = filePath.rfind('/');
    if(pos != string::npos){
        fileName = filePath.substr(pos + 1);
    }
    _bodyPrefix = multiFormBodyPrefix(args,boundary,fileName);
    _bodySuffix = multiFormBodySuffix(boundary);
    _totalSize =  _bodyPrefix.size() + _bodySuffix.size() + _fileBody->remainSize();
}

uint64_t HttpMultiFormBody::remainSize() {
    return _totalSize - _offset;
}

Buffer::Ptr HttpMultiFormBody::readData(uint32_t size){
    if(_bodyPrefix.size()){
        auto ret = std::make_shared<BufferString>(_bodyPrefix);
        _offset += _bodyPrefix.size();
        _bodyPrefix.clear();
        return ret;
    }

    if(_fileBody->remainSize()){
        auto ret = _fileBody->readData(size);
        if(!ret){
            //读取文件出现异常，提前中断
            _offset = _totalSize;
        }else{
            _offset += ret->size();
        }
        return ret;
    }

    if(_bodySuffix.size()){
        auto ret = std::make_shared<BufferString>(_bodySuffix);
        _offset = _totalSize;
        _bodySuffix.clear();
        return ret;
    }

    return nullptr;
}

string HttpMultiFormBody::multiFormBodySuffix(const string &boundary){
    string MPboundary = string("--") + boundary;
    string endMPboundary = MPboundary + "--";
    _StrPrinter body;
    body << "\r\n" << endMPboundary;
    return body;
}

uint64_t HttpMultiFormBody::fileSize(FILE *fp) {
    auto current = ftell(fp);
    fseek(fp,0L,SEEK_END); /* 定位到文件末尾 */
    auto end  = ftell(fp); /* 得到文件大小 */
    fseek(fp,current,SEEK_SET);
    return end - current;
}

string HttpMultiFormBody::multiFormContentType(const string &boundary){
    return StrPrinter << "multipart/form-data; boundary=" << boundary;
}

string HttpMultiFormBody::multiFormBodyPrefix(const HttpArgs &args,const string &boundary,const string &fileName){
    string MPboundary = string("--") + boundary;
    _StrPrinter body;
    for(auto &pr : args){
        body << MPboundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"" << pr.first << "\"\r\n\r\n";
        body << pr.second << "\r\n";
    }
    body << MPboundary << "\r\n";
    body << "Content-Disposition: form-data; name=\"" << "file" << "\";filename=\"" << fileName << "\"\r\n";
    body << "Content-Type: application/octet-stream\r\n\r\n" ;
    return body;
}

}//namespace mediakit
