#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <gateway/http/remote_user_client.hpp>
#include <user.hpp>

#include "base.pb.h"
#include "user.pb.h"

namespace {

using im::gateway::RemoteUserClient;
using im::gateway::UserRpcClient;

class FakeUserRpcClient final : public UserRpcClient {
public:
    ::grpc::Status register_user(::grpc::ClientContext* /*context*/,
                                 const im::user::RegisterRequest& request,
                                 im::user::RegisterResponse* response) override {
        register_requests.push_back(request);
        response->mutable_base()->set_error_code(register_base_code);
        response->mutable_base()->set_error_message(register_base_message);
        if (register_base_code == im::base::SUCCESS) {
            fill_user(response->mutable_user(), request.account(), "registered-uid");
        }
        return register_status;
    }

    ::grpc::Status login(::grpc::ClientContext* /*context*/,
                         const im::user::LoginRequest& request,
                         im::user::LoginResponse* response) override {
        login_requests.push_back(request);
        response->mutable_base()->set_error_code(login_base_code);
        response->mutable_base()->set_error_message(login_base_message);
        if (login_base_code == im::base::SUCCESS) {
            fill_user(response->mutable_user(), request.account(), "login-uid");
        }
        return login_status;
    }

    ::grpc::Status get_user_info(::grpc::ClientContext* /*context*/,
                                 const im::user::GetUserInfoRequest& request,
                                 im::user::GetUserInfoResponse* response) override {
        profile_requests.push_back(request);
        response->mutable_base()->set_error_code(profile_base_code);
        response->mutable_base()->set_error_message(profile_base_message);
        if (profile_base_code == im::base::SUCCESS) {
            fill_user(response->mutable_user(), "profile-account", request.uid());
        }
        return profile_status;
    }

    static void fill_user(im::user::UserInfo* user,
                          const std::string& account,
                          const std::string& uid) {
        user->set_uid(uid);
        user->set_account(account);
        user->set_nickname("Remote User");
        user->set_gender(im::user::MALE);
        user->set_create_time(1000);
        user->set_last_login(2000);
    }

    ::grpc::Status register_status = ::grpc::Status::OK;
    ::grpc::Status login_status = ::grpc::Status::OK;
    ::grpc::Status profile_status = ::grpc::Status::OK;
    im::base::ErrorCode register_base_code = im::base::SUCCESS;
    im::base::ErrorCode login_base_code = im::base::SUCCESS;
    im::base::ErrorCode profile_base_code = im::base::SUCCESS;
    std::string register_base_message;
    std::string login_base_message;
    std::string profile_base_message;
    std::vector<im::user::RegisterRequest> register_requests;
    std::vector<im::user::LoginRequest> login_requests;
    std::vector<im::user::GetUserInfoRequest> profile_requests;
};

}  // namespace

TEST(RemoteUserClientTest, RegisterMapsSuccessProfile) {
    auto fake = std::make_unique<FakeUserRpcClient>();
    auto* raw = fake.get();
    RemoteUserClient client(std::move(fake));

    im::service::user::RegisterRequest request;
    request.account = "remote-user-register";
    request.password = "pass";
    request.nickname = "Remote Nick";

    auto result = client.register_user(request);

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(raw->register_requests.size(), 1u);
    EXPECT_EQ(raw->register_requests[0].account(), "remote-user-register");
    EXPECT_EQ(raw->register_requests[0].password(), "pass");
    EXPECT_EQ(result.profile.uid, "registered-uid");
    EXPECT_EQ(result.profile.account, "remote-user-register");
    EXPECT_EQ(result.profile.gender, im::service::user::Gender::MALE);
}

TEST(RemoteUserClientTest, RegisterMapsDuplicateAccount) {
    auto fake = std::make_unique<FakeUserRpcClient>();
    fake->register_base_code = im::base::ALREADY_EXISTS;
    fake->register_base_message = "duplicate";
    RemoteUserClient client(std::move(fake));

    im::service::user::RegisterRequest request;
    request.account = "remote-user-duplicate";
    request.password = "pass";

    auto result = client.register_user(request);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "DUPLICATE_ACCOUNT");
    EXPECT_EQ(result.message, "duplicate");
}

TEST(RemoteUserClientTest, LoginMapsAuthFailureToExistingGatewaySemantics) {
    auto fake = std::make_unique<FakeUserRpcClient>();
    fake->login_base_code = im::base::AUTH_FAILED;
    fake->login_base_message = "bad credentials";
    RemoteUserClient client(std::move(fake));

    auto result = client.login_by_account("remote-user-login", "wrong", 1234);

    EXPECT_FALSE(result.ok);
    EXPECT_EQ(result.error_code, "INVALID_ACCOUNT");
    EXPECT_EQ(result.message, "bad credentials");
}

TEST(RemoteUserClientTest, GetProfileReturnsNulloptOnNotFoundOrRpcFailure) {
    auto fake = std::make_unique<FakeUserRpcClient>();
    fake->profile_base_code = im::base::NOT_FOUND;
    RemoteUserClient not_found_client(std::move(fake));

    EXPECT_FALSE(not_found_client.get_profile_by_uid("missing").has_value());

    auto failing = std::make_unique<FakeUserRpcClient>();
    failing->profile_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteUserClient failing_client(std::move(failing));

    EXPECT_FALSE(failing_client.get_profile_by_uid("down").has_value());
}
