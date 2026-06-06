#include <gtest/gtest.h>

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <future>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <httplib.h>
#include <nlohmann/json.hpp>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>
#include <openssl/err.h>

#include <database/redis/redis_mgr.hpp>
#include <gateway/auth/multi_platform_auth.hpp>
#include <gateway/gateway_server/gateway_server.hpp>
#include <group_service.hpp>
#include <message.hpp>
#include <message_service.hpp>
#include <password_hasher.hpp>
#include <push_server_app.hpp>
#include <user_service.hpp>
#include <utils/log_manager.hpp>

#include "../support/postgres_schema.hpp"

#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/proto/command.pb.h"
#include "../../common/proto/message.pb.h"
#include "../../common/proto/push.pb.h"

namespace {

namespace beast = boost::beast;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
namespace websocket = beast::websocket;
using tcp = net::ip::tcp;
using json = nlohmann::json;

using im::db::RedisConfig;
using im::db::redis_manager;
using im::gateway::GatewayServer;
using im::gateway::MultiPlatformAuthManager;
using im::network::ProtobufCodec;
using im::service::group::CreateGroupRequest;
using im::service::group::GroupService;
using im::service::message::MessageService;
using im::service::message::MessageStatus;
using im::service::push::PushServerApp;
using im::service::push::PushServerConfig;
using im::service::user::PasswordHasher;
using im::service::user::RegisterRequest;
using im::service::user::UserService;
using im::utils::LogManager;

const char* kConnStr =
    "host=127.0.0.1 port=5432 dbname=mychat user=mychat password=mychat-dev-pass";
const char* kTestUidPrefix = "remote-push-server-smoke-";

int64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
}

RedisConfig test_redis_config() {
    RedisConfig config;
    config.host = "127.0.0.1";
    config.port = 6379;
    config.password = "mychat-dev-pass";
    config.db = 15;
    config.pool_size = 4;
    config.connect_timeout = 1000;
    config.socket_timeout = 1000;
    config.pool_wait_timeout = 1000;
    return config;
}

int find_free_port() {
    net::io_context ioc;
    tcp::acceptor acceptor(ioc);
    acceptor.open(tcp::v4());
    acceptor.set_option(net::socket_base::reuse_address(true));
    acceptor.bind(tcp::endpoint(net::ip::make_address("127.0.0.1"), 0));
    const int port = acceptor.local_endpoint().port();
    acceptor.close();
    return port;
}

