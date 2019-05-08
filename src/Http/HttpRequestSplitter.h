/*
 * MIT License
 *
 * Copyright (c) 2016-2019 xiongziliang <771730766@qq.com>
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#ifndef ZLMEDIAKIT_HTTPREQUESTSPLITTER_H
#define ZLMEDIAKIT_HTTPREQUESTSPLITTER_H

#include <string>
using namespace std;

namespace mediakit {

class HttpRequestSplitter {
public:
    HttpRequestSplitter(){};
    virtual ~HttpRequestSplitter(){};

    /**
     * 添加数据
     * @param data 需要添加的数据
     * @param len 数据长度
     */
    virtual void input(const char *data,uint64_t len);
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
     * 判断数据中是否有包尾
     * @param data 数据指针
     * @param len 数据长度
     * @return nullptr代表未找到包位，否则返回包尾指针
     */
    virtual const char *onSearchPacketTail(const char *data,int len);

    /**
     * 设置content len
     */
    void setContentLen(int64_t content_len);

    /**
     * 恢复初始设置
     */
     void reset();

     /**
      * 剩余数据大小
      */
     int64_t remainDataSize();
private:
    string _remain_data;
    int64_t _content_len = 0;
    int64_t _remain_data_size = 0;
};

} /* namespace mediakit */

#endif //ZLMEDIAKIT_HTTPREQUESTSPLITTER_H
