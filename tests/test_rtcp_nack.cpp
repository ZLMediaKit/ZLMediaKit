/*
 * Copyright (c) 2016-present The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include "Util/logger.h"
#include "../webrtc/Nack.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

int main() {
    Logger::Instance().add(std::make_shared<ConsoleChannel>());
    Logger::Instance().setWriter(std::make_shared<AsyncLogWriter>());

    srand((unsigned)time(NULL));
    NackContext ctx;
    ctx.setOnNack([](const FCI_NACK &nack){
        InfoL << nack.dumpString();
    });
    auto drop_start = 0;
    auto drop_len = 0;
    uint16_t offset = 0xFFFF - 200 - 50;
    for (int i = 1; i < 10000; ++i) {
        if (i % 100 == 0) {
            drop_start = i + rand() % 16;
            drop_len = 4 + rand() % 16;
            InfoL << "start drop:" << (uint16_t)(drop_start + offset) << " -> "
                  << (uint16_t)(drop_start + offset + drop_len);
        }
        uint16_t seq =  i + offset;
        if ((i >= drop_start && i <= drop_start + drop_len) || seq == 65535 || seq == 0 || seq == 1) {
            TraceL << "drop:" << (uint16_t)(i + offset);
        } else {
            static auto last_seq = seq;
            if (seq - last_seq > 16) {
                ctx.received(last_seq);
                ctx.received(seq);
                DebugL << "seq reduce:" << last_seq;
                last_seq = seq;
            } else {
                ctx.received(seq);
            }
        }
    }
    sleep(1);
    return 0;
}
