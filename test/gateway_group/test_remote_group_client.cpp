#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

#include <gateway/http/remote_group_client.hpp>
#include <group.hpp>

#include "base.pb.h"
#include "group.pb.h"

namespace {

using im::gateway::GroupRpcClient;
using im::gateway::RemoteGroupClient;

class FakeGroupRpcClient final : public GroupRpcClient {
public:
    ::grpc::Status create_group(::grpc::ClientContext* /*context*/,
                                const im::group::CreateGroupRequest& request,
                                im::group::CreateGroupResponse* response) override {
        create_requests.push_back(request);
        response->mutable_base()->set_error_code(create_base_code);
        response->mutable_base()->set_error_message(create_base_message);
        if (create_base_code == im::base::SUCCESS) {
            response->set_group_id(42);
            response->mutable_group()->set_group_record_id(42);
            response->mutable_group()->set_name(request.name());
            response->mutable_group()->set_owner_uid(request.header().from_uid());
            response->mutable_group()->set_member_count(1);
        }
        return create_status;
    }

    ::grpc::Status join_group(::grpc::ClientContext* /*context*/,
                              const im::group::JoinGroupRequest& request,
                              im::group::GroupActionResponse* response) override {
        join_requests.push_back(request);
        response->mutable_base()->set_error_code(join_base_code);
        response->mutable_base()->set_error_message(join_base_message);
        return join_status;
    }

    ::grpc::Status leave_group(::grpc::ClientContext* /*context*/,
                               const im::group::LeaveGroupRequest& request,
                               im::group::GroupActionResponse* response) override {
        leave_requests.push_back(request);
        response->mutable_base()->set_error_code(leave_base_code);
        response->mutable_base()->set_error_message(leave_base_message);
        return leave_status;
    }

    ::grpc::Status get_group_info(::grpc::ClientContext* /*context*/,
                                  const im::group::GetGroupInfoRequest& request,
                                  im::group::GetGroupInfoResponse* response) override {
        info_requests.push_back(request);
        response->mutable_base()->set_error_code(info_base_code);
        response->mutable_base()->set_error_message(info_base_message);
        if (info_base_code == im::base::SUCCESS) {
            response->mutable_group()->set_group_record_id(request.group_record_id());
            response->mutable_group()->set_name("remote-group-info");
            response->mutable_group()->set_owner_uid("owner");
            response->mutable_group()->set_create_time(1234);
            response->mutable_group()->set_member_count(3);
        }
        return info_status;
    }

    ::grpc::Status search_groups(::grpc::ClientContext* /*context*/,
                                 const im::group::SearchGroupsRequest& request,
                                 im::group::SearchGroupsResponse* response) override {
        search_requests.push_back(request);
        response->mutable_base()->set_error_code(search_base_code);
        response->mutable_base()->set_error_message(search_base_message);
        if (search_base_code == im::base::SUCCESS) {
            auto* group = response->add_groups();
            group->set_group_record_id(47);
            group->set_name("remote-search-group");
            group->set_owner_uid("owner");
            group->set_create_time(1234);
            group->set_member_count(4);
        }
        return search_status;
    }

    ::grpc::Status group_exists(::grpc::ClientContext* /*context*/,
                                const im::group::GroupExistsRequest& request,
                                im::group::GroupExistsResponse* response) override {
        exists_requests.push_back(request);
        response->mutable_base()->set_error_code(exists_base_code);
        response->set_exists(exists_value);
        return exists_status;
    }

    ::grpc::Status list_my_groups(::grpc::ClientContext* /*context*/,
                                  const im::group::GetGroupListRequest& request,
                                  im::group::GetGroupListResponse* response) override {
        list_requests.push_back(request);
        response->mutable_base()->set_error_code(list_base_code);
        if (list_base_code == im::base::SUCCESS) {
            auto* group = response->add_groups();
            group->set_group_record_id(43);
            group->set_name("remote-group");
            group->set_owner_uid(request.header().from_uid());
            group->set_create_time(1234);
            group->set_member_count(2);
        }
        return list_status;
    }

    ::grpc::Status list_members(::grpc::ClientContext* /*context*/,
                                const im::group::GetGroupMembersRequest& request,
                                im::group::GetGroupMembersResponse* response) override {
        members_requests.push_back(request);
        response->mutable_base()->set_error_code(members_base_code);
        if (members_base_code == im::base::SUCCESS) {
            auto* member = response->add_members();
            member->set_member_record_id(44);
            member->set_uid("member-uid");
            member->set_nickname("Member Nick");
            member->set_role(im::group::MEMBER);
            member->set_join_time(1234);
        }
        return members_status;
    }

