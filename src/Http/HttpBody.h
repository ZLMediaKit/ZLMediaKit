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

#ifndef ZLMEDIAKIT_FILEREADER_H
#define ZLMEDIAKIT_FILEREADER_H

#include <stdlib.h>
#include <memory>
#include "Network/Buffer.h"
#include "Util/ResourcePool.h"
#include "Util/logger.h"

using namespace std;
using namespace toolkit;

#ifndef MIN
#define MIN(a,b) ((a) < (b) ? (a) : (b) )
#endif //MIN

namespace mediakit {

/**
 * http content部分基类定义
 */
class HttpBody{
public:
    typedef std::shared_ptr<HttpBody> Ptr;
    HttpBody(){}
    virtual ~HttpBody(){}

    /**
     * 剩余数据大小
     */
    virtual uint64_t remainSize() { return 0;};

    /**
     * 读取一定字节数，返回大小可能小于size
     * @param size 请求大小
     * @return 字节对象
     */
    virtual Buffer::Ptr readData(uint32_t size) { return nullptr;};
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
