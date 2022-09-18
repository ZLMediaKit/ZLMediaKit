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
#include <functional>
#include "Util/TimeTicker.h"

namespace mediakit {

class TwccContext {
public:
    using onSendTwccCB = std::function<void(uint32_t ssrc, std::string fci)>;
    //每个twcc rtcp包最多表明的rtp ext seq增量
    static constexpr size_t kMaxSeqSize = 20;
    //每个twcc rtcp包发送的最大时间间隔，单位毫秒
    static constexpr size_t kMaxTimeDelta = 256;

    TwccContext() = default;
    ~TwccContext() = default;

    void onRtp(uint32_t ssrc, uint16_t twcc_ext_seq, uint64_t stamp_ms);
    void setOnSendTwccCB(onSendTwccCB cb);

private:
    void onSendTwcc(uint32_t ssrc);
    bool needSendTwcc() const;
    int checkSeqStatus(uint16_t twcc_ext_seq) const;
    void clearStatus();

private:
    uint64_t _min_stamp = 0;
    uint64_t _max_stamp;
    std::map<uint32_t /*twcc_ext_seq*/, uint64_t/*recv time in ms*/> _rtp_recv_status;
    uint8_t _twcc_pkt_count = 0;
    onSendTwccCB _cb;
};

}// namespace mediakit
#endif //ZLMEDIAKIT_TWCCCONTEXT_H
