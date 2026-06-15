#include "remote_user_client.hpp"

#include <chrono>
#include <utility>

#include <grpcpp/channel.h>
#include <grpcpp/client_context.h>
#include <grpcpp/create_channel.h>
#include <grpcpp/security/credentials.h>

#include "base.pb.h"
#include "user.hpp"
#include "../../common/utils/log_manager.hpp"

namespace im::gateway {

namespace {

class GrpcUserRpcClient final : public UserRpcClient {
public:
    explicit GrpcUserRpcClient(const std::string& endpoint)
        : stub_(im::user::UserService::NewStub(
              ::grpc::CreateChannel(endpoint, ::grpc::InsecureChannelCredentials()))) {}

    ::grpc::Status register_user(::grpc::ClientContext* context,
                                 const im::user::RegisterRequest& request,
                                 im::user::RegisterResponse* response) override {
        return stub_->Register(context, request, response);
    }

    ::grpc::Status login(::grpc::ClientContext* context,
                         const im::user::LoginRequest& request,
                         im::user::LoginResponse* response) override {
        return stub_->Login(context, request, response);
    }

    ::grpc::Status get_user_info(::grpc::ClientContext* context,
                                 const im::user::GetUserInfoRequest& request,
                                 im::user::GetUserInfoResponse* response) override {
        return stub_->GetUserInfo(context, request, response);
    }

    ::grpc::Status search_users(::grpc::ClientContext* context,
                                const im::user::SearchUsersRequest& request,
                                im::user::SearchUsersResponse* response) override {
        return stub_->SearchUsers(context, request, response);
    }

    ::grpc::Status update_user_info(::grpc::ClientContext* context,
                                    const im::user::UpdateUserInfoRequest& request,
                                    im::user::UpdateUserInfoResponse* response) override {
        return stub_->UpdateUserInfo(context, request, response);
    }

private:
    std::unique_ptr<im::user::UserService::Stub> stub_;
};

im::service::user::Gender to_service_gender(im::user::Gender gender) {
    switch (gender) {
    case im::user::MALE:
        return im::service::user::Gender::MALE;
    case im::user::FEMALE:
        return im::service::user::Gender::FEMALE;
    case im::user::OTHER:
        return im::service::user::Gender::OTHER;
    case im::user::UNKNOWN:
    default:
        return im::service::user::Gender::UNKNOWN;
    }
}

im::service::user::UserProfile to_profile(const im::user::UserInfo& user) {
    im::service::user::UserProfile profile;
    profile.uid = user.uid();
    profile.account = user.account();
    profile.nickname = user.nickname();
    profile.avatar = user.avatar();
    profile.gender = to_service_gender(user.gender());
    profile.signature = user.signature();
    profile.create_time = user.create_time();
    profile.last_login = user.last_login();
    profile.online = user.online();
    return profile;
}

std::string register_error_code(im::base::ErrorCode code) {
    switch (code) {
    case im::base::ALREADY_EXISTS:
        return "DUPLICATE_ACCOUNT";
    case im::base::PARAM_ERROR:
        return "PARAM_ERROR";
    default:
        return "REMOTE_USER_ERROR";
    }
}

std::string login_error_code(im::base::ErrorCode code) {
    switch (code) {
    case im::base::AUTH_FAILED:
        return "INVALID_ACCOUNT";
    case im::base::PARAM_ERROR:
        return "PARAM_ERROR";
    default:
        return "REMOTE_USER_ERROR";
    }
}

std::string update_error_code(im::base::ErrorCode code) {
    switch (code) {
    case im::base::PARAM_ERROR:
        return "PARAM_ERROR";
    case im::base::NOT_FOUND:
        return "USER_NOT_FOUND";
    default:
        return "REMOTE_USER_ERROR";
    }
}

}  // namespace

RemoteUserClient::RemoteUserClient(const std::string& endpoint,
                                   std::chrono::milliseconds timeout)
    : RemoteUserClient(std::make_unique<GrpcUserRpcClient>(endpoint), timeout) {}

RemoteUserClient::RemoteUserClient(std::unique_ptr<UserRpcClient> client,
                                   std::chrono::milliseconds timeout)
    : client_(std::move(client))
    , timeout_(timeout) {
    im::utils::LogManager::SetLogToFile("remote_user_client",
                                        "logs/remote_user_client.log");
    logger_ = im::utils::LogManager::GetLogger("remote_user_client");
}

im::service::user::RegisterResult RemoteUserClient::register_user(
    const im::service::user::RegisterRequest& request) {
    im::service::user::RegisterResult result;
    if (!client_) {
        result.error_code = "REMOTE_USER_UNAVAILABLE";
        result.message = "Remote User client is not configured";
        return result;
    }

    im::user::RegisterRequest rpc_request;
    rpc_request.set_account(request.account);
    rpc_request.set_password(request.password);
    rpc_request.set_nickname(request.nickname);

    im::user::RegisterResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->register_user(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_USER_UNAVAILABLE";
        result.message = status.error_message();
        logger_->warn("Remote user register RPC failed for {}: {}",
                      request.account, status.error_message());
        return result;
    }

    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = register_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }

