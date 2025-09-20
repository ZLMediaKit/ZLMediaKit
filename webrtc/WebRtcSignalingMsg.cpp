/*
 * Copyright (c) 2016-present The ZLMediaKit project authors. All Rights Reserved.
 *
 * This file is part of ZLMediaKit(https://github.com/ZLMediaKit/ZLMediaKit).
 *
 * Use of this source code is governed by MIT-like license that can be found in the
 * LICENSE file in the root of the source tree. All contributing project authors
 * may be found in the AUTHORS file in the root of the source tree.
 */

#include "WebRtcSignalingMsg.h"

namespace mediakit {
namespace Rtc {

// WebRTC 信令消息键名和值常量定义
const char* const CLASS_KEY = "class";
const char* const CLASS_VALUE_REQUEST = "request";
const char* const CLASS_VALUE_INDICATION = "indication";        // 指示类型,不需要应答
const char* const CLASS_VALUE_ACCEPT = "accept";                // 作为CLASS_VALUE_REQUEST的应答
const char* const CLASS_VALUE_REJECT = "reject";                // 作为CLASS_VALUE_REQUEST的应答
const char* const METHOD_KEY = "method";
const char* const METHOD_VALUE_REGISTER = "register";           // 注册
const char* const METHOD_VALUE_UNREGISTER = "unregister";       // 注销
const char* const METHOD_VALUE_CALL = "call";                   // 呼叫(取流或推流)

const char* const METHOD_VALUE_BYE = "bye";                     //  挂断
const char* const METHOD_VALUE_CANDIDATE = "candidate";
const char* const TRANSACTION_ID_KEY = "transaction_id";        // 消息id,每条消息拥有一个唯一的id
const char* const ROOM_ID_KEY = "room_id";
const char* const GUEST_ID_KEY = "guest_id";                    // 每个独立的会话，会拥有一个唯一的guest_id
const char* const SENDER_KEY = "sender";
const char* const TYPE_KEY = "type";
const char* const TYPE_VALUE_PLAY = "play";                     // 拉流
const char* const TYPE_VALUE_PUSH = "push";                     // 推流
const char* const REASON_KEY = "reason";
const char* const CALL_VHOST_KEY = "vhost";
const char* const CALL_APP_KEY = "app";
const char* const CALL_STREAM_KEY = "stream";
const char* const SDP_KEY = "sdp";

const char* const ICE_SERVERS_KEY = "ice_servers";
const char* const CANDIDATE_KEY = "candidate";
const char* const URL_KEY = "url";
const char* const UFRAG_KEY = "ufrag";
const char* const PWD_KEY = "pwd";

} // namespace Rtc
} // namespace mediakit
