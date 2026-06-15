#include "user_grpc_service.hpp"

#include "user_service.hpp"

#include <chrono>
#include <exception>
#include <optional>
#include <string>

#include <grpcpp/server_context.h>

#include "base.pb.h"
#include "user.hpp"
#include "user.pb.h"

namespace im::service::user {

namespace {

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

void set_base(im::base::BaseResponse* base,
              im::base::ErrorCode code,
              const std::string& message = {}) {
    base->set_error_code(code);
    base->set_error_message(message);
}

im::base::ErrorCode register_error_code(const std::string& error_code) {
    if (error_code == "DUPLICATE_ACCOUNT") {
        return im::base::ALREADY_EXISTS;
    }
    if (error_code == "EMPTY_ACCOUNT" || error_code == "EMPTY_PASSWORD") {
        return im::base::PARAM_ERROR;
    }
    return im::base::SERVER_ERROR;
}

im::base::ErrorCode login_error_code(const std::string& error_code) {
    if (error_code == "INVALID_ACCOUNT" || error_code == "WRONG_PASSWORD") {
        return im::base::AUTH_FAILED;
    }
    return im::base::SERVER_ERROR;
}

im::base::ErrorCode update_error_code(const std::string& error_code) {
    if (error_code == "EMPTY_UID" || error_code == "EMPTY_NICKNAME") {
        return im::base::PARAM_ERROR;
    }
    if (error_code == "USER_NOT_FOUND") {
        return im::base::NOT_FOUND;
    }
    return im::base::SERVER_ERROR;
}

im::user::Gender to_proto_gender(Gender gender) {
    switch (gender) {
    case Gender::MALE:
        return im::user::MALE;
    case Gender::FEMALE:
        return im::user::FEMALE;
    case Gender::OTHER:
        return im::user::OTHER;
    case Gender::UNKNOWN:
    default:
        return im::user::UNKNOWN;
    }
}

Gender to_service_gender(im::user::Gender gender) {
    switch (gender) {
    case im::user::MALE:
        return Gender::MALE;
    case im::user::FEMALE:
        return Gender::FEMALE;
    case im::user::OTHER:
        return Gender::OTHER;
    case im::user::UNKNOWN:
    default:
        return Gender::UNKNOWN;
    }
}

void fill_user_info(const UserProfile& profile, im::user::UserInfo* user) {
    user->set_uid(profile.uid);
    user->set_account(profile.account);
    user->set_nickname(profile.nickname);
    user->set_avatar(profile.avatar);
    user->set_gender(to_proto_gender(profile.gender));
    user->set_signature(profile.signature);
    user->set_create_time(profile.create_time);
    user->set_last_login(profile.last_login);
    user->set_online(profile.online);
}

}  // namespace

UserGrpcService::UserGrpcService(UserService* user_service)
    : user_service_(user_service) {}

::grpc::Status UserGrpcService::Register(::grpc::ServerContext* /*context*/,
                                         const im::user::RegisterRequest* request,
                                         im::user::RegisterResponse* response) {
    if (!response) {
        return ::grpc::Status::OK;
    }
    if (!user_service_) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, "UserService is not available");
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "Register request is null");
        return ::grpc::Status::OK;
    }

    try {
        RegisterRequest service_request;
        service_request.account = request->account();
        if (service_request.account.empty()) {
            service_request.account = request->phone_number();
        }
        if (service_request.account.empty()) {
            service_request.account = request->email();
        }
        service_request.password = request->password();
        service_request.nickname = request->nickname();
        service_request.now_ms = now_ms();

        const auto result = user_service_->register_user(service_request);
        if (!result.ok) {
            set_base(response->mutable_base(),
                     register_error_code(result.error_code),
                     result.message);
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        fill_user_info(result.profile, response->mutable_user());
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status UserGrpcService::Login(::grpc::ServerContext* /*context*/,
                                      const im::user::LoginRequest* request,
                                      im::user::LoginResponse* response) {
    if (!response) {
        return ::grpc::Status::OK;
    }
    if (!user_service_) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, "UserService is not available");
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "Login request is null");
        return ::grpc::Status::OK;
    }

    try {
        std::string account = request->account();
        if (account.empty()) {
            account = request->phone_number();
        }
        if (account.empty()) {
            account = request->email();
        }
        if (account.empty() || request->password().empty()) {
            set_base(response->mutable_base(), im::base::PARAM_ERROR,
                     "Missing required fields: account, password");
            return ::grpc::Status::OK;
        }

        const auto result = user_service_->login_by_account(account, request->password(), now_ms());
        if (!result.ok) {
            set_base(response->mutable_base(), login_error_code(result.error_code), result.message);
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        fill_user_info(result.profile, response->mutable_user());
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status UserGrpcService::GetUserInfo(::grpc::ServerContext* /*context*/,
                                            const im::user::GetUserInfoRequest* request,
                                            im::user::GetUserInfoResponse* response) {
    if (!response) {
        return ::grpc::Status::OK;
    }
    if (!user_service_) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, "UserService is not available");
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "GetUserInfo request is null");
        return ::grpc::Status::OK;
    }

    try {
        std::optional<UserProfile> profile;
        if (!request->uid().empty()) {
            profile = user_service_->get_profile_by_uid(request->uid());
        } else if (!request->account().empty()) {
            profile = user_service_->get_profile_by_account(request->account());
        } else {
            set_base(response->mutable_base(), im::base::PARAM_ERROR,
                     "Missing required field: uid or account");
            return ::grpc::Status::OK;
        }

        if (!profile.has_value()) {
            set_base(response->mutable_base(), im::base::NOT_FOUND, "User profile not found");
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        fill_user_info(*profile, response->mutable_user());
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status UserGrpcService::SearchUsers(::grpc::ServerContext* /*context*/,
                                            const im::user::SearchUsersRequest* request,
                                            im::user::SearchUsersResponse* response) {
    if (!response) {
        return ::grpc::Status::OK;
    }
    if (!user_service_) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, "UserService is not available");
        return ::grpc::Status::OK;
    }
    if (!request || request->keyword().empty()) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR,
                 "Missing required field: keyword");
        return ::grpc::Status::OK;
    }

    try {
        const auto limit = request->limit() > 0 ? static_cast<std::size_t>(request->limit()) : 20u;
        const auto profiles = user_service_->search_profiles(request->keyword(), limit);
        set_base(response->mutable_base(), im::base::SUCCESS);
        for (const auto& profile : profiles) {
            fill_user_info(profile, response->add_users());
        }
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

::grpc::Status UserGrpcService::UpdateUserInfo(::grpc::ServerContext* /*context*/,
                                               const im::user::UpdateUserInfoRequest* request,
                                               im::user::UpdateUserInfoResponse* response) {
    if (!response) {
        return ::grpc::Status::OK;
    }
    if (!user_service_) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, "UserService is not available");
        return ::grpc::Status::OK;
    }
    if (!request) {
        set_base(response->mutable_base(), im::base::PARAM_ERROR, "UpdateUserInfo request is null");
        return ::grpc::Status::OK;
    }

    try {
        UpdateProfileRequest service_request;
        service_request.uid = request->header().from_uid();
        if (service_request.uid.empty()) {
            service_request.uid = request->user().uid();
        }
        service_request.nickname = request->user().nickname();
        service_request.avatar = request->user().avatar();
        service_request.gender = to_service_gender(request->user().gender());
        service_request.signature = request->user().signature();

        const auto result = user_service_->update_profile(service_request);
        if (!result.ok) {
            set_base(response->mutable_base(), update_error_code(result.error_code), result.message);
            return ::grpc::Status::OK;
        }

        set_base(response->mutable_base(), im::base::SUCCESS);
        fill_user_info(result.profile, response->mutable_user());
        return ::grpc::Status::OK;
    } catch (const std::exception& e) {
        set_base(response->mutable_base(), im::base::SERVER_ERROR, e.what());
        return ::grpc::Status::OK;
    }
}

}  // namespace im::service::user
