/*
 * Copyright (c) 2021 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_TWCCCONTEXT_H
#define ZLMEDIAKIT_TWCCCONTEXT_H

#include <stdint.h>
#include <map>
#include "Util/TimeTicker.h"
using namespace toolkit;

class TwccContext {
public:
    //每个twcc rtcp包最多表明的rtp ext seq增量
    static constexpr size_t kMaxSeqDelta = 20;
    //每个twcc rtcp包发送的最大时间间隔，单位毫秒
    static constexpr size_t kMaxTimeDelta = 64;

    TwccContext() = default;
    ~TwccContext() = default;

    void onRtp(uint16_t twcc_ext_seq);

private:
    void onSendTwcc();
    bool checkIfNeedSendTwcc() const;
    int checkSeqStatus(uint16_t twcc_ext_seq) const;
    void clearStatus();

private:
    Ticker _ticker;
    uint64_t _min_stamp = 0;
    uint64_t _max_stamp;
    std::map<uint32_t /*twcc_ext_seq*/, uint64_t/*recv time in ms*/> _rtp_recv_status;
    uint8_t _twcc_pkt_count = 0;
};


#endif //ZLMEDIAKIT_TWCCCONTEXT_H