    result.ok = true;
    result.profile = to_profile(rpc_response.user());
    return result;
}

im::service::user::LoginResult RemoteUserClient::login_by_account(
    const std::string& account,
    const std::string& password,
    int64_t /*now_ms*/) {
    im::service::user::LoginResult result;
    if (!client_) {
        result.error_code = "REMOTE_USER_UNAVAILABLE";
        result.message = "Remote User client is not configured";
        return result;
    }

    im::user::LoginRequest rpc_request;
    rpc_request.set_account(account);
    rpc_request.set_password(password);

    im::user::LoginResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->login(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_USER_UNAVAILABLE";
        result.message = status.error_message();
        logger_->warn("Remote user login RPC failed for {}: {}",
                      account, status.error_message());
        return result;
    }

    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = login_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }

    result.ok = true;
    result.profile = to_profile(rpc_response.user());
    return result;
}

std::optional<im::service::user::UserProfile> RemoteUserClient::get_profile_by_uid(
    const std::string& uid) {
    if (!client_) {
        logger_->warn("Remote user profile lookup skipped: RPC client is not configured");
        return std::nullopt;
    }

    im::user::GetUserInfoRequest rpc_request;
    rpc_request.set_uid(uid);

    im::user::GetUserInfoResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->get_user_info(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        logger_->warn("Remote user profile RPC failed for {}: {}",
                      uid, status.error_message());
        return std::nullopt;
    }

    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        return std::nullopt;
    }

    return to_profile(rpc_response.user());
}

std::optional<im::service::user::UserProfile> RemoteUserClient::get_profile_by_account(
    const std::string& account) {
    if (!client_) {
        logger_->warn("Remote user profile lookup skipped: RPC client is not configured");
        return std::nullopt;
    }

    im::user::GetUserInfoRequest rpc_request;
    rpc_request.set_account(account);

    im::user::GetUserInfoResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->get_user_info(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        logger_->warn("Remote user profile RPC failed for account {}: {}",
                      account, status.error_message());
        return std::nullopt;
    }

    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        return std::nullopt;
    }

    return to_profile(rpc_response.user());
}

std::vector<im::service::user::UserProfile> RemoteUserClient::search_profiles(
    const std::string& keyword,
    std::size_t limit) {
    std::vector<im::service::user::UserProfile> profiles;
    if (!client_ || keyword.empty() || limit == 0) {
        return profiles;
    }

    im::user::SearchUsersRequest rpc_request;
    rpc_request.set_keyword(keyword);
    rpc_request.set_limit(static_cast<int32_t>(limit));

    im::user::SearchUsersResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->search_users(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        logger_->warn("Remote user search RPC failed for {}: {}",
                      keyword, status.error_message());
        return profiles;
    }

    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        return profiles;
    }

    profiles.reserve(static_cast<std::size_t>(rpc_response.users_size()));
    for (const auto& user : rpc_response.users()) {
        profiles.push_back(to_profile(user));
    }
    return profiles;
}

im::service::user::UpdateProfileResult RemoteUserClient::update_profile(
    const im::service::user::UpdateProfileRequest& request) {
    im::service::user::UpdateProfileResult result;
    if (!client_) {
        result.error_code = "REMOTE_USER_UNAVAILABLE";
        result.message = "Remote User client is not configured";
        return result;
    }

    im::user::UpdateUserInfoRequest rpc_request;
    rpc_request.mutable_header()->set_from_uid(request.uid);
    auto* user = rpc_request.mutable_user();
    user->set_nickname(request.nickname);
    user->set_avatar(request.avatar);
    user->set_gender([&]() {
        switch (request.gender) {
        case im::service::user::Gender::MALE:
            return im::user::MALE;
        case im::service::user::Gender::FEMALE:
            return im::user::FEMALE;
        case im::service::user::Gender::OTHER:
            return im::user::OTHER;
        case im::service::user::Gender::UNKNOWN:
        default:
            return im::user::UNKNOWN;
        }
    }());
    user->set_signature(request.signature);

    im::user::UpdateUserInfoResponse rpc_response;
    ::grpc::ClientContext context;
    apply_deadline(context);

    auto status = client_->update_user_info(&context, rpc_request, &rpc_response);
    if (!status.ok()) {
        result.error_code = "REMOTE_USER_UNAVAILABLE";
        result.message = status.error_message();
        logger_->warn("Remote user update RPC failed for {}: {}",
                      request.uid, status.error_message());
        return result;
    }

    if (rpc_response.base().error_code() != im::base::SUCCESS) {
        result.error_code = update_error_code(rpc_response.base().error_code());
        result.message = rpc_response.base().error_message();
        return result;
    }

    result.ok = true;
    result.profile = to_profile(rpc_response.user());
    return result;
}

void RemoteUserClient::apply_deadline(::grpc::ClientContext& context) const {
    if (timeout_.count() > 0) {
        context.set_deadline(std::chrono::system_clock::now() + timeout_);
    }
}

}  // namespace im::gateway
