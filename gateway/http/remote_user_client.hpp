#ifndef GATEWAY_HTTP_REMOTE_USER_CLIENT_HPP
#define GATEWAY_HTTP_REMOTE_USER_CLIENT_HPP

#include <chrono>
#include <cstddef>
#include <memory>
#include <optional>
#include <string>
#include <vector>

#include <grpcpp/support/status.h>
#include <spdlog/logger.h>

#include "user.grpc.pb.h"
#include "user_client.hpp"

namespace grpc {
class ClientContext;
}

namespace im::gateway {

class UserRpcClient {
public:
    virtual ~UserRpcClient() = default;

    virtual ::grpc::Status register_user(
        ::grpc::ClientContext* context,
        const im::user::RegisterRequest& request,
        im::user::RegisterResponse* response) = 0;

    virtual ::grpc::Status login(
        ::grpc::ClientContext* context,
        const im::user::LoginRequest& request,
        im::user::LoginResponse* response) = 0;

    virtual ::grpc::Status get_user_info(
        ::grpc::ClientContext* context,
        const im::user::GetUserInfoRequest& request,
        im::user::GetUserInfoResponse* response) = 0;

    virtual ::grpc::Status search_users(
        ::grpc::ClientContext* context,
        const im::user::SearchUsersRequest& request,
        im::user::SearchUsersResponse* response) = 0;

    virtual ::grpc::Status update_user_info(
        ::grpc::ClientContext* context,
        const im::user::UpdateUserInfoRequest& request,
        im::user::UpdateUserInfoResponse* response) = 0;
};

class RemoteUserClient final : public UserClient {
public:
    explicit RemoteUserClient(
        const std::string& endpoint,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    RemoteUserClient(
        std::unique_ptr<UserRpcClient> client,
        std::chrono::milliseconds timeout = std::chrono::milliseconds(200));

    im::service::user::RegisterResult register_user(
        const im::service::user::RegisterRequest& request) override;

    im::service::user::LoginResult login_by_account(
        const std::string& account,
        const std::string& password,
        int64_t now_ms) override;

    std::optional<im::service::user::UserProfile> get_profile_by_uid(
        const std::string& uid) override;

    std::optional<im::service::user::UserProfile> get_profile_by_account(
        const std::string& account) override;

    std::vector<im::service::user::UserProfile> search_profiles(
        const std::string& keyword,
        std::size_t limit) override;

    im::service::user::UpdateProfileResult update_profile(
        const im::service::user::UpdateProfileRequest& request) override;

private:
    void apply_deadline(::grpc::ClientContext& context) const;

    std::unique_ptr<UserRpcClient> client_;
    std::chrono::milliseconds timeout_;
    std::shared_ptr<spdlog::logger> logger_;
};

}  // namespace im::gateway

#endif  // GATEWAY_HTTP_REMOTE_USER_CLIENT_HPP