    ::grpc::Status send_message(::grpc::ClientContext* /*context*/,
                                const im::group::SendGroupMessageRequest& request,
                                im::group::SendGroupMessageResponse* response) override {
        send_requests.push_back(request);
        response->mutable_base()->set_error_code(send_base_code);
        response->mutable_base()->set_error_message(send_base_message);
        if (send_base_code == im::base::SUCCESS) {
            response->mutable_message()->set_msg_id(45);
            response->mutable_message()->set_sender_uid(request.header().from_uid());
            response->mutable_message()->set_content(request.content());
        }
        return send_status;
    }

    ::grpc::Status get_history(::grpc::ClientContext* /*context*/,
                               const im::group::GetGroupMessagesRequest& request,
                               im::group::GetGroupMessagesResponse* response) override {
        history_requests.push_back(request);
        response->mutable_base()->set_error_code(history_base_code);
        if (history_base_code == im::base::SUCCESS) {
            auto* message = response->add_messages();
            message->set_msg_id(46);
            message->set_sender_uid("member-uid");
            message->set_sender_nickname("Member Nick");
            message->set_content("hello");
            message->set_create_time(1234);
        }
        return history_status;
    }

    ::grpc::Status create_status = ::grpc::Status::OK;
    ::grpc::Status join_status = ::grpc::Status::OK;
    ::grpc::Status leave_status = ::grpc::Status::OK;
    ::grpc::Status info_status = ::grpc::Status::OK;
    ::grpc::Status search_status = ::grpc::Status::OK;
    ::grpc::Status exists_status = ::grpc::Status::OK;
    ::grpc::Status list_status = ::grpc::Status::OK;
    ::grpc::Status members_status = ::grpc::Status::OK;
    ::grpc::Status send_status = ::grpc::Status::OK;
    ::grpc::Status history_status = ::grpc::Status::OK;
    im::base::ErrorCode create_base_code = im::base::SUCCESS;
    im::base::ErrorCode join_base_code = im::base::SUCCESS;
    im::base::ErrorCode leave_base_code = im::base::SUCCESS;
    im::base::ErrorCode info_base_code = im::base::SUCCESS;
    im::base::ErrorCode search_base_code = im::base::SUCCESS;
    im::base::ErrorCode exists_base_code = im::base::SUCCESS;
    im::base::ErrorCode list_base_code = im::base::SUCCESS;
    im::base::ErrorCode members_base_code = im::base::SUCCESS;
    im::base::ErrorCode send_base_code = im::base::SUCCESS;
    im::base::ErrorCode history_base_code = im::base::SUCCESS;
    std::string create_base_message;
    std::string join_base_message;
    std::string leave_base_message;
    std::string info_base_message;
    std::string search_base_message;
    std::string send_base_message;
    bool exists_value = true;
    std::vector<im::group::CreateGroupRequest> create_requests;
    std::vector<im::group::JoinGroupRequest> join_requests;
    std::vector<im::group::LeaveGroupRequest> leave_requests;
    std::vector<im::group::GetGroupInfoRequest> info_requests;
    std::vector<im::group::SearchGroupsRequest> search_requests;
    std::vector<im::group::GroupExistsRequest> exists_requests;
    std::vector<im::group::GetGroupListRequest> list_requests;
    std::vector<im::group::GetGroupMembersRequest> members_requests;
    std::vector<im::group::SendGroupMessageRequest> send_requests;
    std::vector<im::group::GetGroupMessagesRequest> history_requests;
};

}  // namespace

