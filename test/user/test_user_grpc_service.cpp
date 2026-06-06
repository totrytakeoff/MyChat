#include <gtest/gtest.h>

#include <memory>
#include <string>

#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>

#include <password_hasher.hpp>
#include <user_grpc_service.hpp>
#include <user_service.hpp>

#include "base.pb.h"
#include "user.pb.h"

#include "../support/postgres_schema.hpp"

namespace {

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";

class UserGrpcServiceTest : public ::testing::Test {
protected:
    void SetUp() override {
        db_ = std::make_shared<odb::pgsql::database>(kConnStr);
        im::test::EnsureCoreSchema(*db_);
        cleanup();
        user_service_ = std::make_unique<im::service::user::UserService>(
            db_, std::make_unique<im::service::user::PasswordHasher>());
        grpc_service_ = std::make_unique<im::service::user::UserGrpcService>(
            user_service_.get());
    }

    void TearDown() override {
        grpc_service_.reset();
        user_service_.reset();
        cleanup();
    }

    void cleanup() {
        odb::transaction t(db_->begin());
        db_->execute(R"(DELETE FROM "im_users" WHERE "account" LIKE 'grpc-user-test-%')");
        t.commit();
    }

    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<im::service::user::UserService> user_service_;
    std::unique_ptr<im::service::user::UserGrpcService> grpc_service_;
};

}  // namespace

TEST_F(UserGrpcServiceTest, RegisterDelegatesAndMapsProfile) {
    im::user::RegisterRequest request;
    im::user::RegisterResponse response;
    request.set_account("grpc-user-test-register");
    request.set_password("securePass123");
    request.set_nickname("Grpc User");

    auto status = grpc_service_->Register(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(response.has_user());
    EXPECT_FALSE(response.user().uid().empty());
    EXPECT_EQ(response.user().account(), "grpc-user-test-register");
    EXPECT_EQ(response.user().nickname(), "Grpc User");
}

TEST_F(UserGrpcServiceTest, RegisterRejectsDuplicateAccount) {
    im::user::RegisterRequest request;
    im::user::RegisterResponse first_response;
    im::user::RegisterResponse second_response;
    request.set_account("grpc-user-test-duplicate");
    request.set_password("securePass123");

    ASSERT_TRUE(grpc_service_->Register(nullptr, &request, &first_response).ok());
    ASSERT_EQ(first_response.base().error_code(), im::base::SUCCESS);

    auto status = grpc_service_->Register(nullptr, &request, &second_response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(second_response.base().error_code(), im::base::ALREADY_EXISTS);
}

TEST_F(UserGrpcServiceTest, LoginDelegatesAndMapsAuthFailures) {
    im::user::RegisterRequest register_request;
    im::user::RegisterResponse register_response;
    register_request.set_account("grpc-user-test-login");
    register_request.set_password("correctPass");
    ASSERT_TRUE(grpc_service_->Register(nullptr, &register_request, &register_response).ok());
    ASSERT_EQ(register_response.base().error_code(), im::base::SUCCESS);

    im::user::LoginRequest login_request;
    im::user::LoginResponse login_response;
    login_request.set_account("grpc-user-test-login");
    login_request.set_password("correctPass");

    auto status = grpc_service_->Login(nullptr, &login_request, &login_response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(login_response.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(login_response.has_user());
    EXPECT_EQ(login_response.user().account(), "grpc-user-test-login");

    im::user::LoginResponse failed_response;
    login_request.set_password("wrongPass");
    status = grpc_service_->Login(nullptr, &login_request, &failed_response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(failed_response.base().error_code(), im::base::AUTH_FAILED);
}

TEST_F(UserGrpcServiceTest, GetUserInfoSupportsUidAndAccountLookup) {
    im::user::RegisterRequest register_request;
    im::user::RegisterResponse register_response;
    register_request.set_account("grpc-user-test-profile");
    register_request.set_password("profilePass");
    register_request.set_nickname("Profile Name");
    ASSERT_TRUE(grpc_service_->Register(nullptr, &register_request, &register_response).ok());
    ASSERT_EQ(register_response.base().error_code(), im::base::SUCCESS);

    im::user::GetUserInfoRequest uid_request;
    im::user::GetUserInfoResponse uid_response;
    uid_request.set_uid(register_response.user().uid());

    auto status = grpc_service_->GetUserInfo(nullptr, &uid_request, &uid_response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(uid_response.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(uid_response.has_user());
    EXPECT_EQ(uid_response.user().account(), "grpc-user-test-profile");
    EXPECT_EQ(uid_response.user().nickname(), "Profile Name");

    im::user::GetUserInfoRequest account_request;
    im::user::GetUserInfoResponse account_response;
    account_request.set_account("grpc-user-test-profile");

    status = grpc_service_->GetUserInfo(nullptr, &account_request, &account_response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(account_response.base().error_code(), im::base::SUCCESS);
    EXPECT_EQ(account_response.user().uid(), register_response.user().uid());

    im::user::GetUserInfoRequest missing_request;
    im::user::GetUserInfoResponse missing_response;
    missing_request.set_account("grpc-user-test-missing");

    status = grpc_service_->GetUserInfo(nullptr, &missing_request, &missing_response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(missing_response.base().error_code(), im::base::NOT_FOUND);
}

TEST(UserGrpcServiceStandaloneTest, MissingCoreServiceReturnsServerError) {
    im::service::user::UserGrpcService service(nullptr);
    im::user::RegisterRequest request;
    im::user::RegisterResponse response;
    request.set_account("grpc-user-test-no-service");
    request.set_password("pass");

    auto status = service.Register(nullptr, &request, &response);

    EXPECT_TRUE(status.ok());
    EXPECT_EQ(response.base().error_code(), im::base::SERVER_ERROR);
}
