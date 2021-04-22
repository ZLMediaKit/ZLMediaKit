/*
 * Copyright (c) 2016 The ZLToolKit project authors. All Rights Reserved.
 *
 * This file is part of ZLToolKit(https://github.com/xia-chu/ZLToolKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include <iostream>
#include "Util/logger.h"
#include "Rtcp/RtcpFCI.h"
using namespace std;
using namespace toolkit;
using namespace mediakit;

int main() {
    //初始化日志系统
    Logger::Instance().add(std::make_shared<ConsoleChannel> ());

    {
        FCI_SLI fci(0xFFFF, 0, 0xFF);
        InfoL << 0b10101010101 << " " << 0b01010101010 << " " << (int) 0b101010 << " " << hexdump(&fci, FCI_SLI::kSize);
        fci.net2Host();
        InfoL << fci.dumpString();
    }
    {
        FCI_FIR fci(123456, 139, 456789);
        InfoL << hexdump(&fci, FCI_FIR::kSize);
        fci.net2Host();
        InfoL << fci.dumpString();
    }
    {
        auto str = FCI_REMB::create({1234,2345,5678}, 4 * 1024 * 1024);
        FCI_REMB *ptr = (FCI_REMB *)str.data();
        ptr->net2Host(str.size());
        InfoL << ptr->dumpString();
    }
    {
        FCI_NACK nack(1234, vector<int>({1, 0, 0, 0, 1, 0, 1, 0, 1, 0}));
        nack.net2Host();
        InfoL << nack.dumpString();
    }
    {
        RunLengthChunk chunk(SymbolStatus::large_delta, 8024);
        InfoL << hexdump(&chunk, RunLengthChunk::kSize);
        InfoL << chunk.dumpString();
    }

    auto lam = [](const initializer_list<int> &lst){
        vector<SymbolStatus> ret;
        for(auto &num : lst){
            ret.emplace_back((SymbolStatus)num);
        }
        return ret;
    };
    {
        StatusVecChunk chunk(lam({0, 1, 0, 1, 0, 1, 0, 0, 0, 1, 1, 1, 0, 1}));
        InfoL << hexdump(&chunk, StatusVecChunk::kSize);
        InfoL << chunk.dumpString();
    }
    {
        StatusVecChunk chunk(lam({0, 1, 2, 3, 0, 1, 2}));
        InfoL << hexdump(&chunk, StatusVecChunk::kSize);
        InfoL << chunk.dumpString();
    }
    return 0;
}
