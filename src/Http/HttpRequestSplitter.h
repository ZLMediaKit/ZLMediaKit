//
// Created by xzl on 2018/9/20.
//

#ifndef ZLMEDIAKIT_HTTPREQUESTSPLITTER_H
#define ZLMEDIAKIT_HTTPREQUESTSPLITTER_H

#include <string>
using namespace std;

class HttpRequestSplitter {
public:
    HttpRequestSplitter(){};
    virtual ~HttpRequestSplitter(){};

    /**
     * 添加数据
     * @param data 需要添加的数据
     */
    void input(const string &data);
protected:
    /**
     * 收到请求头
     * @param header 请求头
     * @return 请求头后的content长度,
     *  <0 : 代表后面所有数据都是content
     *  0 : 代表为后面数据还是请求头,
     *  >0 : 代表后面数据为固定长度content,
     */
    virtual int64_t onRecvHeader(const string &header) = 0;

    /**
     * 收到content分片或全部数据
     * onRecvHeader函数返回>0,则为全部数据
     * @param content
     */
    virtual void onRecvContent(const string &content) = 0;
private:
    string _remain_data;
    int64_t _content_len = 0;
};


#endif //ZLMEDIAKIT_HTTPREQUESTSPLITTER_H
