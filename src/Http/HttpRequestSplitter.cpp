/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "HttpRequestSplitter.h"
#include "Util/logger.h"
#include "Util/util.h"
using namespace toolkit;
using namespace std;

// 协议解析最大缓存4兆数据  [AUTO-TRANSLATED:75159526]
// Protocol parsing maximum cache 4MB data
static constexpr size_t kMaxCacheSize = 4 * 1024 * 1024;

namespace mediakit {

void HttpRequestSplitter::input(const char *data,size_t len) {
    {
        auto size = remainDataSize();
        if (size > _max_cache_size) {
            // 缓存太多数据无法处理则上抛异常  [AUTO-TRANSLATED:30e48e9e]
            // If too much data is cached and cannot be processed, throw an exception
            reset();
            throw std::out_of_range("remain data size is too huge, now cleared:" + to_string(size));
        }
    }
    const char *ptr = data;
    if(!_remain_data.empty()){
        _remain_data.append(data,len);
        data = ptr = _remain_data.data();
        len = _remain_data.size();
    }

    splitPacket:

    /*确保ptr最后一个字节是0，防止strstr越界
     *由于ZLToolKit确保内存最后一个字节是保留未使用字节并置0，
     *所以此处可以不用再次置0
     *但是上层数据可能来自其他渠道，保险起见还是置0
     *Ensure the last byte of ptr is 0 to prevent strstr from going out of bounds
     * Since ZLToolKit ensures that the last byte of memory is a reserved unused byte and set to 0,
     * so there is no need to set it to 0 again here
     * But the upper layer data may come from other channels, so it is better to set it to 0 for safety
     
     * [AUTO-TRANSLATED:28ff47a5]
     */

    char &tail_ref = ((char *) ptr)[len];
    char tail_tmp = tail_ref;
    tail_ref = 0;

    // 数据按照请求头处理  [AUTO-TRANSLATED:e7a0dbb4]
    // Data is processed according to the request header
    const char *index = nullptr;
    _remain_data_size = len;
    while (_content_len == 0 && _remain_data_size > 0 && (index = onSearchPacketTail(ptr,_remain_data_size)) != nullptr) {
        if (index == ptr) {
            break;
        }
        if (index < ptr || index > ptr + _remain_data_size) {
            throw std::out_of_range("上层分包逻辑异常");
        }
        // _content_len == 0，这是请求头  [AUTO-TRANSLATED:32af637b]
        // _content_len == 0, this is the request header
        const char *header_ptr = ptr;
        ssize_t header_size = index - ptr;
        ptr = index;
        _remain_data_size = len - (ptr - data);
        _content_len = onRecvHeader(header_ptr, header_size);
    }

    /*
     * 恢复末尾字节
     * 移动到这来，目的是防止HttpRequestSplitter::reset()导致内存失效
     /*
     * Restore the last byte
     * Move it here to prevent HttpRequestSplitter::reset() from causing memory failure
     
     * [AUTO-TRANSLATED:9c3e0597]
     */
    tail_ref = tail_tmp;

    if(_remain_data_size <= 0){
        // 没有剩余数据，清空缓存  [AUTO-TRANSLATED:16613daa]
        // No remaining data, clear the cache
        _remain_data.clear();
        return;
    }

    if(_content_len == 0){
        // 尚未找到http头，缓存定位到剩余数据部分  [AUTO-TRANSLATED:7a9d6205]
        // HTTP header not found yet, cache is located at the remaining data part
        _remain_data.assign(ptr,_remain_data_size);
        return;
    }

    // 已经找到http头了  [AUTO-TRANSLATED:df166db7]
    // HTTP header has been found
    if(_content_len > 0){
        // 数据按照固定长度content处理  [AUTO-TRANSLATED:7272b7e7]
        // Data is processed according to fixed length content
        if(_remain_data_size < (size_t)_content_len){
            // 数据不够，缓存定位到剩余数据部分  [AUTO-TRANSLATED:61c32f5c]
            // Insufficient data, cache is located at the remaining data part
            _remain_data.assign(ptr, _remain_data_size);
            return;
        }
        // 收到content数据，并且接收content完毕  [AUTO-TRANSLATED:0342dc0e]
        // Content data received and content reception completed
        onRecvContent(ptr,_content_len);

        _remain_data_size -= _content_len;
        ptr += _content_len;
        // content处理完毕,后面数据当做请求头处理  [AUTO-TRANSLATED:d268dfe4]
        // Content processing completed, subsequent data is treated as request header
        _content_len = 0;

        if(_remain_data_size > 0){
            // 还有数据没有处理完毕  [AUTO-TRANSLATED:1cac6727]
            // There is still data that has not been processed
            _remain_data.assign(ptr,_remain_data_size);
            data = ptr = (char *)_remain_data.data();
            len = _remain_data.size();
            goto splitPacket;
        }
        _remain_data.clear();
        return;
    }


    // _content_len < 0;数据按照不固定长度content处理  [AUTO-TRANSLATED:68d6a4d0]
    // _content_len < 0; Data is processed according to variable length content
    onRecvContent(ptr,_remain_data_size);//消费掉所有剩余数据
    _remain_data.clear();
}

void HttpRequestSplitter::setContentLen(ssize_t content_len) {
    _content_len = content_len;
}

void HttpRequestSplitter::reset() {
    _content_len = 0;
    _remain_data_size = 0;
    _remain_data.clear();
}

const char *HttpRequestSplitter::onSearchPacketTail(const char *data,size_t len) {
    auto pos = strstr(data,"\r\n\r\n");
    if(pos == nullptr){
        return nullptr;
    }
    return  pos + 4;
}

size_t HttpRequestSplitter::remainDataSize() {
    return _remain_data_size;
}

const char *HttpRequestSplitter::remainData() const {
    return _remain_data.data();
}

void HttpRequestSplitter::setMaxCacheSize(size_t max_cache_size) {
    if (!max_cache_size) {
        max_cache_size = kMaxCacheSize;
    }
    _max_cache_size = max_cache_size;
}

HttpRequestSplitter::HttpRequestSplitter() {
    setMaxCacheSize(0);
}

} /* namespace mediakit */

