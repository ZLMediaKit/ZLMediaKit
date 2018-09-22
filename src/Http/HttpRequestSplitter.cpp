//
// Created by xzl on 2018/9/20.
//

#include "HttpRequestSplitter.h"
#include "Util/logger.h"
#include "Util/util.h"
using namespace ZL::Util;

void HttpRequestSplitter::input(const char *data,uint64_t len) {
    const char *ptr = data;
    if(!_remain_data.empty()){
        _remain_data.append(data,len);
        data = ptr = _remain_data.data();
        len = _remain_data.size();
    }

splitPacket:

    //数据按照请求头处理
    char *index = nullptr;
    while (_content_len == 0 && (index = strstr(ptr,"\r\n\r\n")) != nullptr) {
        //_content_len == 0，这是请求头
        _content_len = onRecvHeader(ptr, index - ptr + 4);
        ptr = index + 4;
    }

    uint64_t remain = len - (ptr - data);
    if(remain <= 0){
        //没有剩余数据，清空缓存
        _remain_data.clear();
        return;
    }

    if(_content_len == 0){
        //尚未找到http头，缓存定位到剩余数据部分
        _remain_data.assign(ptr,remain);
        return;
    }

    //已经找到http头了
    if(_content_len > 0){
        //数据按照固定长度content处理
        if(remain < _content_len){
            //数据不够，缓存定位到剩余数据部分
            _remain_data.assign(ptr,remain);
            return;
        }
        //收到content数据，并且接受content完毕
        onRecvContent(ptr,_content_len);

        remain -= _content_len;
        ptr += _content_len;
        //content处理完毕,后面数据当做请求头处理
        _content_len = 0;

        if(remain > 0){
            //还有数据没有处理完毕
            _remain_data.assign(ptr,remain);

            data = ptr = (char *)_remain_data.data();
            len = _remain_data.size();
            goto splitPacket;
        }
        return;
    }


    //_content_len < 0;数据按照不固定长度content处理
    onRecvContent(ptr,remain);//消费掉所有剩余数据
    _remain_data.clear();
}

void HttpRequestSplitter::setContentLen(int64_t content_len) {
    _content_len = content_len;
}
