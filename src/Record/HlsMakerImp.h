/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef HLSMAKERIMP_H
#define HLSMAKERIMP_H

#include <memory>
#include <string>
#include <stdlib.h>
#include "HlsMaker.h"
#include "HlsMediaSource.h"
using namespace std;

namespace mediakit {

class HlsMakerImp : public HlsMaker{
public:
    HlsMakerImp(const string &m3u8_file,
                const string &params,
                uint32_t bufSize  = 64 * 1024,
                float seg_duration = 5,
                uint32_t seg_number = 3);
    virtual ~HlsMakerImp();

    /**
     * 设置媒体信息
     * @param vhost 虚拟主机
     * @param app 应用名
     * @param stream_id 流id
     */
    void setMediaSource(const string &vhost, const string &app, const string &stream_id);

    /**
     * 获取MediaSource
     * @return
     */
     MediaSource::Ptr getMediaSource() const;
protected:
    string onOpenSegment(int index) override ;
    void onDelSegment(int index) override;
    void onWriteSegment(const char *data, int len) override;
    void onWriteHls(const char *data, int len) override;
private:
    std::shared_ptr<FILE> makeFile(const string &file,bool setbuf = false);
private:
    HlsMediaSource::Ptr _media_src;
    map<int /*index*/,string/*file_path*/> _segment_file_paths;
    std::shared_ptr<FILE> _file;
    std::shared_ptr<char> _file_buf;
    string _path_prefix;
    string _path_hls;
    string _params;
    int _buf_size;
};

}//namespace mediakit
#endif //HLSMAKERIMP_H
