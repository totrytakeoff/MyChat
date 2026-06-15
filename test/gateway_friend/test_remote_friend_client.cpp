#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <gateway/http/remote_friend_client.hpp>
#include <friend.hpp>

#include "base.pb.h"
#include "friend.pb.h"

namespace {

using im::gateway::FriendRpcClient;
using im::gateway::RemoteFriendClient;

class FakeFriendRpcClient final : public FriendRpcClient {
public:
    ::grpc::Status send_request(::grpc::ClientContext* /*context*/,
                                const im::friend_::AddFriendRequest& request,
                                im::friend_::AddFriendResponse* response) override {
        send_requests.push_back(request);
        response->mutable_base()->set_error_code(send_base_code);
        response->mutable_base()->set_error_message(send_base_message);
        if (send_base_code == im::base::SUCCESS) {
            fill_request(response->mutable_request(), "42", request.header().from_uid(),
                         request.to_uid(), "Requester Nick", im::friend_::PENDING);
        }
        return send_status;
    }

    ::grpc::Status respond_to_request(
        ::grpc::ClientContext* /*context*/,
        const im::friend_::HandleFriendRequest& request,
        im::friend_::HandleFriendResponse* response) override {
        respond_requests.push_back(request);
        response->mutable_base()->set_error_code(respond_base_code);
        response->mutable_base()->set_error_message(respond_base_message);
        return respond_status;
    }

    ::grpc::Status get_friends(::grpc::ClientContext* /*context*/,
                               const im::friend_::GetFriendListRequest& request,
                               im::friend_::GetFriendListResponse* response) override {
        friends_requests.push_back(request);
        response->mutable_base()->set_error_code(friends_base_code);
        if (friends_base_code == im::base::SUCCESS) {
            auto* info = response->add_friends();
            info->set_friend_id(43);
            info->set_uid("friend-uid");
            info->set_nickname("Friend Nick");
            info->set_status(im::friend_::ACCEPTED);
            info->set_add_time(1234);
        }
        return friends_status;
    }

    ::grpc::Status get_pending_requests(
        ::grpc::ClientContext* /*context*/,
        const im::friend_::GetFriendRequestsRequest& request,
        im::friend_::GetFriendRequestsResponse* response) override {
        pending_requests.push_back(request);
        response->mutable_base()->set_error_code(pending_base_code);
        if (pending_base_code == im::base::SUCCESS) {
            fill_request(response->add_requests(), "44", "requester-uid",
                         request.header().from_uid(), "Requester Nick", im::friend_::PENDING);
        }
        return pending_status;
    }

    static void fill_request(im::friend_::FriendRequest* request,
                             const std::string& id,
                             const std::string& from_uid,
                             const std::string& to_uid,
                             const std::string& nickname,
                             im::friend_::FriendRequestStatus status) {
        request->set_request_id(id);
        request->set_friend_id(std::stoull(id));
        request->set_from_uid(from_uid);
        request->set_to_uid(to_uid);
        request->set_nickname(nickname);
        request->set_status(status);
        request->set_create_time(1234);
    }

    ::grpc::Status send_status = ::grpc::Status::OK;
    ::grpc::Status respond_status = ::grpc::Status::OK;
    ::grpc::Status friends_status = ::grpc::Status::OK;
    ::grpc::Status pending_status = ::grpc::Status::OK;
    im::base::ErrorCode send_base_code = im::base::SUCCESS;
    im::base::ErrorCode respond_base_code = im::base::SUCCESS;
    im::base::ErrorCode friends_base_code = im::base::SUCCESS;
    im::base::ErrorCode pending_base_code = im::base::SUCCESS;
    std::string send_base_message;
    std::string respond_base_message;
    std::vector<im::friend_::AddFriendRequest> send_requests;
    std::vector<im::friend_::HandleFriendRequest> respond_requests;
    std::vector<im::friend_::GetFriendListRequest> friends_requests;
    std::vector<im::friend_::GetFriendRequestsRequest> pending_requests;
};

}  // namespace

TEST(RemoteFriendClientTest, SendMapsSuccess) {
    auto fake = std::make_unique<FakeFriendRpcClient>();
    auto* raw = fake.get();
    RemoteFriendClient client(std::move(fake));

    im::service::friend_::FriendRequest request;
    request.requester_uid = "sender";
    request.target_uid = "target";
    request.now_ms = 1234;

    auto result = client.send_request(request);

    ASSERT_TRUE(result.ok);
    EXPECT_EQ(result.message, "Friend request sent");
    ASSERT_EQ(raw->send_requests.size(), 1u);
    EXPECT_EQ(raw->send_requests[0].header().from_uid(), "sender");
    EXPECT_EQ(raw->send_requests[0].to_uid(), "target");
}

TEST(RemoteFriendClientTest, SendMapsBusinessAndRpcFailures) {
    auto already = std::make_unique<FakeFriendRpcClient>();
    already->send_base_code = im::base::ALREADY_EXISTS;
    already->send_base_message = "exists";
    RemoteFriendClient already_client(std::move(already));

    auto already_result = already_client.send_request({});
    EXPECT_FALSE(already_result.ok);
    EXPECT_EQ(already_result.error_code, "ALREADY_EXISTS");
    EXPECT_EQ(already_result.message, "exists");

    auto failing = std::make_unique<FakeFriendRpcClient>();
    failing->send_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    RemoteFriendClient failing_client(std::move(failing));

    auto rpc_result = failing_client.send_request({});
    EXPECT_FALSE(rpc_result.ok);
    EXPECT_EQ(rpc_result.error_code, "REMOTE_FRIEND_UNAVAILABLE");
}

TEST(RemoteFriendClientTest, RespondAndQueriesMapResults) {
    auto fake = std::make_unique<FakeFriendRpcClient>();
    auto* raw = fake.get();
    RemoteFriendClient client(std::move(fake));

    auto respond = client.respond_to_request(42, "target", true);
    ASSERT_TRUE(respond.ok);
    EXPECT_EQ(respond.message, "Friend request accepted");
    ASSERT_EQ(raw->respond_requests.size(), 1u);
    EXPECT_EQ(raw->respond_requests[0].request_id(), "42");
    EXPECT_EQ(raw->respond_requests[0].header().from_uid(), "target");

    auto friends = client.get_friends("target");
    ASSERT_EQ(friends.size(), 1u);
    EXPECT_EQ(friends[0].friend_id, 43u);
    EXPECT_EQ(friends[0].friend_uid, "friend-uid");
    EXPECT_EQ(friends[0].nickname, "Friend Nick");
    EXPECT_EQ(friends[0].status, im::service::friend_::FriendStatus::ACCEPTED);

    auto pending = client.get_pending_requests("target");
    ASSERT_EQ(pending.size(), 1u);
    EXPECT_EQ(pending[0].friend_id, 44u);
    EXPECT_EQ(pending[0].friend_uid, "requester-uid");
    EXPECT_EQ(pending[0].nickname, "Requester Nick");
    EXPECT_EQ(pending[0].status, im::service::friend_::FriendStatus::PENDING);
}

TEST(RemoteFriendClientTest, QueryFailuresReturnEmptyVectors) {
    auto failing = std::make_unique<FakeFriendRpcClient>();
    failing->friends_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    failing->pending_base_code = im::base::SERVER_ERROR;
    RemoteFriendClient client(std::move(failing));

    EXPECT_TRUE(client.get_friends("target").empty());
    EXPECT_TRUE(client.get_pending_requests("target").empty());
}