void cleanup_test_data() {
    odb::pgsql::database db(kConnStr);
    odb::transaction t(db.begin());
    db.execute(std::string(R"(DELETE FROM "im_group_messages"
        WHERE "group_id" IN (
                SELECT "group_id" FROM "im_groups"
                WHERE "name" LIKE ')") + kTestUidPrefix + "%' ) OR \"sender_uid\" LIKE '" +
               kTestUidPrefix + "%'");
    db.execute(std::string(R"(DELETE FROM "im_group_members"
        WHERE "group_id" IN (
                SELECT "group_id" FROM "im_groups"
                WHERE "name" LIKE ')") + kTestUidPrefix + "%' ) OR \"user_uid\" LIKE '" +
               kTestUidPrefix + "%'");
    db.execute(std::string(R"(DELETE FROM "im_groups"
        WHERE "name" LIKE ')") + kTestUidPrefix + "%' OR \"creator_uid\" LIKE '" +
               kTestUidPrefix + "%'");
    db.execute(std::string(R"(DELETE FROM "im_messages"
        WHERE "sender_uid" LIKE ')") + kTestUidPrefix + "%' OR \"receiver_uid\" LIKE '" +
               kTestUidPrefix + "%'");
    db.execute(std::string(R"(DELETE FROM "im_users"
        WHERE "account" LIKE ')") + kTestUidPrefix + "%'");
    t.commit();
}

std::filesystem::path source_path(const std::string& relative) {
    return std::filesystem::path(MYCHAT_SOURCE_DIR) / relative;
}

std::filesystem::path write_temp_config(int ws_port,
                                        int http_port,
                                        int push_port,
                                        int gateway_delivery_port,
                                        json push_overrides = json::object()) {
    std::ifstream input(source_path("config/dev.json"));
    if (!input) {
        throw std::runtime_error("Failed to open config/dev.json");
    }
    json config = json::parse(input);

    config["gateway"]["websocket_port"] = ws_port;
    config["gateway"]["http_port"] = http_port;
    config["gateway"]["cert_file"] = source_path("test/network/test_cert.pem").string();
    config["gateway"]["key_file"] = source_path("test/network/test_key.pem").string();

    config["redis"]["db"] = 15;
    config["redis"]["pool_size"] = 4;
    config["redis"]["pool_wait_timeout"] = 1000;

    config["push"]["mode"] = "remote";
    config["push"]["remote_endpoint"] = "127.0.0.1:" + std::to_string(push_port);
    config["push"]["gateway_delivery_listen_address"] =
        "127.0.0.1:" + std::to_string(gateway_delivery_port);
    config["push"]["gateway_delivery_endpoint"] =
        "127.0.0.1:" + std::to_string(gateway_delivery_port);
    config["push"]["timeout_ms"] = 1000;
    for (auto it = push_overrides.begin(); it != push_overrides.end(); ++it) {
        config["push"][it.key()] = it.value();
    }

    auto out_path = std::filesystem::temp_directory_path() /
        ("mychat-remote-push-gateway-server-" + std::to_string(now_ms()) + ".json");
    std::ofstream output(out_path);
    if (!output) {
        throw std::runtime_error("Failed to write temp Gateway config");
    }
    output << config.dump(2);
    return out_path;
}

class TlsWebSocketClient {
public:
    explicit TlsWebSocketClient(std::string token)
        : ssl_ctx_(ssl::context::tlsv12_client),
          ws_(ioc_, ssl_ctx_),
          token_(std::move(token)) {
        ssl_ctx_.set_verify_mode(ssl::verify_none);
    }

    ~TlsWebSocketClient() {
        close();
    }

    bool connect(const std::string& host, int port) {
        try {
            auto results = resolver_.resolve(host, std::to_string(port));
            net::connect(beast::get_lowest_layer(ws_), results);

            if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host.c_str())) {
                beast::error_code ec(static_cast<int>(::ERR_get_error()),
                                     net::error::get_ssl_category());
                last_error_ = ec.message();
                return false;
            }

            ws_.next_layer().handshake(ssl::stream_base::client);
            ws_.binary(true);
            ws_.handshake(host, "/?token=" + token_);
            connected_ = true;
            return true;
        } catch (const std::exception& e) {
            last_error_ = e.what();
            return false;
        }
    }

    bool send(const im::base::IMHeader& header,
              const google::protobuf::Message& message) {
        std::string encoded;
        if (!ProtobufCodec::encode(header, message, encoded)) {
            last_error_ = "failed to encode protobuf frame";
            return false;
        }

        try {
            ws_.write(net::buffer(encoded));
            return true;
        } catch (const std::exception& e) {
            last_error_ = e.what();
            return false;
        }
    }

    std::optional<std::string> receive(std::chrono::milliseconds timeout) {
        buffer_.consume(buffer_.size());

        bool completed = false;
        beast::error_code read_ec;
        ws_.async_read(buffer_, [&](beast::error_code ec, std::size_t) {
            read_ec = ec;
            completed = true;
        });

        ioc_.restart();
        ioc_.run_for(timeout);
        if (!completed) {
            beast::error_code cancel_ec;
            beast::get_lowest_layer(ws_).cancel(cancel_ec);
            ioc_.run();
            last_error_ = "websocket receive timeout";
            return std::nullopt;
        }
        if (read_ec) {
            last_error_ = read_ec.message();
            return std::nullopt;
        }
        return beast::buffers_to_string(buffer_.data());
    }

    void close() {
        if (!connected_) {
            return;
        }
        beast::error_code ec;
        ws_.close(websocket::close_code::normal, ec);
        connected_ = false;
    }

    const std::string& last_error() const {
        return last_error_;
    }

private:
    net::io_context ioc_;
    tcp::resolver resolver_{ioc_};
    ssl::context ssl_ctx_;
    websocket::stream<ssl::stream<tcp::socket>> ws_;
    beast::flat_buffer buffer_;
    std::string token_;
    bool connected_ = false;
    std::string last_error_;
};

class RemotePushGatewayServerSmokeTest : public ::testing::Test {
protected:
    void SetUp() override {
        LogManager::SetLogToConsole("remote_push_gateway_server_smoke");
        LogManager::SetLogToConsole("auth_mgr");
        LogManager::SetLogToConsole("redis_manager");

        redis_manager().shutdown();
        ASSERT_TRUE(redis_manager().initialize(test_redis_config()))
            << "Start Redis with `docker compose up -d redis` before running this test.";

        odb::pgsql::database db(kConnStr);
        im::test::EnsureCoreSchema(db);
        cleanup_test_data();
        cleanup_redis_connection_keys();
    }

    void TearDown() override {
        if (gateway_) {
            gateway_->stop();
        }
        gateway_.reset();
        if (push_server_) {
            push_server_->shutdown();
        }
        push_server_.reset();

        if (!temp_config_.empty()) {
            std::error_code ec;
            std::filesystem::remove(temp_config_, ec);
        }
        cleanup_test_data();
        cleanup_redis_connection_keys();
        redis_manager().shutdown();
    }

    void cleanup_redis_connection_keys() {
        const auto created_user_ids = created_user_ids_;
        redis_manager().execute([created_user_ids](auto& redis) {
            redis.del("online:users");
            for (const auto& key : redis.keys(std::string("user:sessions:") + kTestUidPrefix + "*")) {
                redis.del(key);
            }
            for (const auto& key : redis.keys(std::string("user:platform:") + kTestUidPrefix + "*")) {
                redis.del(key);
            }
            for (const auto& uid : created_user_ids) {
                redis.del(std::string("user:sessions:") + uid);
                redis.del(std::string("user:platform:") + uid);
            }
            for (const auto& key : redis.keys("session:user:*")) {
                auto uid = redis.hget(key, "user_id");
                if (uid && uid->rfind(kTestUidPrefix, 0) == 0) {
                    redis.del(key);
                    continue;
                }
                if (uid) {
                    for (const auto& created_uid : created_user_ids) {
                        if (*uid == created_uid) {
                            redis.del(key);
                            break;
                        }
                    }
                }
            }
        });
    }

    void start_servers() {
        ws_port_ = find_free_port();
        http_port_ = find_free_port();
        gateway_delivery_port_ = find_free_port();

        PushServerConfig push_config;
        push_config.listen_address = "127.0.0.1:0";
        push_config.gateway_delivery_endpoint =
            "127.0.0.1:" + std::to_string(gateway_delivery_port_);
        push_config.timeout_ms = 1000;
        push_server_ = std::make_unique<PushServerApp>(push_config);
        ASSERT_TRUE(push_server_->start());
        ASSERT_GT(push_server_->selected_port(), 0);

        temp_config_ = write_temp_config(
            ws_port_, http_port_, push_server_->selected_port(), gateway_delivery_port_);
        gateway_ = std::make_unique<GatewayServer>(
            temp_config_.string(), temp_config_.string(), ws_port_, http_port_);
        gateway_->start();
        ASSERT_TRUE(wait_for_http_health());
    }

    void start_gateway_with_unavailable_push_server() {
        ws_port_ = find_free_port();
        http_port_ = find_free_port();
        gateway_delivery_port_ = find_free_port();

        temp_config_ = write_temp_config(ws_port_,
                                         http_port_,
                                         find_free_port(),
                                         gateway_delivery_port_,
                                         {{"timeout_ms", 50}});
        gateway_ = std::make_unique<GatewayServer>(
            temp_config_.string(), temp_config_.string(), ws_port_, http_port_);
        gateway_->start();
        ASSERT_TRUE(wait_for_http_health());
    }

    bool wait_for_http_health() const {
        for (int i = 0; i < 50; ++i) {
            httplib::Client client("127.0.0.1", http_port_);
            client.set_connection_timeout(0, 100000);
            client.set_read_timeout(0, 200000);
            auto res = client.Get("/api/v1/health");
            if (res && res->status == 200) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

    bool wait_for_online_count(size_t expected) const {
        for (int i = 0; i < 50; ++i) {
            if (gateway_->get_online_count() >= expected) {
                return true;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        return false;
    }

    void expect_redis_pool_idle(size_t expected_total) const {
        for (int i = 0; i < 50; ++i) {
            const auto stats = redis_manager().get_pool_stats();
            if (stats.total_connections == expected_total &&
                stats.available_connections == expected_total &&
                stats.active_connections == 0) {
                return;
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }

        const auto stats = redis_manager().get_pool_stats();
        EXPECT_EQ(stats.total_connections, expected_total);
        EXPECT_EQ(stats.available_connections, expected_total);
        EXPECT_EQ(stats.active_connections, 0u);
    }

    std::string make_token(const std::string& uid, const std::string& device_id) const {
        MultiPlatformAuthManager auth(temp_config_.string());
        return auth.generate_access_token(uid, uid + "-account", device_id, "web", 3600);
    }

    im::base::IMHeader send_header(const std::string& token,
                                   const std::string& sender_uid,
                                   const std::string& receiver_uid,
                                   const std::string& device_id) const {
        im::base::IMHeader header;
        header.set_version("1.0");
        header.set_seq(1001);
        header.set_cmd_id(im::command::CMD_SEND_MESSAGE);
        header.set_from_uid(sender_uid);
        header.set_to_uid(receiver_uid);
        header.set_timestamp(static_cast<uint64_t>(now_ms()));
        header.set_token(token);
        header.set_device_id(device_id);
        header.set_platform("web");
        return header;
    }

    im::message::SendMessageRequest send_request(const std::string& sender_uid,
                                                 const std::string& receiver_uid,
                                                 const std::string& content) const {
        im::message::SendMessageRequest request;
        request.mutable_header()->set_from_uid(sender_uid);
        request.mutable_header()->set_to_uid(receiver_uid);
        request.mutable_body()->set_content(content);
        request.mutable_body()->set_type(im::message::TEXT);
        return request;
    }

    std::string create_user(const std::string& account) {
        auto db = std::make_shared<odb::pgsql::database>(kConnStr);
        auto hasher = std::make_unique<PasswordHasher>();
        UserService user_service(db, std::move(hasher));

        RegisterRequest request;
        request.account = account;
        request.password = "testPass123";
        request.nickname = account;
        request.now_ms = now_ms();

        auto result = user_service.register_user(request);
        EXPECT_TRUE(result.ok) << "Failed to create test user " << account
                               << ": " << result.message;
        created_user_ids_.push_back(result.profile.uid);
        return result.profile.uid;
    }

    uint64_t create_group(const std::string& name, const std::string& creator_uid) const {
        auto db = std::make_shared<odb::pgsql::database>(kConnStr);
        auto hasher = std::make_unique<PasswordHasher>();
        auto user_service = std::make_shared<UserService>(db, std::move(hasher));
        GroupService group_service(db, user_service);

        CreateGroupRequest request;
        request.name = name;
        request.creator_uid = creator_uid;
        request.now_ms = now_ms();

        auto result = group_service.create_group(request);
        EXPECT_TRUE(result.ok) << "Failed to create test group " << name
                               << ": " << result.message;
        return result.group_id;
    }

    void join_group(uint64_t group_id, const std::string& user_uid) const {
        auto db = std::make_shared<odb::pgsql::database>(kConnStr);
        auto hasher = std::make_unique<PasswordHasher>();
        auto user_service = std::make_shared<UserService>(db, std::move(hasher));
        GroupService group_service(db, user_service);

        auto result = group_service.join_group(group_id, user_uid, now_ms());
        EXPECT_TRUE(result.ok) << "Failed to join user " << user_uid
                               << " to group " << group_id << ": " << result.message;
    }

    int ws_port_ = 0;
    int http_port_ = 0;
    int gateway_delivery_port_ = 0;
    std::filesystem::path temp_config_;
    std::unique_ptr<PushServerApp> push_server_;
    std::unique_ptr<GatewayServer> gateway_;
    std::vector<std::string> created_user_ids_;
};

TEST_F(RemotePushGatewayServerSmokeTest, WsSendReachesReceiverThroughRemotePushServer) {
    start_servers();

    const std::string sender_uid = std::string(kTestUidPrefix) + "sender";
    const std::string receiver_uid = std::string(kTestUidPrefix) + "receiver";
    const std::string sender_device = "remote-server-smoke-sender-device";
    const std::string receiver_device = "remote-server-smoke-receiver-device";
    const std::string sender_token = make_token(sender_uid, sender_device);
    const std::string receiver_token = make_token(receiver_uid, receiver_device);
    ASSERT_FALSE(sender_token.empty());
    ASSERT_FALSE(receiver_token.empty());

    TlsWebSocketClient receiver(receiver_token);
    ASSERT_TRUE(receiver.connect("127.0.0.1", ws_port_)) << receiver.last_error();

    TlsWebSocketClient sender(sender_token);
    ASSERT_TRUE(sender.connect("127.0.0.1", ws_port_)) << sender.last_error();
    ASSERT_TRUE(wait_for_online_count(2));
    expect_redis_pool_idle(4);

    const std::string content = "remote push from real gateway server";
    ASSERT_TRUE(sender.send(
        send_header(sender_token, sender_uid, receiver_uid, sender_device),
        send_request(sender_uid, receiver_uid, content)))
        << sender.last_error();

    auto ack_frame = sender.receive(std::chrono::seconds(5));
    ASSERT_TRUE(ack_frame.has_value()) << sender.last_error();

    im::base::IMHeader ack_header;
    im::message::SendMessageResponse ack;
    ASSERT_TRUE(ProtobufCodec::decode(*ack_frame, ack_header, ack));
    ASSERT_EQ(ack.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(ack.has_message());
    ASSERT_FALSE(ack.message().message_id().empty());

    auto push_frame = receiver.receive(std::chrono::seconds(5));
    ASSERT_TRUE(push_frame.has_value()) << receiver.last_error();

    im::base::IMHeader push_header;
    im::push::PushRequest push_request;
    ASSERT_TRUE(ProtobufCodec::decode(*push_frame, push_header, push_request));
    EXPECT_EQ(push_header.cmd_id(), im::command::CMD_PUSH_MESSAGE);
    EXPECT_EQ(push_header.to_uid(), receiver_uid);
    ASSERT_TRUE(push_request.has_body());
    EXPECT_EQ(push_request.body().type(), im::push::PUSH_MESSAGE);
    EXPECT_EQ(push_request.body().content(), content);
    EXPECT_EQ(push_request.body().related_message_id(), ack.message().message_id());
    expect_redis_pool_idle(4);
}

TEST_F(RemotePushGatewayServerSmokeTest, ConcurrentWsSendsKeepRedisPoolHealthy) {
    start_servers();

    constexpr int kPairCount = 6;
    std::vector<std::string> sender_uids;
    std::vector<std::string> receiver_uids;
    std::vector<std::string> sender_devices;
    std::vector<std::string> sender_tokens;
    std::vector<std::string> receiver_tokens;
    std::vector<std::string> contents;
    std::vector<std::unique_ptr<TlsWebSocketClient>> senders;
    std::vector<std::unique_ptr<TlsWebSocketClient>> receivers;
    sender_uids.reserve(kPairCount);
    receiver_uids.reserve(kPairCount);
    sender_devices.reserve(kPairCount);
    sender_tokens.reserve(kPairCount);
    receiver_tokens.reserve(kPairCount);
    contents.reserve(kPairCount);
    senders.reserve(kPairCount);
    receivers.reserve(kPairCount);

    for (int i = 0; i < kPairCount; ++i) {
        sender_uids.push_back(std::string(kTestUidPrefix) + "concurrent-sender-" +
                              std::to_string(i));
        receiver_uids.push_back(std::string(kTestUidPrefix) + "concurrent-receiver-" +
                                std::to_string(i));
        sender_devices.push_back("remote-server-concurrent-sender-device-" +
                                 std::to_string(i));
        sender_tokens.push_back(make_token(sender_uids.back(), sender_devices.back()));
        receiver_tokens.push_back(make_token(receiver_uids.back(),
                                             "remote-server-concurrent-receiver-device-" +
                                                     std::to_string(i)));
        contents.push_back("remote concurrent push " + std::to_string(i));
        ASSERT_FALSE(sender_tokens.back().empty());
        ASSERT_FALSE(receiver_tokens.back().empty());

        receivers.push_back(std::make_unique<TlsWebSocketClient>(receiver_tokens.back()));
        ASSERT_TRUE(receivers.back()->connect("127.0.0.1", ws_port_))
            << receivers.back()->last_error();
        senders.push_back(std::make_unique<TlsWebSocketClient>(sender_tokens.back()));
        ASSERT_TRUE(senders.back()->connect("127.0.0.1", ws_port_))
            << senders.back()->last_error();
    }

    ASSERT_TRUE(wait_for_online_count(kPairCount * 2));
    expect_redis_pool_idle(4);

    struct SendResult {
        bool ok = false;
        std::string error;
        std::string message_id;
    };
    std::vector<std::future<SendResult>> send_tasks;
    send_tasks.reserve(kPairCount);
    for (int i = 0; i < kPairCount; ++i) {
        send_tasks.push_back(std::async(std::launch::async, [&, i] {
            SendResult result;
            if (!senders[i]->send(send_header(sender_tokens[i],
                                              sender_uids[i],
                                              receiver_uids[i],
                                              sender_devices[i]),
                                  send_request(sender_uids[i],
                                               receiver_uids[i],
                                               contents[i]))) {
                result.error = senders[i]->last_error();
                return result;
            }

            auto ack_frame = senders[i]->receive(std::chrono::seconds(5));
            if (!ack_frame.has_value()) {
                result.error = senders[i]->last_error();
                return result;
            }

            im::base::IMHeader ack_header;
            im::message::SendMessageResponse ack;
            if (!ProtobufCodec::decode(*ack_frame, ack_header, ack)) {
                result.error = "failed to decode send ack";
                return result;
            }
            if (ack.base().error_code() != im::base::SUCCESS || !ack.has_message()) {
                result.error = "send ack was not successful";
                return result;
            }

            result.ok = true;
            result.message_id = ack.message().message_id();
            return result;
        }));
    }

    std::vector<std::string> message_ids;
    message_ids.reserve(kPairCount);
    for (auto& task : send_tasks) {
        auto result = task.get();
        ASSERT_TRUE(result.ok) << result.error;
        ASSERT_FALSE(result.message_id.empty());
        message_ids.push_back(result.message_id);
    }

    for (int i = 0; i < kPairCount; ++i) {
        auto push_frame = receivers[i]->receive(std::chrono::seconds(5));
        ASSERT_TRUE(push_frame.has_value()) << receivers[i]->last_error();

        im::base::IMHeader push_header;
        im::push::PushRequest push_request;
        ASSERT_TRUE(ProtobufCodec::decode(*push_frame, push_header, push_request));
        EXPECT_EQ(push_header.cmd_id(), im::command::CMD_PUSH_MESSAGE);
        EXPECT_EQ(push_header.to_uid(), receiver_uids[i]);
        ASSERT_TRUE(push_request.has_body());
        EXPECT_EQ(push_request.body().type(), im::push::PUSH_MESSAGE);
        EXPECT_EQ(push_request.body().content(), contents[i]);
        EXPECT_EQ(push_request.body().related_message_id(), message_ids[i]);
    }

    expect_redis_pool_idle(4);
}

TEST_F(RemotePushGatewayServerSmokeTest, GroupHttpSendFansOutThroughRemotePushServer) {
    start_servers();

    const std::string owner_uid = create_user(std::string(kTestUidPrefix) + "group-owner");
    const std::string member1_uid = create_user(std::string(kTestUidPrefix) + "group-member1");
    const std::string member2_uid = create_user(std::string(kTestUidPrefix) + "group-member2");
    ASSERT_FALSE(owner_uid.empty());
    ASSERT_FALSE(member1_uid.empty());
    ASSERT_FALSE(member2_uid.empty());

    const uint64_t group_id =
        create_group(std::string(kTestUidPrefix) + "group-http-remote-push", owner_uid);
    ASSERT_GT(group_id, 0U);
    join_group(group_id, member1_uid);
    join_group(group_id, member2_uid);

    const std::string owner_token = make_token(owner_uid, "group-owner-device");
    const std::string member1_token = make_token(member1_uid, "group-member1-device");
    const std::string member2_token = make_token(member2_uid, "group-member2-device");
    ASSERT_FALSE(owner_token.empty());
    ASSERT_FALSE(member1_token.empty());
    ASSERT_FALSE(member2_token.empty());

    TlsWebSocketClient member1(member1_token);
    ASSERT_TRUE(member1.connect("127.0.0.1", ws_port_)) << member1.last_error();
    TlsWebSocketClient member2(member2_token);
    ASSERT_TRUE(member2.connect("127.0.0.1", ws_port_)) << member2.last_error();
    ASSERT_TRUE(wait_for_online_count(2));
    expect_redis_pool_idle(4);

    const std::string content = "group http send through remote push server";
    httplib::Client client("127.0.0.1", http_port_);
    client.set_connection_timeout(0, 200000);
    client.set_read_timeout(2, 0);
    httplib::Headers headers = {{"Authorization", "Bearer " + owner_token}};
    auto response = client.Post("/api/v1/groups/messages/send",
                                headers,
                                json{{"group_id", group_id}, {"content", content}}.dump(),
                                "application/json");
    ASSERT_TRUE(response) << "HTTP request to Gateway group message route failed";
    ASSERT_EQ(response->status, 201) << response->body;
    const auto body = json::parse(response->body);
    ASSERT_TRUE(body.contains("msg_id"));
    const std::string msg_id = std::to_string(body["msg_id"].get<uint64_t>());

    auto assert_group_push = [&](TlsWebSocketClient& client_ws,
                                 const std::string& expected_uid) {
        auto frame = client_ws.receive(std::chrono::seconds(5));
        ASSERT_TRUE(frame.has_value()) << client_ws.last_error();

        im::base::IMHeader push_header;
        im::push::PushRequest push_request;
        ASSERT_TRUE(ProtobufCodec::decode(*frame, push_header, push_request));
        EXPECT_EQ(push_header.cmd_id(), im::command::CMD_PUSH_MESSAGE);
        EXPECT_EQ(push_header.to_uid(), expected_uid);
        ASSERT_TRUE(push_request.has_body());
        EXPECT_EQ(push_request.body().type(), im::push::PUSH_MESSAGE);
        EXPECT_EQ(push_request.body().content(), content);
        EXPECT_EQ(push_request.body().related_message_id(), msg_id);
    };

    assert_group_push(member1, member1_uid);
    assert_group_push(member2, member2_uid);
    expect_redis_pool_idle(4);
}

TEST_F(RemotePushGatewayServerSmokeTest, RemoteModeRejectsEmptyPushEndpoint) {
    ws_port_ = find_free_port();
    http_port_ = find_free_port();
    gateway_delivery_port_ = find_free_port();
    temp_config_ = write_temp_config(ws_port_,
                                     http_port_,
                                     find_free_port(),
                                     gateway_delivery_port_,
                                     {{"remote_endpoint", ""}});

    EXPECT_THROW({
        GatewayServer gateway(temp_config_.string(),
                              temp_config_.string(),
                              ws_port_,
                              http_port_);
    }, std::runtime_error);
}

TEST_F(RemotePushGatewayServerSmokeTest, RemoteModeRejectsEmptyGatewayDeliveryListenAddress) {
    ws_port_ = find_free_port();
    http_port_ = find_free_port();
    gateway_delivery_port_ = find_free_port();
    temp_config_ = write_temp_config(ws_port_,
                                     http_port_,
                                     find_free_port(),
                                     gateway_delivery_port_,
                                     {{"gateway_delivery_listen_address", ""}});

    EXPECT_THROW({
        GatewayServer gateway(temp_config_.string(),
                              temp_config_.string(),
                              ws_port_,
                              http_port_);
    }, std::runtime_error);
}

TEST_F(RemotePushGatewayServerSmokeTest, UnavailablePushServerKeepsSendBestEffortAndOfflinePullable) {
    start_gateway_with_unavailable_push_server();

    const std::string sender_uid = std::string(kTestUidPrefix) + "sender";
    const std::string receiver_uid = std::string(kTestUidPrefix) + "receiver";
    const std::string sender_device = "remote-server-unavailable-sender-device";
    const std::string sender_token = make_token(sender_uid, sender_device);
    ASSERT_FALSE(sender_token.empty());

    TlsWebSocketClient sender(sender_token);
    ASSERT_TRUE(sender.connect("127.0.0.1", ws_port_)) << sender.last_error();
    ASSERT_TRUE(wait_for_online_count(1));

    const std::string content = "remote push server unavailable";
    ASSERT_TRUE(sender.send(
        send_header(sender_token, sender_uid, receiver_uid, sender_device),
        send_request(sender_uid, receiver_uid, content)))
        << sender.last_error();

    auto ack_frame = sender.receive(std::chrono::seconds(5));
    ASSERT_TRUE(ack_frame.has_value()) << sender.last_error();

    im::base::IMHeader ack_header;
    im::message::SendMessageResponse ack;
    ASSERT_TRUE(ProtobufCodec::decode(*ack_frame, ack_header, ack));
    ASSERT_EQ(ack.base().error_code(), im::base::SUCCESS);
    ASSERT_TRUE(ack.has_message());

    auto db = std::make_shared<odb::pgsql::database>(kConnStr);
    MessageService message_service(db);
    const auto offline = message_service.pull_offline(receiver_uid, now_ms() + 10000, 10);
    ASSERT_EQ(offline.size(), 1U);
    EXPECT_EQ(offline[0].msg_id, std::stoull(ack.message().message_id()));
    EXPECT_EQ(offline[0].sender_uid, sender_uid);
    EXPECT_EQ(offline[0].receiver_uid, receiver_uid);
    EXPECT_EQ(offline[0].content, content);
    EXPECT_EQ(offline[0].status, MessageStatus::SENT);
    EXPECT_EQ(offline[0].delivered_time, 0);
}

} // namespace
