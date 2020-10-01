/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include <list>
#include "Rtsp/RtpReceiver.h"
using namespace mediakit;

//该测试程序由于检验rtp排序算法的正确性
int main(int argc, char *argv[]) {
    srand((unsigned) time(NULL));
    PacketSortor<uint16_t, uint16_t> sortor;
    list<uint16_t> input_list, sorted_list;
    sortor.setOnSort([&](uint16_t seq, const uint16_t &packet) {
        sorted_list.push_back(seq);
    });

    for (int i = 0; i < 1000;) {
        int count = 1 + rand() % 8;
        for (int j = i + count; j >= i; --j) {
            auto seq = j;
            sortor.sortPacket(seq, seq);
            input_list.push_back(seq);
        }
        i += (count + 1);
    }

    {
        cout << "排序前:" << endl;
        int i = 0;
        for (auto &item : input_list) {
            cout << item << " ";
            if (++i % 10 == 0) {
                cout << endl;
            }
        }
        cout << endl;
    }
    {
        cout << "排序后:" << endl;
        int i = 0;
        for (auto &item : sorted_list) {
            cout << item << " ";
            if (++i % 10 == 0) {
                cout << endl;
            }
        }
        cout << endl;
    }
    return 0;
}