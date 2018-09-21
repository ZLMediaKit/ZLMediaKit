//
// Created by xzl on 2018/9/20.
//

#include "HttpRequestSplitter.h"

void HttpRequestSplitter::input(const string &data) {
    if(_remain_data.empty()){
        _remain_data = data;
    }else{
        _remain_data.append(data);
    }

splitPacket:

    //数据按照请求头处理
    size_t index;
    while (_content_len == 0 && (index = _remain_data.find("\r\n\r\n")) != std::string::npos ) {
        //_content_len == 0，这是请求头
        _content_len = onRecvHeader(_remain_data.substr(0, index + 4));
        _remain_data.erase(0, index + 4);
    }

    if(_remain_data.empty()){
        return;
    }

    if(_content_len > 0){
        //数据按照固定长度content处理
        if(_remain_data.size() < _content_len){
            //数据不够
            return;
        }
        //收到content数据，并且接受content完毕
        onRecvContent(_remain_data.substr(0,_content_len));
        _remain_data.erase(0,_content_len);
        //content处理完毕,后面数据当做请求头处理
        _content_len = 0;

        if(!_remain_data.empty()){
            //还有数据没有处理完毕
            goto splitPacket;
        }
    }else{
        //数据按照不固定长度content处理
        onRecvContent(_remain_data);
        _remain_data.clear();
    }
}

void HttpRequestSplitter::setContentLen(int64_t content_len) {
    _content_len = content_len;
}
