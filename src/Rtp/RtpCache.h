/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xia-chu/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#ifndef ZLMEDIAKIT_RTPCACHE_H
#define ZLMEDIAKIT_RTPCACHE_H

#if defined(ENABLE_RTPPROXY)

#include "PSEncoder.h"
#include "Extension/CommonRtp.h"

namespace mediakit{

class RtpCache : public PacketCache<Buffer> {
public:
    using onFlushed = function<void(std::shared_ptr<List<Buffer::Ptr> >)>;
    RtpCache(onFlushed cb);
    ~RtpCache() override = default;

protected:
    /**
     * 输入rtp(目的是为了合并写)
     * @param buffer rtp数据
     */
    void input(uint64_t stamp, Buffer::Ptr buffer);

protected:
    void onFlush(std::shared_ptr<List<Buffer::Ptr> > rtp_list, bool) override;

private:
    onFlushed _cb;
};

class RtpCachePS : public RtpCache, public PSEncoderImp{
public:
    RtpCachePS(onFlushed cb, uint32_t ssrc, uint8_t payload_type = 96) : RtpCache(std::move(cb)), PSEncoderImp(ssrc, payload_type) {};
    ~RtpCachePS() override = default;

protected:
    void onRTP(Buffer::Ptr rtp)  override;
};

}//namespace mediakit
#endif//ENABLE_RTPPROXY
#endif //ZLMEDIAKIT_RTPCACHE_H
