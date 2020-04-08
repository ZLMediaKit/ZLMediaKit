/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_FILEREADER_H
#define ZLMEDIAKIT_FILEREADER_H

#include <stdlib.h>
#include <memory>
#include "Network/Buffer.h"
#include "Util/ResourcePool.h"
#include "Util/logger.h"
#include "Thread/WorkThreadPool.h"

using namespace std;
using namespace toolkit;

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b) )
#endif //MIN

namespace mediakit {

/**
 * http content部分基类定义
 */
class HttpBody : public std::enable_shared_from_this<HttpBody>{
public:
    typedef std::shared_ptr<HttpBody> Ptr;
    HttpBody(){
//        _async_read_thread = WorkThreadPool::Instance().getPoller();
    }
    virtual ~HttpBody(){}

    /**
     * 剩余数据大小，如果返回>=INT64_MAX, 那么就不设置content-length
     */
    virtual uint64_t remainSize() { return 0;};

    /**
     * 读取一定字节数，返回大小可能小于size
     * @param size 请求大小
     * @return 字节对象,如果读完了，那么请返回nullptr
     */
    virtual Buffer::Ptr readData(uint32_t size) { return nullptr;};

    /**
     * 异步请求读取一定字节数，返回大小可能小于size
     * @param size 请求大小
     * @param cb 回调函数
     */
    virtual void readDataAsync(uint32_t size,const function<void(const Buffer::Ptr &buf)> &cb){
#if 0
        if(size >= remainSize()){
            //假如剩余数据很小，那么同步获取(为了优化性能)
            cb(readData(size));
            return;
        }
        //如果是大文件，那么后台读取
        weak_ptr<HttpBody> weakSelf = shared_from_this();
        _async_read_thread->async([cb,size,weakSelf](){
            auto strongSelf = weakSelf.lock();
            if(strongSelf){
                cb(strongSelf->readData(size));
            }
        });
#else
        //由于unix和linux是通过mmap的方式读取文件，所以把读文件操作放在后台线程并不能提高性能
        //反而会由于频繁的线程切换导致性能降低以及延时增加，所以我们默认同步获取文件内容
        //(其实并没有读，拷贝文件数据时在内核态完成文件读)
        cb(readData(size));
#endif
    }
private:
//    EventPoller::Ptr _async_read_thread;
};

/**
 * string类型的content
 */
class HttpStringBody : public HttpBody{
public:
    typedef std::shared_ptr<HttpStringBody> Ptr;
    HttpStringBody(const string &str);
    virtual ~HttpStringBody(){}
    uint64_t remainSize() override ;
    Buffer::Ptr readData(uint32_t size) override ;
private:
    mutable string _str;
    uint64_t _offset = 0;
};

/**
 * 文件类型的content
 */
class HttpFileBody : public HttpBody{
public:
    typedef std::shared_ptr<HttpFileBody> Ptr;

    /**
     * 构造函数
     * @param fp 文件句柄，文件的偏移量必须为0
     * @param offset 相对文件头的偏移量
     * @param max_size 最大读取字节数，未判断是否大于文件真实大小
     */
    HttpFileBody(const std::shared_ptr<FILE> &fp,uint64_t offset,uint64_t max_size);
    HttpFileBody(const string &file_path);
    ~HttpFileBody(){};

    uint64_t remainSize() override ;
    Buffer::Ptr readData(uint32_t size) override;
private:
    void init(const std::shared_ptr<FILE> &fp,uint64_t offset,uint64_t max_size);
private:
    std::shared_ptr<FILE> _fp;
    uint64_t _max_size;
    uint64_t _offset = 0;
    std::shared_ptr<char> _map_addr;
    ResourcePool<BufferRaw> _pool;
};

class HttpArgs;

/**
 * http MultiForm 方式提交的http content
 */
class HttpMultiFormBody : public HttpBody {
public:
    typedef std::shared_ptr<HttpMultiFormBody> Ptr;

    /**
     * 构造函数
     * @param args http提交参数列表
     * @param filePath 文件路径
     * @param boundary boundary字符串
     */
    HttpMultiFormBody(const HttpArgs &args,const string &filePath,const string &boundary = "0xKhTmLbOuNdArY");
    virtual ~HttpMultiFormBody(){}
    uint64_t remainSize() override ;
    Buffer::Ptr readData(uint32_t size) override;
public:
    static string multiFormBodyPrefix(const HttpArgs &args,const string &boundary,const string &fileName);
    static string multiFormBodySuffix(const string &boundary);
    static uint64_t fileSize(FILE *fp);
    static string multiFormContentType(const string &boundary);
private:
    string _bodyPrefix;
    string _bodySuffix;
    uint64_t _offset = 0;
    uint64_t _totalSize;
    HttpFileBody::Ptr _fileBody;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_FILEREADER_H
