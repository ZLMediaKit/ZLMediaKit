/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
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
 * Base class definition for http content part
 
 * [AUTO-TRANSLATED:1eee419a]
 */
class HttpBody : public std::enable_shared_from_this<HttpBody>{
public:
    using Ptr = std::shared_ptr<HttpBody>;
    virtual ~HttpBody() = default;

    /**
     * 剩余数据大小，如果返回-1, 那么就不设置content-length
     * Remaining data size, if -1 is returned, then content-length is not set
     
     * [AUTO-TRANSLATED:75375ce7]
     */
    virtual int64_t remainSize() { return 0;};

    /**
     * 读取一定字节数，返回大小可能小于size
     * @param size 请求大小
     * @return 字节对象,如果读完了，那么请返回nullptr
     * Read a certain number of bytes, the returned size may be less than size
     * @param size Request size
     * @return Byte object, if it is read, please return nullptr
     
     * [AUTO-TRANSLATED:6fd85f91]
     */
    virtual toolkit::Buffer::Ptr readData(size_t size) { return nullptr;};

    /**
     * 异步请求读取一定字节数，返回大小可能小于size
     * @param size 请求大小
     * @param cb 回调函数
     * Asynchronously request to read a certain number of bytes, the returned size may be less than size
     * @param size Request size
     * @param cb Callback function
     
     * [AUTO-TRANSLATED:a5304046]
     */
    virtual void readDataAsync(size_t size,const std::function<void(const toolkit::Buffer::Ptr &buf)> &cb){
        // 由于unix和linux是通过mmap的方式读取文件，所以把读文件操作放在后台线程并不能提高性能  [AUTO-TRANSLATED:59ef443d]
        // Since unix and linux read files through mmap, putting file reading operations in the background thread does not improve performance
        // 反而会由于频繁的线程切换导致性能降低以及延时增加，所以我们默认同步获取文件内容  [AUTO-TRANSLATED:93d2a0b5]
        // On the contrary, frequent thread switching will lead to performance degradation and increased latency, so we get the file content synchronously by default
        // (其实并没有读，拷贝文件数据时在内核态完成文件读)  [AUTO-TRANSLATED:6eb98a5d]
        // (Actually, there is no reading, the file data is copied in the kernel state when copying)
        cb(readData(size));
    }

    /**
     * 使用sendfile优化文件发送
     * @param fd socket fd
     * @return 0成功，其他为错误代码
     * Use sendfile to optimize file sending
     * @param fd socket fd
     * @return 0 success, other error codes
     
     * [AUTO-TRANSLATED:eacc5f98]
     */
    virtual int sendFile(int fd) {
        return -1;
    }
};

/**
 * std::string类型的content
 * std::string type content
 
 * [AUTO-TRANSLATED:59fc3e5b]
 */
class HttpStringBody : public HttpBody{
public:
    using Ptr = std::shared_ptr<HttpStringBody>;
    HttpStringBody(std::string str);

    int64_t remainSize() override;
    toolkit::Buffer::Ptr readData(size_t size) override ;

private:
    size_t _offset = 0;
    mutable std::string _str;
};

/**
 * Buffer类型的content
 * Buffer type content
 
 * [AUTO-TRANSLATED:350b9513]
 */
class HttpBufferBody : public HttpBody{
public:
    using Ptr = std::shared_ptr<HttpBufferBody>;
    HttpBufferBody(toolkit::Buffer::Ptr buffer);

    int64_t remainSize() override;
    toolkit::Buffer::Ptr readData(size_t size) override;

private:
    toolkit::Buffer::Ptr _buffer;
};

/**
 * 文件类型的content
 * File type content
 
 * [AUTO-TRANSLATED:baf9c0f3]
 */
class HttpFileBody : public HttpBody {
public:
    using Ptr = std::shared_ptr<HttpFileBody>;

    /**
     * 构造函数
     * @param file_path 文件路径
     * @param use_mmap 是否使用mmap方式访问文件
     * Constructor
     * @param file_path File path
     * @param use_mmap Whether to use mmap to access the file
     
     * [AUTO-TRANSLATED:40c85c53]
     */
    HttpFileBody(const std::string &file_path, bool use_mmap = true);

    /**
     * 设置读取范围
     * @param offset 相对文件头的偏移量
     * @param max_size 最大读取字节数
     * Set the reading range
     * @param offset Offset relative to the file header
     * @param max_size Maximum number of bytes to read
     
     * [AUTO-TRANSLATED:30532a4e]
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
 * http MultiForm way to submit http content
 
 * [AUTO-TRANSLATED:211a2d8e]
 */
class HttpMultiFormBody : public HttpBody {
public:
    using Ptr = std::shared_ptr<HttpMultiFormBody>;

    /**
     * 构造函数
     * @param args http提交参数列表
     * @param filePath 文件路径
     * @param boundary boundary字符串
     * Constructor
     * @param args http submission parameter list
     * @param filePath File path
     * @param boundary Boundary string
     
     
     * [AUTO-TRANSLATED:d093cfa7]
     */
    HttpMultiFormBody(const HttpArgs &args,const std::string &filePath,const std::string &boundary = "0xKhTmLbOuNdArY");
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
