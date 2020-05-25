/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebSocketSplitter.h"
#include <sys/types.h>
#if !defined(_WIN32)
#include <sys/socket.h>
#include <arpa/inet.h>
#endif //!defined(_WIN32)

#include "Util/logger.h"
#include "Util/util.h"
using namespace toolkit;

namespace mediakit {

/**
 *
  0             1                 2               3
  0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
 +-+-+-+-+-------+-+-------------+-------------------------------+
 |F|R|R|R| opcode|M| Payload len |    Extended payload length    |
 |I|S|S|S|  (4)  |A|     (7)     |             (16/64)           |
 |N|V|V|V|       |S|             |   (if payload len==126/127)   |
 | |1|2|3|       |K|             |                               |
 +-+-+-+-+-------+-+-------------+ - - - - - - - - - - - - - - - +
 |     Extended payload length continued, if payload len == 127  |
 + - - - - - - - - - - - - - - - +-------------------------------+
 |                               |Masking-key, if MASK set to 1  |
 +-------------------------------+-------------------------------+
 | Masking-key (continued)       |          Payload Data         |
 +-------------------------------- - - - - - - - - - - - - - - - +
 :                     Payload Data continued ...                :
 + - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - +
 |                     Payload Data continued ...                |
 +---------------------------------------------------------------+
 */

#define CHECK_LEN(size) \
do{ \
    if(len - (ptr - data) < size){ \
        if(_remain_data.empty()){ \
            _remain_data.assign((char *)data,len); \
        } \
        return ; \
    } \
}while(0) \

void WebSocketSplitter::decode(uint8_t *data,uint64_t len) {
    uint8_t *ptr = data;
    if(!_got_header) {
        //还没有获取数据头
        if(!_remain_data.empty()){
            _remain_data.append((char *)data,len);
            data = ptr = (uint8_t *)_remain_data.data();
            len = _remain_data.size();
        }

begin_decode:
        CHECK_LEN(1);
        _fin = (*ptr & 0x80) >> 7;
        _reserved = (*ptr & 0x70) >> 4;
        _opcode = (WebSocketHeader::Type) (*ptr & 0x0F);
        ptr += 1;

        CHECK_LEN(1);
        _mask_flag = (*ptr & 0x80) >> 7;
        _payload_len = (*ptr & 0x7F);
        ptr += 1;

        if (_payload_len == 126) {
            CHECK_LEN(2);
            _payload_len = (*ptr << 8) | *(ptr + 1);
            ptr += 2;
        } else if (_payload_len == 127) {
            CHECK_LEN(8);
            _payload_len = ((uint64_t) ptr[0] << (8 * 7)) |
                            ((uint64_t) ptr[1] << (8 * 6)) |
                            ((uint64_t) ptr[2] << (8 * 5)) |
                            ((uint64_t) ptr[3] << (8 * 4)) |
                            ((uint64_t) ptr[4] << (8 * 3)) |
                            ((uint64_t) ptr[5] << (8 * 2)) |
                            ((uint64_t) ptr[6] << (8 * 1)) |
                            ((uint64_t) ptr[7] << (8 * 0));
            ptr += 8;
        }
        if (_mask_flag) {
            CHECK_LEN(4);
            _mask.assign(ptr, ptr + 4);
            ptr += 4;
        }
        _got_header = true;
        _mask_offset = 0;
        _payload_offset = 0;
        onWebSocketDecodeHeader(*this);
        if(_payload_len == 0){
            onWebSocketDecodeComplete(*this);
        }
    }

    //进入后面逻辑代表已经获取到了webSocket协议头，

    uint64_t remain = len - (ptr - data);
    if(remain > 0){
        uint64_t payload_slice_len = remain;
        if(payload_slice_len + _payload_offset > _payload_len){
            payload_slice_len = _payload_len - _payload_offset;
        }
        _payload_offset += payload_slice_len;
        onPayloadData(ptr, payload_slice_len);

        if(_payload_offset == _payload_len){
            onWebSocketDecodeComplete(*this);

            //这是下一个包
            remain -= payload_slice_len;
            ptr += payload_slice_len;
            _got_header = false;

            if(remain > 0){
                //剩余数据是下一个包，把它的数据放置在缓存中
                string str((char *)ptr,remain);
                _remain_data = str;

                data = ptr = (uint8_t *)_remain_data.data();
                len = _remain_data.size();
                goto begin_decode;
            }
        }
    }
    _remain_data.clear();
}

void WebSocketSplitter::onPayloadData(uint8_t *data, uint64_t len) {
    if(_mask_flag){
        for(int i = 0; i < len ; ++i,++data){
            *(data) ^= _mask[(i + _mask_offset) % 4];
        }
        _mask_offset = (_mask_offset + len) % 4;
    }
    onWebSocketDecodePayload(*this, _mask_flag ? data - len : data, len, _payload_offset);
}

void WebSocketSplitter::encode(const WebSocketHeader &header,const Buffer::Ptr &buffer) {
    string ret;
    uint64_t len = buffer ? buffer->size() : 0;
    uint8_t byte = header._fin << 7 | ((header._reserved & 0x07) << 4) | (header._opcode & 0x0F) ;
    ret.push_back(byte);

    auto mask_flag = (header._mask_flag && header._mask.size() >= 4);
    byte = mask_flag << 7;

    if(len < 126){
        byte |= len;
        ret.push_back(byte);
    }else if(len <= 0xFFFF){
        byte |= 126;
        ret.push_back(byte);

        auto len_low = htons(len);
        ret.append((char *)&len_low,2);
    }else{
        byte |= 127;
        ret.push_back(byte);

        uint32_t len_high = htonl(len >> 32) ;
        uint32_t len_low = htonl(len & 0xFFFFFFFF);
        ret.append((char *)&len_high,4);
        ret.append((char *)&len_low,4);
    }
    if(mask_flag){
        ret.append((char *)header._mask.data(),4);
    }

    onWebSocketEncodeData(std::make_shared<BufferString>(std::move(ret)));

    if(len > 0){
        if(mask_flag){
            uint8_t *ptr = (uint8_t*)buffer->data();
            for(int i = 0; i < len ; ++i,++ptr){
                *(ptr) ^= header._mask[i % 4];
            }
        }
        onWebSocketEncodeData(buffer);
    }

}



} /* namespace mediakit */




