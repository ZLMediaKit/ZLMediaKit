﻿/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef SRC_SHELL_SHELLCMD_H_
#define SRC_SHELL_SHELLCMD_H_

#include "Util/CMD.h"
#include "Common/MediaSource.h"

namespace mediakit {

class CMD_media : public toolkit::CMD {
public:
    CMD_media() {
        _parser.reset(new toolkit::OptionParser([](const std::shared_ptr<std::ostream> &stream, toolkit::mINI &ini) {
            MediaSource::for_each_media([&](const MediaSource::Ptr &media) {
                if (ini.find("list") != ini.end()) {
                    //列出源
                    (*stream) << "\t" << media->getUrl() << "\r\n";
                    return;
                }

                toolkit::EventPollerPool::Instance().getPoller()->async([ini, media, stream]() {
                    if (ini.find("kick") != ini.end()) {
                        //踢出源
                        do {
                            if (!media) {
                                break;
                            }
                            if (!media->close(true)) {
                                break;
                            }
                            (*stream) << "\t踢出成功:" << media->getUrl() << "\r\n";
                            return;
                        } while (0);
                        (*stream) << "\t踢出失败:" << media->getUrl() << "\r\n";
                    }
                }, false);


            }, ini["schema"], ini["vhost"], ini["app"], ini["stream"]);
        }));
        (*_parser) << toolkit::Option('k', "kick", toolkit::Option::ArgNone, nullptr, false, "踢出媒体源", nullptr);
        (*_parser) << toolkit::Option('l', "list", toolkit::Option::ArgNone, nullptr, false, "列出媒体源", nullptr);
        (*_parser) << toolkit::Option('S', "schema", toolkit::Option::ArgRequired, nullptr, false, "协议筛选", nullptr);
        (*_parser) << toolkit::Option('v', "vhost", toolkit::Option::ArgRequired, nullptr, false, "虚拟主机筛选", nullptr);
        (*_parser) << toolkit::Option('a', "app", toolkit::Option::ArgRequired, nullptr, false, "应用名筛选", nullptr);
        (*_parser) << toolkit::Option('s', "stream", toolkit::Option::ArgRequired, nullptr, false, "流id筛选", nullptr);
    }

    const char *description() const override {
        return "媒体源相关操作.";
    }
};

} /* namespace mediakit */

#endif //SRC_SHELL_SHELLCMD_H_