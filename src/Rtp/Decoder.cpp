/*
 * Copyright (c) 2016 The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/xiongziliang/ZLMediaKit).
 *
 * Use of this source code is governed by MIT license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#if defined(ENABLE_RTPPROXY)
#include "Decoder.h"
#include "PSDecoder.h"
#include "TSDecoder.h"
namespace mediakit {
Decoder::Ptr Decoder::createDecoder(Decoder::Type type) {
    switch (type){
        case decoder_ps : return std::make_shared<PSDecoder>();
        case decoder_ts : return std::make_shared<TSDecoder>();
        default : return nullptr;
    }
}

}//namespace mediakit
#endif//defined(ENABLE_RTPPROXY)
