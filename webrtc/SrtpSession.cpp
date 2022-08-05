/**
ISC License

Copyright © 2015, Iñaki Baz Castillo <ibc@aliax.net>

Permission to use, copy, modify, and/or distribute this software for any
purpose with or without fee is hereby granted, provided that the above
copyright notice and this permission notice appear in all copies.

THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#define MS_CLASS "RTC::SrtpSession"
// #define MS_LOG_DEV_LEVEL 3

#include "SrtpSession.hpp"

#include "Util/logger.h"
#include "Util/util.h"
#include "logger.h"

#include <srtp2/srtp.h>

#include <cstring> // std::memset(), std::memcpy()
#include <vector>

using namespace toolkit;

namespace RTC {

/* Static. */

static std::vector<const char *> errors = {
    // From 0 (srtp_err_status_ok) to 24 (srtp_err_status_pfkey_err).
    "success (srtp_err_status_ok)",
    "unspecified failure (srtp_err_status_fail)",
    "unsupported parameter (srtp_err_status_bad_param)",
    "couldn't allocate memory (srtp_err_status_alloc_fail)",
    "couldn't deallocate memory (srtp_err_status_dealloc_fail)",
    "couldn't initialize (srtp_err_status_init_fail)",
    "can’t process as much data as requested (srtp_err_status_terminus)",
    "authentication failure (srtp_err_status_auth_fail)",
    "cipher failure (srtp_err_status_cipher_fail)",
    "replay check failed (bad index) (srtp_err_status_replay_fail)",
    "replay check failed (index too old) (srtp_err_status_replay_old)",
    "algorithm failed test routine (srtp_err_status_algo_fail)",
    "unsupported operation (srtp_err_status_no_such_op)",
    "no appropriate context found (srtp_err_status_no_ctx)",
    "unable to perform desired validation (srtp_err_status_cant_check)",
    "can’t use key any more (srtp_err_status_key_expired)",
    "error in use of socket (srtp_err_status_socket_err)",
    "error in use POSIX signals (srtp_err_status_signal_err)",
    "nonce check failed (srtp_err_status_nonce_bad)",
    "couldn’t read data (srtp_err_status_read_fail)",
    "couldn’t write data (srtp_err_status_write_fail)",
    "error parsing data (srtp_err_status_parse_err)",
    "error encoding data (srtp_err_status_encode_err)",
    "error while using semaphores (srtp_err_status_semaphore_err)",
    "error while using pfkey (srtp_err_status_pfkey_err)"
};

/* Static methods. */

class DepLibSRTP : public std::enable_shared_from_this<DepLibSRTP> {
public:
    using Ptr = std::shared_ptr<DepLibSRTP>;
    ~DepLibSRTP();

    static bool IsError(srtp_err_status_t code);
    static const char *GetErrorString(srtp_err_status_t code);
    static DepLibSRTP &Instance();

private:
    DepLibSRTP();
};

const char *DepLibSRTP::GetErrorString(srtp_err_status_t code) {
    // This throws out_of_range if the given index is not in the vector.
    return errors.at(code);
}

bool DepLibSRTP::IsError(srtp_err_status_t code) {
    return (code != srtp_err_status_ok);
}

INSTANCE_IMP(DepLibSRTP);

DepLibSRTP::DepLibSRTP() {
    MS_TRACE();

    MS_DEBUG_TAG(info, "libsrtp version: \"%s\"", srtp_get_version_string());

    srtp_err_status_t err = srtp_init();

#if 0
        srtp_install_log_handler([](srtp_log_level_t level,
                                    const char *msg,
                                    void *data) {
            printf("%s\n", msg);
        }, nullptr);
        srtp_set_debug_module("srtp", 1);
        srtp_set_debug_module("hmac sha-1", 1);
        srtp_set_debug_module("aes icm", 1);
        srtp_set_debug_module("alloc", 1);
        srtp_set_debug_module("stat test", 1);
        srtp_set_debug_module("cipher", 1);
        srtp_set_debug_module("auth func", 1);
        srtp_set_debug_module("crypto kernel", 1);
        srtp_list_debug_modules();
#endif

    if (DepLibSRTP::IsError(err)) {
        MS_THROW_ERROR("srtp_init() failed: %s", DepLibSRTP::GetErrorString(err));
    }

    // Set libsrtp event handler.
    err = srtp_install_event_handler([](srtp_event_data_t *data) {
        MS_TRACE();
        switch (data->event) {
            case event_ssrc_collision: MS_WARN_TAG(srtp, "SSRC collision occurred"); break;
            case event_key_soft_limit: MS_WARN_TAG(srtp, "stream reached the soft key usage limit and will expire soon"); break;
            case event_key_hard_limit: MS_WARN_TAG(srtp, "stream reached the hard key usage limit and has expired"); break;
            case event_packet_index_limit: MS_WARN_TAG(srtp, "stream reached the hard packet limit (2^48 packets)"); break;
        }
    });

    if (DepLibSRTP::IsError(err)) {
        MS_THROW_ERROR("srtp_install_event_handler() failed: %s", DepLibSRTP::GetErrorString(err));
    }
}

