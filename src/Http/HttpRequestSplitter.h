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
     * @param len 数据长度
     */
    void input(const char *data,uint64_t len);
protected:
    /**
     * 收到请求头
     * @param data 请求头数据
     * @param len 请求头长度
     *
     * @return 请求头后的content长度,
     *  <0 : 代表后面所有数据都是content，此时后面的content将分段通过onRecvContent函数回调出去
     *  0 : 代表为后面数据还是请求头,
     *  >0 : 代表后面数据为固定长度content,此时将缓存content并等到所有content接收完毕一次性通过onRecvContent函数回调出去
     */
    virtual int64_t onRecvHeader(const char *data,uint64_t len) = 0;

    /**
     * 收到content分片或全部数据
     * onRecvHeader函数返回>0,则为全部数据
     * @param data content分片或全部数据
     * @param len 数据长度
     */
    virtual void onRecvContent(const char *data,uint64_t len) {};

    /**
     * 设置content len
     */
    void setContentLen(int64_t content_len);

    /**
     * 恢复初始设置
     */
     void reset();
private:
    string _remain_data;
    int64_t _content_len = 0;
};


#endif //ZLMEDIAKIT_HTTPREQUESTSPLITTER_H
