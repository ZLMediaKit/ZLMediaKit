/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_HTTPREQUESTSPLITTER_H
#define ZLMEDIAKIT_HTTPREQUESTSPLITTER_H

#include <string>
#include "Network/Buffer.h"

namespace mediakit {
/*
包分割器(框架)
分包逻辑如下：
- onSearchPacketTail获取头部长度
- onRecvHeader解析头部，获取_content_len
- 根据 _content_len 来缓存数据，并回调 onRecvContent，并重置 onRecvContent
*/
class HttpRequestSplitter {
public:
    HttpRequestSplitter(){};
    virtual ~HttpRequestSplitter(){};

    /**
     * 添加数据
     * @param data 需要添加的数据
     * @param len 数据长度
     * @warning 实际内存需保证不小于 len + 1, 内部使用 strstr 进行查找, 为防止查找越界, 会在 @p len + 1 的位置设置 '\0' 结束符.
     */
    virtual void input(const char *data, size_t len);

    /**
     * 恢复初始设置
     */
    void reset();

    /**
     * 剩余数据大小
     */
    size_t remainDataSize();

    /**
     * 获取剩余数据指针
     */
    const char *remainData() const;

protected:
    /**
     * 解析请求头，返回content-length
     * @param data 请求头数据
     * @param len 请求头长度
     *
     * @return 请求头后的content长度，并赋予_content_len
     * @retval 0    代表为后面数据还是请求头, 此时不走context-length分帧和对应onRecvContent回调，
     * 子类必须加入自己数据回调(在onSearchPacketTail或onRecvHeader中, @see TSSegment, PSDecoder)，
     * 此时本类将退化成一个普通分包器, 具体分包逻辑如下： 
     *     input -> [onSearchPacketTail -> onRecvHeader] ... 
     * @retval > 0  代表后面数据为固定长度content, 将在收到所有content后，一次性通过onRecvContent函数回调出去
     * @retval < 0  代表后面是不定长的content，此时后面的content将分段通过onRecvContent函数回调出去
     */
    virtual ssize_t onRecvHeader(const char *data,size_t len) = 0;

    /**
     * 收到content分片或全部数据
     * onRecvHeader函数返回>0,则为全部数据
     * @param data content分片或全部数据
     * @param len 数据长度
     */
    virtual void onRecvContent(const char *data,size_t len) {};

    /**
     * 判断数据中是否有包尾
     * @param data 数据指针
     * @param len 数据长度
     * @return nullptr代表未找到包位，否则返回包尾指针
     */
    virtual const char *onSearchPacketTail(const char *data, size_t len);

    /**
     * 设置content len
     */
    void setContentLen(ssize_t content_len);

private:
    ssize_t _content_len = 0;
    size_t _remain_data_size = 0;
    toolkit::BufferLikeString _remain_data;
};

} /* namespace mediakit */

#endif //ZLMEDIAKIT_HTTPREQUESTSPLITTER_H
