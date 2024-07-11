﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_WEBSOCKETSPLITTER_H
#define ZLMEDIAKIT_WEBSOCKETSPLITTER_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
#include "Network/Buffer.h"

//websocket组合包最大不得超过4MB(防止内存爆炸)
#define MAX_WS_PACKET (4 * 1024 * 1024)

namespace mediakit {

class WebSocketHeader {
public:
    using Ptr = std::shared_ptr<WebSocketHeader>;
    typedef enum {
        CONTINUATION = 0x0,
        TEXT = 0x1,
        BINARY = 0x2,
        RSV3 = 0x3,
        RSV4 = 0x4,
        RSV5 = 0x5,
        RSV6 = 0x6,
        RSV7 = 0x7,
        CLOSE = 0x8,
        PING = 0x9,
        PONG = 0xA,
        CONTROL_RSVB = 0xB,
        CONTROL_RSVC = 0xC,
        CONTROL_RSVD = 0xD,
        CONTROL_RSVE = 0xE,
        CONTROL_RSVF = 0xF
    } Type;
public:

    WebSocketHeader() : _mask(4){
        //获取_mask内部buffer的内存地址，该内存是malloc开辟的，地址为随机
        uint64_t ptr = (uint64_t)(&_mask[0]);
        //根据内存地址设置掩码随机数
        _mask.assign((uint8_t*)(&ptr), (uint8_t*)(&ptr) + 4);
    }

    virtual ~WebSocketHeader() = default;

public:
    bool _fin;
    uint8_t _reserved;
    Type _opcode;
    bool _mask_flag;
    size_t _payload_len;
    std::vector<uint8_t > _mask;
};

//websocket协议收到的字符串类型缓存，用户协议层获取该数据传输的方式
class WebSocketBuffer : public toolkit::BufferString {
public:
    using Ptr = std::shared_ptr<WebSocketBuffer>;

    template<typename ...ARGS>
    WebSocketBuffer(WebSocketHeader::Type headType, bool fin, ARGS &&...args)
            :  toolkit::BufferString(std::forward<ARGS>(args)...), _fin(fin), _head_type(headType){}

    WebSocketHeader::Type headType() const { return _head_type; }

    bool isFinished() const { return _fin; };

private:
    bool _fin;
    WebSocketHeader::Type _head_type;
};

class WebSocketSplitter : public WebSocketHeader{
public:
    /**
     * 输入数据以便解包webSocket数据以及处理粘包问题
     * 可能触发onWebSocketDecodeHeader和onWebSocketDecodePayload回调
     * @param data 需要解包的数据，可能是不完整的包或多个包
     * @param len 数据长度
     */
    void decode(uint8_t *data, size_t len);

    /**
     * 编码一个数据包
     * 将触发2次onWebSocketEncodeData回调
     * @param header 数据头
     * @param buffer 负载数据
     */
    void encode(const WebSocketHeader &header,const toolkit::Buffer::Ptr &buffer);

protected:
    /**
     * 收到一个webSocket数据包包头，后续将继续触发onWebSocketDecodePayload回调
     * @param header 数据包头
     */
    virtual void onWebSocketDecodeHeader(const WebSocketHeader &header) {};

    /**
     * 收到webSocket数据包负载
     * @param header 数据包包头
     * @param ptr 负载数据指针
     * @param len 负载数据长度
     * @param recved 已接收数据长度(包含本次数据长度)，等于header._payload_len时则接受完毕
     */
    virtual void onWebSocketDecodePayload(const WebSocketHeader &header, const uint8_t *ptr, size_t len, size_t recved) {};

    /**
     * 接收到完整的一个webSocket数据包后回调
     * @param header 数据包包头
     */
    virtual void onWebSocketDecodeComplete(const WebSocketHeader &header) {};

    /**
     * websocket数据编码回调
     * @param ptr 数据指针
     * @param len 数据指针长度
     */
    virtual void onWebSocketEncodeData(toolkit::Buffer::Ptr buffer){};

private:
    void onPayloadData(uint8_t *data, size_t len);

private:
    bool _got_header = false;
    int _mask_offset = 0;
    size_t _payload_offset = 0;
    std::string _remain_data;
};

} /* namespace mediakit */


#endif //ZLMEDIAKIT_WEBSOCKETSPLITTER_H