TEST(RemoteGroupClientTest, ActionsMapSuccess) {
    auto fake = std::make_unique<FakeGroupRpcClient>();
    auto* raw = fake.get();
    RemoteGroupClient client(std::move(fake));

    im::service::group::CreateGroupRequest create_request;
    create_request.creator_uid = "owner";
    create_request.name = "group-name";
    create_request.now_ms = 1234;
    auto create = client.create_group(create_request);
    ASSERT_TRUE(create.ok);
    EXPECT_EQ(create.group_id, 42u);
    ASSERT_EQ(raw->create_requests.size(), 1u);
    EXPECT_EQ(raw->create_requests[0].header().from_uid(), "owner");
    EXPECT_EQ(raw->create_requests[0].name(), "group-name");

    auto join = client.join_group(42, "member", 1235);
    ASSERT_TRUE(join.ok);
    ASSERT_EQ(raw->join_requests.size(), 1u);
    EXPECT_EQ(raw->join_requests[0].group_id(), 42u);
    EXPECT_EQ(raw->join_requests[0].header().from_uid(), "member");

    auto leave = client.leave_group(42, "member");
    ASSERT_TRUE(leave.ok);
    ASSERT_EQ(raw->leave_requests.size(), 1u);
    EXPECT_EQ(raw->leave_requests[0].header().from_uid(), "member");
}

TEST(RemoteGroupClientTest, QueriesAndMessagesMapResults) {
    auto fake = std::make_unique<FakeGroupRpcClient>();
    auto* raw = fake.get();
    RemoteGroupClient client(std::move(fake));

    EXPECT_TRUE(client.group_exists(42));
    ASSERT_EQ(raw->exists_requests.size(), 1u);
    EXPECT_EQ(raw->exists_requests[0].group_id(), 42u);

    auto groups = client.list_my_groups("owner");
    ASSERT_EQ(groups.size(), 1u);
    EXPECT_EQ(groups[0].group_id, 43u);
    EXPECT_EQ(groups[0].name, "remote-group");
    EXPECT_EQ(groups[0].creator_uid, "owner");

    auto group_info = client.get_group_info(42);
    ASSERT_TRUE(group_info.has_value());
    EXPECT_EQ(group_info->group_id, 42u);
    EXPECT_EQ(group_info->name, "remote-group-info");
    ASSERT_EQ(raw->info_requests.size(), 1u);
    EXPECT_EQ(raw->info_requests[0].group_record_id(), 42u);

    auto search = client.search_groups("remote", 10);
    ASSERT_EQ(search.size(), 1u);
    EXPECT_EQ(search[0].group_id, 47u);
    EXPECT_EQ(search[0].name, "remote-search-group");
    ASSERT_EQ(raw->search_requests.size(), 1u);
    EXPECT_EQ(raw->search_requests[0].keyword(), "remote");
    EXPECT_EQ(raw->search_requests[0].limit(), 10);

    auto members = client.list_members(42, "owner");
    ASSERT_EQ(members.size(), 1u);
    EXPECT_EQ(members[0].user_uid, "member-uid");
    EXPECT_EQ(members[0].role, im::service::group::GroupRole::MEMBER);

    auto send = client.send_message(42, "member", "hello", 1236);
    ASSERT_TRUE(send.ok);
    EXPECT_EQ(send.msg_id, 45u);
    ASSERT_EQ(raw->send_requests.size(), 1u);
    EXPECT_EQ(raw->send_requests[0].content(), "hello");

    auto history = client.get_history(42, "owner", 2000, 10);
    ASSERT_EQ(history.size(), 1u);
    EXPECT_EQ(history[0].msg_id, 46u);
    ASSERT_EQ(raw->history_requests.size(), 1u);
    EXPECT_EQ(raw->history_requests[0].header().from_uid(), "owner");
    EXPECT_EQ(raw->history_requests[0].group_record_id(), 42u);
}

TEST(RemoteGroupClientTest, FailuresReturnConservativeValues) {
    auto fake = std::make_unique<FakeGroupRpcClient>();
    fake->create_base_code = im::base::NOT_FOUND;
    fake->create_base_message = "missing";
    fake->exists_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    fake->list_base_code = im::base::SERVER_ERROR;
    fake->members_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    fake->send_status = ::grpc::Status(::grpc::StatusCode::UNAVAILABLE, "down");
    fake->history_base_code = im::base::PERMISSION_DENIED;
    RemoteGroupClient client(std::move(fake));

    auto create = client.create_group({});
    EXPECT_FALSE(create.ok);
    EXPECT_EQ(create.error_code, "GROUP_NOT_FOUND");
    EXPECT_FALSE(client.group_exists(42));
    EXPECT_TRUE(client.list_my_groups("owner").empty());
    EXPECT_TRUE(client.list_members(42, "owner").empty());
    EXPECT_FALSE(client.send_message(42, "member", "hello", 1236).ok);
    EXPECT_TRUE(client.get_history(42, "owner", 2000, 10).empty());
}
