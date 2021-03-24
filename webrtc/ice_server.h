#pragma once

#include <functional>
#include <memory>

#include "logger.h"
#include "stun_packet.h"

typedef std::function<void(char *buf, size_t len, struct sockaddr_in *remote_address)> UdpSendCallback;

class IceServer {
public:
    enum class IceState { kNew = 1, kConnect, kCompleted, kDisconnected };
    typedef std::shared_ptr<IceServer> Ptr;
    IceServer();
    IceServer(const std::string &username_fragment, const std::string &password);
    const std::string &GetUsernameFragment() const;
    const std::string &GetPassword() const;
    void SetUsernameFragment(const std::string &username_fragment);
    void SetPassword(const std::string &password);
    IceState GetState() const;
    void ProcessStunPacket(RTC::StunPacket *packet, struct sockaddr_in *remote_address);
    void HandleTuple(struct sockaddr_in *remote_address, bool has_use_candidate);
    ~IceServer();
    void SetSendCB(UdpSendCallback send_cb) { send_callback_ = send_cb; }
    void SetIceServerCompletedCB(std::function<void()> cb) { ice_server_completed_callback_ = cb; };
    struct sockaddr_in *GetSelectAddr() {
        return &remote_address_;
    }

private:
    UdpSendCallback send_callback_;
    std::function<void()> ice_server_completed_callback_;
    std::string username_fragment_;
    std::string password_;
    std::string old_username_fragment_;
    std::string old_password_;
    IceState state{IceState::kNew};
    struct sockaddr_in remote_address_;
};
