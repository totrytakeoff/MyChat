#ifndef GATEWAY_REMOTE_PUSH_NOTIFIER_HPP
#define GATEWAY_REMOTE_PUSH_NOTIFIER_HPP

#include <chrono>
#include <cstdint>
#include <memory>
#include <string>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "../../common/proto/push.grpc.pb.h"
#include "../../services/push/push_notifier.hpp"

namespace grpc {
class ClientContext;
}

namespace im::gateway {

class PushRpcClient {
public:
    virtual ~PushRpcClient() = default;

    virtual ::grpc::Status notify_user(
        ::grpc::ClientContext* context,
        const im::push::NotifyUserRequest& request,
        im::push::NotifyUserResponse* response) = 0;
};

class RemotePushNotifier final : public im::service::push::PushNotifier {
public:
    explicit RemotePushNotifier(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemotePushNotifier(
        std::unique_ptr<PushRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    void notify_user(const std::string& receiver_uid,
                     uint64_t msg_id,
                     const std::string& content) override;

private:
    std::unique_ptr<PushRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

} // namespace im::gateway

#endif // GATEWAY_REMOTE_PUSH_NOTIFIER_HPP
