/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */


#ifndef ZLMEDIAKIT_WEBRTC_SIGNALING_MSG_H
#define ZLMEDIAKIT_WEBRTC_SIGNALING_MSG_H

#include "server/WebApi.h"

namespace mediakit {
namespace Rtc {

#define SIGNALING_MSG_ARGS const HttpAllArgs<Json::Value>& allArgs

// WebRTC 信令消息键名和值常量声明
extern const char* const CLASS_KEY;
extern const char* const CLASS_VALUE_REQUEST;
extern const char* const CLASS_VALUE_INDICATION;        // 指示类型,不需要应答
extern const char* const CLASS_VALUE_ACCEPT;            // 作为CLASS_VALUE_REQUEST的应答
extern const char* const CLASS_VALUE_REJECT;            // 作为CLASS_VALUE_REQUEST的应答
extern const char* const METHOD_KEY;
extern const char* const METHOD_VALUE_REGISTER;         // 注册
extern const char* const METHOD_VALUE_UNREGISTER;       // 注销
extern const char* const METHOD_VALUE_CALL;             // 呼叫(取流或推流)

extern const char* const METHOD_VALUE_BYE;              //  挂断
extern const char* const METHOD_VALUE_CANDIDATE;
extern const char* const TRANSACTION_ID_KEY;            // 消息id,每条消息拥有一个唯一的id
extern const char* const ROOM_ID_KEY;
extern const char* const GUEST_ID_KEY;                 // 每个独立的会话，会拥有一个唯一的guest_id
extern const char* const SENDER_KEY;
extern const char* const TYPE_KEY;
extern const char* const TYPE_VALUE_PLAY;              // 拉流
extern const char* const TYPE_VALUE_PUSH;              // 推流
extern const char* const REASON_KEY;
extern const char* const CALL_VHOST_KEY;
extern const char* const CALL_APP_KEY;
extern const char* const CALL_STREAM_KEY;
extern const char* const SDP_KEY;

extern const char* const ICE_SERVERS_KEY;
extern const char* const CANDIDATE_KEY;
extern const char* const URL_KEY;
extern const char* const UFRAG_KEY;
extern const char* const PWD_KEY;

} // namespace Rtc
} // namespace mediakit
//

#endif //ZLMEDIAKIT_WEBRTC_SIGNALING_PEER_H
