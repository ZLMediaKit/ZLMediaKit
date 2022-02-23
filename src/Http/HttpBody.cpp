/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <csignal>
#include <tuple>

#ifndef _WIN32
#include <sys/mman.h>
#endif
#if defined(__linux__) || defined(__linux)
#include <sys/sendfile.h>
#endif

#include "Util/File.h"
#include "Util/logger.h"
#include "Util/onceToken.h"
#include "Util/util.h"
#include "Util/uv_errno.h"

#include "HttpBody.h"
#include "HttpClient.h"
#include "Common/macros.h"

#ifndef _WIN32
#define ENABLE_MMAP
#endif

using namespace std;
using namespace toolkit;

namespace mediakit {

HttpStringBody::HttpStringBody(string str) {
    _str = std::move(str);
}

int64_t HttpStringBody::remainSize() {
    return _str.size() - _offset;
}

Buffer::Ptr HttpStringBody::readData(size_t size) {
    size = MIN((size_t)remainSize(), size);
    if (!size) {
        //没有剩余字节了
        return nullptr;
    }
    auto ret = std::make_shared<BufferString>(_str, _offset, size);
    _offset += size;
    return ret;
}

//////////////////////////////////////////////////////////////////

#ifdef ENABLE_MMAP

static mutex s_mtx;
static unordered_map<string /*file_path*/, std::tuple<char */*ptr*/, int64_t /*size*/, weak_ptr<char> /*mmap*/ > > s_shared_mmap;

//删除mmap记录
static void delSharedMmap(const string &file_path, char *ptr) {
    lock_guard<mutex> lck(s_mtx);
    auto it = s_shared_mmap.find(file_path);
    if (it != s_shared_mmap.end() && std::get<0>(it->second) == ptr) {
        s_shared_mmap.erase(it);
    }
}

static std::shared_ptr<char> getSharedMmap(const string &file_path, int64_t &file_size) {
    {
        lock_guard<mutex> lck(s_mtx);
        auto it = s_shared_mmap.find(file_path);
        if (it != s_shared_mmap.end()) {
            auto ret = std::get<2>(it->second).lock();
            if (ret) {
                //命中mmap缓存
                file_size = std::get<1>(it->second);
                return ret;
            }
        }
    }

    //打开文件
    std::shared_ptr<FILE> fp(fopen(file_path.data(), "rb"), [](FILE *fp) {
        if (fp) {
            fclose(fp);
        }
    });
    if (!fp) {
        //文件不存在
        file_size = -1;
        return nullptr;
    }
    //获取文件大小
    file_size = File::fileSize(fp.get());

    int fd = fileno(fp.get());
    if (fd < 0) {
        WarnL << "fileno failed:" << get_uv_errmsg(false);
        return nullptr;
    }
    auto ptr = (char *)mmap(NULL, file_size, PROT_READ, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        WarnL << "mmap " << file_path << " failed:" << get_uv_errmsg(false);
        return nullptr;
    }
    std::shared_ptr<char> ret(ptr, [file_size, fp, file_path](char *ptr) {
        munmap(ptr, file_size);
        delSharedMmap(file_path, ptr);
    });

#if 0
    if (file_size < 10 * 1024 * 1024 && file_path.rfind(".ts") != string::npos) {
        //如果是小ts文件，那么尝试先加载到内存
        auto buf = BufferRaw::create();
        buf->assign(ret.get(), file_size);
        ret.reset(buf->data(), [buf, file_path](char *ptr) {
            delSharedMmap(file_path, ptr);
        });
    }
#endif
    {
        lock_guard<mutex> lck(s_mtx);
        s_shared_mmap[file_path] = std::make_tuple(ret.get(), file_size, ret);
    }
    return ret;
}
#endif

HttpFileBody::HttpFileBody(const string &file_path, bool use_mmap) {
#ifdef ENABLE_MMAP
    if (use_mmap ) {
        _map_addr = getSharedMmap(file_path, _read_to);
    }
#endif
    if (!_map_addr && _read_to != -1) {
        //mmap失败(且不是由于文件不存在导致的)或未执行mmap时，才进入fread逻辑分支
        _fp.reset(fopen(file_path.data(), "rb"), [](FILE *fp) {
            if (fp) {
                fclose(fp);
            }
        });
        if (!_fp) {
            //文件不存在
            _read_to = -1;
            return;
        }
        if (!_read_to) {
            //_read_to等于0时，说明还未尝试获取文件大小
            //加上该判断逻辑，在mmap失败时，可以省去一次该操作
            _read_to = File::fileSize(_fp.get());
        }
    }
}

void HttpFileBody::setRange(uint64_t offset, uint64_t max_size) {
    CHECK((int64_t)offset <= _read_to && (int64_t)(max_size + offset) <= _read_to);
    _read_to = max_size + offset;
    _file_offset = offset;
    if (_fp && !_map_addr) {
        fseek64(_fp.get(), _file_offset, SEEK_SET);
    }
}

int HttpFileBody::sendFile(int fd) {
#if defined(__linux__) || defined(__linux)
    if (!_fp) {
        return -1;
    }
    static onceToken s_token([]() { signal(SIGPIPE, SIG_IGN); });
    off_t off = _file_offset;
    return sendfile(fd, fileno(_fp.get()), &off, _read_to - _file_offset);
#else
    return -1;
#endif
}

class BufferMmap : public Buffer {
public:
    typedef std::shared_ptr<BufferMmap> Ptr;
    BufferMmap(const std::shared_ptr<char> &map_addr, size_t offset, size_t size) {
        _map_addr = map_addr;
        _data = map_addr.get() + offset;
        _size = size;
    }
    ~BufferMmap() override {};
    //返回数据长度
    char *data() const override { return _data; }
    size_t size() const override { return _size; }

private:
    char *_data;
    size_t _size;
    std::shared_ptr<char> _map_addr;
};

int64_t HttpFileBody::remainSize() {
    return _read_to - _file_offset;
}

Buffer::Ptr HttpFileBody::readData(size_t size) {
    size = (size_t)(MIN(remainSize(), (int64_t)size));
    if (!size) {
        //没有剩余字节了
        return nullptr;
    }
    if (!_map_addr) {
        // fread模式
        ssize_t iRead;
        auto ret = _pool.obtain2();
        ret->setCapacity(size + 1);
        do {
            iRead = fread(ret->data(), 1, size, _fp.get());
        } while (-1 == iRead && UV_EINTR == get_uv_error(false));

        if (iRead > 0) {
            //读到数据了
            ret->setSize(iRead);
            _file_offset += iRead;
            return std::move(ret);
        }
        //读取文件异常，文件真实长度小于声明长度
        _file_offset = _read_to;
        WarnL << "read file err:" << get_uv_errmsg();
        return nullptr;
    }

    // mmap模式
    auto ret = std::make_shared<BufferMmap>(_map_addr, _file_offset, size);
    _file_offset += size;
    return ret;
}

//////////////////////////////////////////////////////////////////

HttpMultiFormBody::HttpMultiFormBody(const HttpArgs &args, const string &filePath, const string &boundary) {
    _fileBody = std::make_shared<HttpFileBody>(filePath);
    if (_fileBody->remainSize() < 0) {
        throw std::invalid_argument(StrPrinter << "open file failed：" << filePath << " " << get_uv_errmsg());
    }

    auto fileName = filePath;
    auto pos = filePath.rfind('/');
    if (pos != string::npos) {
        fileName = filePath.substr(pos + 1);
    }
    _bodyPrefix = multiFormBodyPrefix(args, boundary, fileName);
    _bodySuffix = multiFormBodySuffix(boundary);
    _totalSize = _bodyPrefix.size() + _bodySuffix.size() + _fileBody->remainSize();
}

int64_t HttpMultiFormBody::remainSize() {
    return _totalSize - _offset;
}

Buffer::Ptr HttpMultiFormBody::readData(size_t size) {
    if (_bodyPrefix.size()) {
        auto ret = std::make_shared<BufferString>(_bodyPrefix);
        _offset += _bodyPrefix.size();
        _bodyPrefix.clear();
        return ret;
    }

    if (_fileBody->remainSize()) {
        auto ret = _fileBody->readData(size);
        if (!ret) {
            //读取文件出现异常，提前中断
            _offset = _totalSize;
        } else {
            _offset += ret->size();
        }
        return ret;
    }

    if (_bodySuffix.size()) {
        auto ret = std::make_shared<BufferString>(_bodySuffix);
        _offset = _totalSize;
        _bodySuffix.clear();
        return ret;
    }

    return nullptr;
}

string HttpMultiFormBody::multiFormBodySuffix(const string &boundary) {
    string MPboundary = string("--") + boundary;
    string endMPboundary = MPboundary + "--";
    _StrPrinter body;
    body << "\r\n" << endMPboundary;
    return std::move(body);
}

string HttpMultiFormBody::multiFormContentType(const string &boundary) {
    return StrPrinter << "multipart/form-data; boundary=" << boundary;
}

string HttpMultiFormBody::multiFormBodyPrefix(const HttpArgs &args, const string &boundary, const string &fileName) {
    string MPboundary = string("--") + boundary;
    _StrPrinter body;
    for (auto &pr : args) {
        body << MPboundary << "\r\n";
        body << "Content-Disposition: form-data; name=\"" << pr.first << "\"\r\n\r\n";
        body << pr.second << "\r\n";
    }
    body << MPboundary << "\r\n";
    body << "Content-Disposition: form-data; name=\""
         << "file"
         << "\";filename=\"" << fileName << "\"\r\n";
    body << "Content-Type: application/octet-stream\r\n\r\n";
    return std::move(body);
}

HttpBufferBody::HttpBufferBody(Buffer::Ptr buffer) {
    _buffer = std::move(buffer);
}

int64_t HttpBufferBody::remainSize() {
    return _buffer ? _buffer->size() : 0;
}

Buffer::Ptr HttpBufferBody::readData(size_t size) {
    return Buffer::Ptr(std::move(_buffer));
}

} // namespace mediakit
