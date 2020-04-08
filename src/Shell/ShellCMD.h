/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_SHELL_SHELLCMD_H_
#define SRC_SHELL_SHELLCMD_H_

#include "Util/CMD.h"
#include "Common/MediaSource.h"
using namespace toolkit;

namespace mediakit {


class CMD_media: public CMD {
public:
    CMD_media(){
        _parser.reset(new OptionParser([](const std::shared_ptr<ostream> &stream,mINI &ini){
            MediaSource::for_each_media([&](const MediaSource::Ptr &media){
                if(!ini["schema"].empty() && ini["schema"] != media->getSchema()){
                    //筛选协议不匹配
                    return;
                }
                if(!ini["vhost"].empty() && ini["vhost"] != media->getVhost()){
                    //筛选虚拟主机不匹配
                    return;
                }
                if(!ini["app"].empty() && ini["app"] != media->getApp()){
                    //筛选应用名不匹配
                    return;
                }
                if(!ini["stream"].empty() && ini["stream"] != media->getId()){
                    //流id不匹配
                    return;
                }
                if(ini.find("list") != ini.end()){
                    //列出源
                    (*stream) << "\t"
                              << media->getSchema() << "/"
                              << media->getVhost() << "/"
                              << media->getApp() << "/"
                              << media->getId()
                              << "\r\n";
                    return;
                }

                EventPollerPool::Instance().getPoller()->async([ini,media,stream](){
                    if(ini.find("kick") != ini.end()){
                        //踢出源
                        do{
                            if(!media) {
                                break;
                            }
                            if(!media->close(true)) {
                                break;
                            }
                            (*stream) << "\t踢出成功:"
                                      << media->getSchema() << "/"
                                      << media->getVhost() << "/"
                                      << media->getApp() << "/"
                                      << media->getId()
                                      << "\r\n";
                            return;
                        }while(0);
                        (*stream) << "\t踢出失败:"
                                  << media->getSchema() << "/"
                                  << media->getVhost() << "/"
                                  << media->getApp() << "/"
                                  << media->getId()
                                  << "\r\n";
                    }
                },false);


            });
        }));
        (*_parser) << Option('k', "kick", Option::ArgNone,nullptr,false, "踢出媒体源", nullptr);
        (*_parser) << Option('l', "list", Option::ArgNone,nullptr,false, "列出媒体源", nullptr);
        (*_parser) << Option('S', "schema", Option::ArgRequired,nullptr,false, "协议筛选", nullptr);
        (*_parser) << Option('v', "vhost", Option::ArgRequired,nullptr,false, "虚拟主机筛选", nullptr);
        (*_parser) << Option('a', "app", Option::ArgRequired,nullptr,false, "应用名筛选", nullptr);
        (*_parser) << Option('s', "stream", Option::ArgRequired,nullptr,false, "流id筛选", nullptr);
    }
    virtual ~CMD_media() {}
    const char *description() const override {
        return "媒体源相关操作.";
    }
};

} /* namespace mediakit */

#endif //SRC_SHELL_SHELLCMD_H_