/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
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
    HttpBody(){}

    virtual ~HttpBody(){}

    /**
     * 剩余数据大小，如果返回-1, 那么就不设置content-length
     */
    virtual int64_t remainSize() { return 0;};

    /**
     * 读取一定字节数，返回大小可能小于size
     * @param size 请求大小
     * @return 字节对象,如果读完了，那么请返回nullptr
     */
    virtual toolkit::Buffer::Ptr readData(size_t size) { return nullptr;};

    /**
     * 异步请求读取一定字节数，返回大小可能小于size
     * @param size 请求大小
     * @param cb 回调函数
     */
    virtual void readDataAsync(size_t size,const std::function<void(const toolkit::Buffer::Ptr &buf)> &cb){
        //由于unix和linux是通过mmap的方式读取文件，所以把读文件操作放在后台线程并不能提高性能
        //反而会由于频繁的线程切换导致性能降低以及延时增加，所以我们默认同步获取文件内容
        //(其实并没有读，拷贝文件数据时在内核态完成文件读)
        cb(readData(size));
    }

    /**
     * 使用sendfile优化文件发送
     * @param fd socket fd
     * @return 0成功，其他为错误代码
     */
    virtual int sendFile(int fd) {
        return -1;
    }
};

/**
 * std::string类型的content
 */
class HttpStringBody : public HttpBody{
public:
    typedef std::shared_ptr<HttpStringBody> Ptr;
    HttpStringBody(std::string str);
    ~HttpStringBody() override = default;

    int64_t remainSize() override;
    toolkit::Buffer::Ptr readData(size_t size) override ;

private:
    size_t _offset = 0;
    mutable std::string _str;
};

/**
 * Buffer类型的content
 */
class HttpBufferBody : public HttpBody{
public:
    typedef std::shared_ptr<HttpBufferBody> Ptr;
    HttpBufferBody(toolkit::Buffer::Ptr buffer);
    ~HttpBufferBody() override = default;

    int64_t remainSize() override;
    toolkit::Buffer::Ptr readData(size_t size) override;

private:
    toolkit::Buffer::Ptr _buffer;
};

/**
 * 文件类型的content
 */
class HttpFileBody : public HttpBody {
public:
    typedef std::shared_ptr<HttpFileBody> Ptr;

    /**
     * 构造函数
     * @param file_path 文件路径
     * @param use_mmap 是否使用mmap方式访问文件
     */
    HttpFileBody(const std::string &file_path, bool use_mmap = true);
    ~HttpFileBody() override = default;

    /**
     * 设置读取范围
     * @param offset 相对文件头的偏移量
     * @param max_size 最大读取字节数
     */
    void setRange(uint64_t offset, uint64_t max_size);

    int64_t remainSize() override;
    toolkit::Buffer::Ptr readData(size_t size) override;
    int sendFile(int fd) override;

private:
    int64_t _read_to = 0;
    uint64_t _file_offset = 0;
    std::shared_ptr<FILE> _fp;
    std::shared_ptr<char> _map_addr;
    toolkit::ResourcePool<toolkit::BufferRaw> _pool;
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
    HttpMultiFormBody(const HttpArgs &args,const std::string &filePath,const std::string &boundary = "0xKhTmLbOuNdArY");
    virtual ~HttpMultiFormBody(){}
    int64_t remainSize() override ;
    toolkit::Buffer::Ptr readData(size_t size) override;

public:
    static std::string multiFormBodyPrefix(const HttpArgs &args,const std::string &boundary,const std::string &fileName);
    static std::string multiFormBodySuffix(const std::string &boundary);
    static std::string multiFormContentType(const std::string &boundary);

private:
    uint64_t _offset = 0;
    int64_t _totalSize;
    std::string _bodyPrefix;
    std::string _bodySuffix;
    HttpFileBody::Ptr _fileBody;
};

}//namespace mediakit

#endif //ZLMEDIAKIT_FILEREADER_H