DepLibSRTP::~DepLibSRTP() {
    MS_TRACE();
    srtp_shutdown();
}

/////////////////////////////////////////////////////////////////////////////////////

/* Instance methods. */

SrtpSession::SrtpSession(Type type, CryptoSuite cryptoSuite, uint8_t *key, size_t keyLen) {
    _env = DepLibSRTP::Instance().shared_from_this();
    MS_TRACE();

    srtp_policy_t policy; // NOLINT(cppcoreguidelines-pro-type-member-init)

    // Set all policy fields to 0.
    std::memset(&policy, 0, sizeof(srtp_policy_t));

    switch (cryptoSuite) {
    case CryptoSuite::AES_CM_128_HMAC_SHA1_80: {
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtp);
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);

        break;
    }

    case CryptoSuite::AES_CM_128_HMAC_SHA1_32: {
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_32(&policy.rtp);
        // NOTE: Must be 80 for RTCP.
        srtp_crypto_policy_set_aes_cm_128_hmac_sha1_80(&policy.rtcp);

        break;
    }

    case CryptoSuite::AEAD_AES_256_GCM: {
        srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtp);
        srtp_crypto_policy_set_aes_gcm_256_16_auth(&policy.rtcp);

        break;
    }

    case CryptoSuite::AEAD_AES_128_GCM: {
        srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtp);
        srtp_crypto_policy_set_aes_gcm_128_16_auth(&policy.rtcp);

        break;
    }

    default: {
        MS_ABORT("unknown SRTP crypto suite");
    }
    }

    MS_ASSERT((int)keyLen == policy.rtp.cipher_key_len, "given keyLen does not match policy.rtp.cipher_keyLen");

    switch (type) {
    case Type::INBOUND:
        policy.ssrc.type = ssrc_any_inbound;
        break;

    case Type::OUTBOUND:
        policy.ssrc.type = ssrc_any_outbound;
        break;
    }

    policy.ssrc.value = 0;
    policy.key = key;
    // Required for sending RTP retransmission without RTX.
    policy.allow_repeat_tx = 1;
    policy.window_size = 1024;
    policy.next = nullptr;

    // Set the SRTP session.
    srtp_err_status_t err = srtp_create(&this->session, &policy);

    if (DepLibSRTP::IsError(err))
        MS_THROW_ERROR("srtp_create() failed: %s", DepLibSRTP::GetErrorString(err));
}

SrtpSession::~SrtpSession() {
    MS_TRACE();

    if (this->session != nullptr) {
        srtp_err_status_t err = srtp_dealloc(this->session);

        if (DepLibSRTP::IsError(err))
            MS_ABORT("srtp_dealloc() failed: %s", DepLibSRTP::GetErrorString(err));
    }
}

bool SrtpSession::EncryptRtp(uint8_t *data, int *len) {
    MS_TRACE();
    srtp_err_status_t err = srtp_protect(this->session, static_cast<void *>(data), reinterpret_cast<int *>(len));

    if (DepLibSRTP::IsError(err)) {
        WarnL << "srtp_protect() failed:" << DepLibSRTP::GetErrorString(err);
        return false;
    }

    return true;
}

bool SrtpSession::DecryptSrtp(uint8_t *data, int *len) {
    MS_TRACE();

    srtp_err_status_t err = srtp_unprotect(this->session, static_cast<void *>(data), reinterpret_cast<int *>(len));

    if (DepLibSRTP::IsError(err)) {
        WarnL << "srtp_unprotect() failed:" << DepLibSRTP::GetErrorString(err);
        return false;
    }

    return true;
}

bool SrtpSession::EncryptRtcp(uint8_t *data, int *len) {
    MS_TRACE();
    srtp_err_status_t err = srtp_protect_rtcp(this->session, static_cast<void *>(data), reinterpret_cast<int *>(len));

    if (DepLibSRTP::IsError(err)) {
        WarnL << "srtp_protect_rtcp() failed:" << DepLibSRTP::GetErrorString(err);
        return false;
    }

    return true;
}

bool SrtpSession::DecryptSrtcp(uint8_t *data, int *len) {
    MS_TRACE();

    srtp_err_status_t err = srtp_unprotect_rtcp(this->session, static_cast<void *>(data), reinterpret_cast<int *>(len));

    if (DepLibSRTP::IsError(err)) {
        WarnL << "srtp_unprotect_rtcp() failed:" << DepLibSRTP::GetErrorString(err);
        return false;
    }

    return true;
}

void SrtpSession::RemoveStream(uint32_t ssrc) {
    srtp_remove_stream(this->session, uint32_t { htonl(ssrc) });
}

} // namespace RTC
