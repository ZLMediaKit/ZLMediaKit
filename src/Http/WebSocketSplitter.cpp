//
// Created by xzl on 2018/9/21.
//

#include "WebSocketSplitter.h"
#include "Util/logger.h"
#include "Util/util.h"
using namespace ZL::Util;
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
        _playload_len = (*ptr & 0x7F);
        ptr += 1;

        if (_playload_len == 126) {
            CHECK_LEN(2);
            _playload_len = (*ptr << 8) | *(ptr + 1);
            ptr += 2;
        } else if (_playload_len == 127) {
            CHECK_LEN(8);
            _playload_len = ((uint64_t) ptr[0] << (8 * 7)) |
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
        _playload_offset = 0;
        onWebSocketHeader(*this);
    }

    uint64_t remain = len - (ptr - data);
    if(remain > 0){
        uint64_t playload_slice_len = remain;
        if(playload_slice_len + _playload_offset > _playload_len){
            playload_slice_len = _playload_len - _playload_offset;
        }
        onPlayloadData(ptr,playload_slice_len);
        _playload_offset += playload_slice_len;

        if(_playload_offset == _playload_len){
            //这是下一个包
            if(remain - playload_slice_len > 0){
                string nextPacket((char *)ptr + playload_slice_len,remain - playload_slice_len);
                _got_header = false;
                _remain_data.clear();

                data = ptr = (uint8_t *)nextPacket.data();
                len = nextPacket.size();
                goto begin_decode;
            } else{
                _got_header = false;
            }
        }
    }
    _remain_data.clear();
}

void WebSocketSplitter::onPlayloadData(uint8_t *ptr, uint64_t len) {
    if(_mask_flag){
        for(int i = 0; i < len ; ++i,++ptr){
            *(ptr) ^= _mask[(i + _mask_offset) % 4];
        }
        _mask_offset = (_mask_offset + len) % 4;
    }
    onWebSocketPlayload(*this,_mask_flag ? ptr - len : ptr,len,_playload_offset);
}
