#include "ice_server.h"

#include <iostream>

static constexpr size_t StunSerializeBufferSize{65536};
static uint8_t StunSerializeBuffer[StunSerializeBufferSize];

IceServer::IceServer() {}

IceServer::~IceServer() {}

IceServer::IceServer(const std::string &username_fragment, const std::string &password)
        : username_fragment_(username_fragment), password_(password) {}

void IceServer::ProcessStunPacket(RTC::StunPacket *packet, sockaddr_in *remote_address) {
    // Must be a Binding method.
    if (packet->GetMethod() != RTC::StunPacket::Method::BINDING) {
        if (packet->GetClass() == RTC::StunPacket::Class::REQUEST) {
            ELOG_WARN("unknown method %#.3x in STUN Request => 400",
                      static_cast<unsigned int>(packet->GetMethod()));
            ELOG_WARN("unknown method %#.3x in STUN Request => 400",
                      static_cast<unsigned int>(packet->GetMethod()));
            // Reply 400.
            RTC::StunPacket *response = packet->CreateErrorResponse(400);
            response->Serialize(StunSerializeBuffer);
            if (send_callback_) {
                send_callback_((char *) StunSerializeBuffer, response->GetSize(), remote_address);
            }
            delete response;
        } else {
            ELOG_WARN("ignoring STUN Indication or Response with unknown method %#.3x",
                      static_cast<unsigned int>(packet->GetMethod()));
        }
        return;
    }

    // Must use FINGERPRINT (optional for ICE STUN indications).
    if (!packet->HasFingerprint() && packet->GetClass() != RTC::StunPacket::Class::INDICATION) {
        if (packet->GetClass() == RTC::StunPacket::Class::REQUEST) {
            ELOG_WARN("STUN Binding Request without FINGERPRINT => 400");
            // Reply 400.
            RTC::StunPacket *response = packet->CreateErrorResponse(400);
            response->Serialize(StunSerializeBuffer);
            if (send_callback_) {
                send_callback_((char *) StunSerializeBuffer, response->GetSize(), remote_address);
            }
            delete response;
        } else {
            ELOG_WARN("ignoring STUN Binding Response without FINGERPRINT");
        }
        return;
    }

    switch (packet->GetClass()) {
        case RTC::StunPacket::Class::REQUEST: {
            // USERNAME, MESSAGE-INTEGRITY and PRIORITY are required.
            if (!packet->HasMessageIntegrity() || (packet->GetPriority() == 0u) ||
                packet->GetUsername().empty()) {
                ELOG_WARN("mising required attributes in STUN Binding Request => 400");

                // Reply 400.
                RTC::StunPacket *response = packet->CreateErrorResponse(400);
                response->Serialize(StunSerializeBuffer);
                if (send_callback_) {
                    send_callback_((char *) StunSerializeBuffer, response->GetSize(), remote_address);
                }
                delete response;
                return;
            }

            // Check authentication.
            switch (packet->CheckAuthentication(this->username_fragment_, this->password_)) {
                case RTC::StunPacket::Authentication::OK: {
                    if (!this->old_password_.empty()) {
                        ELOG_DEBUG("kNew ICE credentials applied");
                        this->old_username_fragment_.clear();
                        this->old_password_.clear();
                    }
                    break;
                }

                case RTC::StunPacket::Authentication::UNAUTHORIZED: {
                    // We may have changed our username_fragment_ and password_, so check
                    // the old ones.
                    // clang-format off
                    if (!this->old_username_fragment_.empty() &&
                        !this->old_password_.empty() &&
                        packet->CheckAuthentication(this->old_username_fragment_, this->old_password_) ==
                        RTC::StunPacket::Authentication::OK) {
                        ELOG_DEBUG("using old ICE credentials");
                        break;
                    }
                    ELOG_WARN("wrong authentication in STUN Binding Request => 401");
                    // Reply 401.
                    RTC::StunPacket *response = packet->CreateErrorResponse(401);
                    response->Serialize(StunSerializeBuffer);
                    if (send_callback_) {
                        send_callback_((char *) StunSerializeBuffer, response->GetSize(), remote_address);
                    }
                    delete response;
                    return;
                }

                case RTC::StunPacket::Authentication::BAD_REQUEST: {
                    ELOG_WARN("cannot check authentication in STUN Binding Request => 400");
                    // Reply 400.
                    RTC::StunPacket *response = packet->CreateErrorResponse(400);
                    response->Serialize(StunSerializeBuffer);
                    if (send_callback_) {
                        send_callback_((char *) StunSerializeBuffer, response->GetSize(), remote_address);
                    }
                    delete response;
                    return;
                }
            }

#if 0
            // NOTE: Should be rejected with 487, but this makes Chrome happy:
            //   https://bugs.chromium.org/p/webrtc/issues/detail?id=7478
            // The remote peer must be ICE controlling.
            if (packet->GetIceControlled()) {
                MS_WARN_TAG(ice, "peer indicates ICE-CONTROLLED in STUN Binding Request => 487");
                // Reply 487 (Role Conflict).
                RTC::StunPacket *response = packet->CreateErrorResponse(487);
                response->Serialize(StunSerializeBuffer);
                if (send_callback_) {
                    send_callback_((char *) StunSerializeBuffer, response->GetSize(), remote_address);
                }
                delete response;
                return;
            }
#endif

            ELOG_DEBUG("processing STUN Binding Request [Priority:%d, UseCandidate:%s]",
                       static_cast<uint32_t>(packet->GetPriority()),
                       (packet->HasUseCandidate() ? "true" : "false"));
            // Create a success response.
            RTC::StunPacket *response = packet->CreateSuccessResponse();
            // Add XOR-MAPPED-ADDRESS.
            // response->SetXorMappedAddress(tuple->GetRemoteAddress());
            response->SetXorMappedAddress((struct sockaddr *) remote_address);
            // Authenticate the response.
            if (this->old_password_.empty()) {
                response->Authenticate(this->password_);
            } else {
                response->Authenticate(this->old_password_);
            }

            // Send back.
            response->Serialize(StunSerializeBuffer);
            if (send_callback_) {
                send_callback_((char *) StunSerializeBuffer, response->GetSize(), remote_address);
            }
            delete response;
            // Handle the tuple.
            HandleTuple(remote_address, packet->HasUseCandidate());
            break;
        }

        case RTC::StunPacket::Class::INDICATION: {
            ELOG_DEBUG("STUN Binding Indication processed");
            break;
        }

        case RTC::StunPacket::Class::SUCCESS_RESPONSE: {
            ELOG_DEBUG("STUN Binding Success Response processed");
            break;
        }

        case RTC::StunPacket::Class::ERROR_RESPONSE: {
            ELOG_DEBUG("STUN Binding Error Response processed");
            break;
        }
    }
}
void IceServer::HandleTuple(sockaddr_in *remote_address, bool has_use_candidate) {
    remote_address_ = *remote_address;
    if (has_use_candidate) {
        this->state = IceState::kCompleted;
    }
    if (ice_server_completed_callback_) {
        ice_server_completed_callback_();
        ice_server_completed_callback_ = nullptr;
    }
}

const std::string &IceServer::GetUsernameFragment() const { return this->username_fragment_; }

const std::string &IceServer::GetPassword() const { return this->password_; }

inline void IceServer::SetUsernameFragment(const std::string &username_fragment) {
    this->old_username_fragment_ = this->username_fragment_;
    this->username_fragment_ = username_fragment;
}

inline void IceServer::SetPassword(const std::string &password) {
    this->old_password_ = this->password_;
    this->password_ = password;
}

inline IceServer::IceState IceServer::GetState() const { return this->state; }