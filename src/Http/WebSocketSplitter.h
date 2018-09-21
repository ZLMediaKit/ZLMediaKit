//
// Created by xzl on 2018/9/21.
//

#ifndef ZLMEDIAKIT_WEBSOCKETSPLITTER_H
#define ZLMEDIAKIT_WEBSOCKETSPLITTER_H

#include <cstdint>
#include <string>
#include <vector>
#include <memory>
using namespace std;

class WebSocketHeader {
public:
    typedef std::shared_ptr<WebSocketHeader> Ptr;
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
    bool _fin;
    uint8_t _reserved;
    Type _opcode;
    bool _mask_flag;
    uint64_t _playload_len;
    vector<uint8_t > _mask;
};

class WebSocketSplitter : public WebSocketHeader{
public:
    WebSocketSplitter(){}
    virtual ~WebSocketSplitter(){}

    void decode(uint8_t *data,uint64_t len);
    void encode(uint8_t *data,uint64_t len);
protected:
    virtual void onWebSocketHeader(const WebSocketHeader &packet) {};
    virtual void onWebSocketPlayload(const WebSocketHeader &packet,const uint8_t *ptr,uint64_t len,uint64_t recved) {};
private:
    void onPlayloadData(uint8_t *data,uint64_t len);
private:
    string _remain_data;
    int _mask_offset = 0;
    bool _got_header = false;
    uint64_t _playload_offset = 0;
};


#endif //ZLMEDIAKIT_WEBSOCKETSPLITTER_H
