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
            fill_user(response->mutable_user(),
                      request.account().empty() ? "profile-account" : request.account(),
                      request.uid().empty() ? "profile-uid" : request.uid());
        }
        return profile_status;
    }

    ::grpc::Status search_users(::grpc::ClientContext* /*context*/,
                                const im::user::SearchUsersRequest& request,
                                im::user::SearchUsersResponse* response) override {
        search_requests.push_back(request);
        response->mutable_base()->set_error_code(search_base_code);
        response->mutable_base()->set_error_message(search_base_message);
        if (search_base_code == im::base::SUCCESS) {
            fill_user(response->add_users(), "remote-search-one", "search-uid-1");
            fill_user(response->add_users(), "remote-search-two", "search-uid-2");
        }
        return search_status;
    }

    ::grpc::Status update_user_info(::grpc::ClientContext* /*context*/,
                                    const im::user::UpdateUserInfoRequest& request,
                                    im::user::UpdateUserInfoResponse* response) override {
        update_requests.push_back(request);
        response->mutable_base()->set_error_code(update_base_code);
        response->mutable_base()->set_error_message(update_base_message);
        if (update_base_code == im::base::SUCCESS) {
            fill_user(response->mutable_user(), "updated-account", request.header().from_uid());
            response->mutable_user()->set_nickname(request.user().nickname());
            response->mutable_user()->set_avatar(request.user().avatar());
            response->mutable_user()->set_gender(request.user().gender());
            response->mutable_user()->set_signature(request.user().signature());
        }
        return update_status;
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
    ::grpc::Status search_status = ::grpc::Status::OK;
    ::grpc::Status update_status = ::grpc::Status::OK;
    im::base::ErrorCode register_base_code = im::base::SUCCESS;
    im::base::ErrorCode login_base_code = im::base::SUCCESS;
    im::base::ErrorCode profile_base_code = im::base::SUCCESS;
    im::base::ErrorCode search_base_code = im::base::SUCCESS;
    im::base::ErrorCode update_base_code = im::base::SUCCESS;
    std::string register_base_message;
    std::string login_base_message;
    std::string profile_base_message;
    std::string search_base_message;
    std::string update_base_message;
    std::vector<im::user::RegisterRequest> register_requests;
    std::vector<im::user::LoginRequest> login_requests;
    std::vector<im::user::GetUserInfoRequest> profile_requests;
    std::vector<im::user::SearchUsersRequest> search_requests;
    std::vector<im::user::UpdateUserInfoRequest> update_requests;
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

TEST(RemoteUserClientTest, GetProfileByAccountSendsAccountLookup) {
    auto fake = std::make_unique<FakeUserRpcClient>();
    auto* raw = fake.get();
    RemoteUserClient client(std::move(fake));

    auto profile = client.get_profile_by_account("remote-profile-account");

    ASSERT_TRUE(profile.has_value());
    ASSERT_EQ(raw->profile_requests.size(), 1u);
    EXPECT_EQ(raw->profile_requests[0].account(), "remote-profile-account");
    EXPECT_TRUE(raw->profile_requests[0].uid().empty());
    EXPECT_EQ(profile->account, "remote-profile-account");
    EXPECT_EQ(profile->uid, "profile-uid");
}

TEST(RemoteUserClientTest, SearchProfilesMapsRemoteUsers) {
    auto fake = std::make_unique<FakeUserRpcClient>();
    auto* raw = fake.get();
    RemoteUserClient client(std::move(fake));

    auto profiles = client.search_profiles("remote", 10);

    ASSERT_EQ(raw->search_requests.size(), 1u);
    EXPECT_EQ(raw->search_requests[0].keyword(), "remote");
    EXPECT_EQ(raw->search_requests[0].limit(), 10);
    ASSERT_EQ(profiles.size(), 2u);
    EXPECT_EQ(profiles[0].account, "remote-search-one");
    EXPECT_EQ(profiles[1].uid, "search-uid-2");
}

TEST(RemoteUserClientTest, UpdateProfileMapsEditableFields) {
    auto fake = std::make_unique<FakeUserRpcClient>();
    auto* raw = fake.get();
    RemoteUserClient client(std::move(fake));

    im::service::user::UpdateProfileRequest request;
    request.uid = "remote-update-uid";
    request.nickname = "新的昵称";
    request.avatar = "https://example.test/avatar.png";
    request.gender = im::service::user::Gender::FEMALE;
    request.signature = "新的签名";

    auto result = client.update_profile(request);

    ASSERT_TRUE(result.ok);
    ASSERT_EQ(raw->update_requests.size(), 1u);
    EXPECT_EQ(raw->update_requests[0].header().from_uid(), "remote-update-uid");
    EXPECT_EQ(raw->update_requests[0].user().nickname(), "新的昵称");
    EXPECT_EQ(raw->update_requests[0].user().avatar(), "https://example.test/avatar.png");
    EXPECT_EQ(raw->update_requests[0].user().gender(), im::user::FEMALE);
    EXPECT_EQ(raw->update_requests[0].user().signature(), "新的签名");
    EXPECT_EQ(result.profile.uid, "remote-update-uid");
    EXPECT_EQ(result.profile.nickname, "新的昵称");
    EXPECT_EQ(result.profile.gender, im::service::user::Gender::FEMALE);
}
