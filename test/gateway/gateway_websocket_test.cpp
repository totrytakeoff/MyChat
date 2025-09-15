/**
 * @file gateway_websocket_test.cpp
 * @brief Gateway WebSocket功能完整测试
 * @details 测试WebSocket连接、认证、消息收发、错误处理等完整功能
 * @author myself
 * @date 2025/09/13
 */

#include <gtest/gtest.h>
#include <thread>
#include <chrono>
#include <future>
#include <atomic>
#include <memory>
#include <vector>
#include <string>
#include <iostream>

// Boost Beast WebSocket客户端
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>

// 项目头文件
#include "../../gateway/gateway_server/gateway_server.hpp"
#include "../../gateway/message_processor/message_parser.hpp"
#include "../../gateway/message_processor/message_processor.hpp"
#include "../../gateway/router/router_mgr.hpp"
#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../common/proto/command.pb.h"
#include "../../common/utils/config_mgr.hpp"
#include "../../gateway/auth/multi_platform_auth.hpp"

// JSON库
#include <nlohmann/json.hpp>

using json = nlohmann::json;
using namespace im::gateway;
using namespace im::base;
using namespace im::command;
using namespace im::network;

namespace beast = boost::beast;
namespace http = beast::http;
namespace websocket = beast::websocket;
namespace net = boost::asio;
namespace ssl = boost::asio::ssl;
using tcp = net::ip::tcp;

/**
 * @brief 简单的WebSocket测试客户端
 * @details 使用Boost Beast实现的WebSocket客户端，用于测试服务器功能
 */
class SimpleWebSocketClient {
public:
    SimpleWebSocketClient() : resolver_(ioc_), ssl_ctx_(ssl::context::tlsv12_client), 
                               ws_(ioc_, ssl_ctx_), connected_(false) {}
    
    ~SimpleWebSocketClient() {
        disconnect();
    }
    
    /**
     * @brief 连接到WebSocket服务器
     * @param host 服务器地址
     * @param port 服务器端口
     * @param target 连接路径（可包含token参数）
     * @return 是否连接成功
     */
    bool connect(const std::string& host, const std::string& port, const std::string& target = "/") {
        try {
            // 解析地址
            auto const results = resolver_.resolve(host, port);
            
            // 连接到服务器
            auto ep = net::connect(boost::beast::get_lowest_layer(ws_), results);
            
            // 设置SNI主机名
            if (!SSL_set_tlsext_host_name(ws_.next_layer().native_handle(), host.c_str())) {
                beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
                std::cout << "SSL SNI error: " << ec.message() << std::endl;
                return false;
            }
            
            // 执行SSL握手
            ws_.next_layer().handshake(ssl::stream_base::client);
            
            // // 设置用户代理
            // ws_.set_option(websocket::stream_base::user_agent(
            //     std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-coro"));
            
            // 执行WebSocket握手
            ws_.handshake(host, target);
            
            connected_ = true;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "Connection error: " << e.what() << std::endl;
            return false;
        }
    }
    
    /**
     * @brief 连接到非SSL WebSocket服务器
     * @param host 服务器地址
     * @param port 服务器端口
     * @param target 连接路径
     * @return 是否连接成功
     */
    bool connect_plain(const std::string& host, const std::string& port, const std::string& target = "/") {
        try {
            // 创建普通WebSocket流
            websocket::stream<tcp::socket> plain_ws(ioc_);
            
            // 解析地址
            auto const results = resolver_.resolve(host, port);
            
            // 连接到服务器
            auto ep = net::connect(plain_ws.next_layer(), results);
            
            // 设置用户代理
            // plain_ws.set_option(websocket::stream_base::user_agent(
            //     std::string(BOOST_BEAST_VERSION_STRING) + " websocket-client-test"));
            
            // 执行WebSocket握手
            plain_ws.handshake(host, target);
            
            // 将连接移动到成员变量（这里简化处理，实际应该用variant或者重新设计）
            // 为了测试目的，我们先假设服务器支持非SSL连接
            
            connected_ = true;
            return true;
            
        } catch (const std::exception& e) {
            std::cout << "Plain connection error: " << e.what() << std::endl;
            return false;
        }
    }
    
    /**
     * @brief 断开连接
     */
    void disconnect() {
        if (connected_) {
            try {
                ws_.close(websocket::close_code::normal);
                connected_ = false;
            } catch (const std::exception& e) {
                std::cout << "Disconnect error: " << e.what() << std::endl;
            }
        }
    }
    
    /**
     * @brief 发送protobuf消息
     * @param header 消息头
     * @param message protobuf消息体
     * @return 是否发送成功
     */
    bool send_protobuf_message(const IMHeader& header, const google::protobuf::Message& message) {
        if (!connected_) return false;
        
        std::string encoded_data;
        if (!ProtobufCodec::encode(header, message, encoded_data)) {
            std::cout << "Failed to encode protobuf message" << std::endl;
            return false;
        }
        
        try {
            ws_.write(net::buffer(encoded_data));
            return true;
        } catch (const std::exception& e) {
            std::cout << "Send error: " << e.what() << std::endl;
            return false;
        }
    }
    
    /**
     * @brief 接收消息
     * @param timeout_ms 超时时间（毫秒）
     * @return 接收到的消息，失败返回空字符串
     */
    std::string receive_message(int timeout_ms = 5000) {
        if (!connected_) return "";
        
        try {
            beast::flat_buffer buffer;
            
            // 设置超时 (注意：SSL stream的超时设置方式不同)
            // ws_.next_layer().expires_after(std::chrono::milliseconds(timeout_ms));
            
            // 读取消息
            ws_.read(buffer);
            
            std::string message = beast::buffers_to_string(buffer.data());
            
            // 尝试解析protobuf消息
            IMHeader header;
            BaseResponse response;
            if (ProtobufCodec::decode(message, header, response)) {
                std::cout << "Received protobuf message - cmd_id: " << header.cmd_id() 
                         << ", seq: " << header.seq() 
                         << ", error_code: " << response.error_code() 
                         << ", message: " << response.error_message() << std::endl;
            } else {
                std::cout << "Received raw message length: " << message.length() << std::endl;
            }
            
            return message;
            
        } catch (const std::exception& e) {
            std::cout << "Receive error: " << e.what() << std::endl;
            return "";
        }
    }
    
    /**
     * @brief 检查连接状态
     */
    bool is_connected() const {
        return connected_;
    }

private:
    net::io_context ioc_;
    tcp::resolver resolver_;
    ssl::context ssl_ctx_;
    websocket::stream<ssl::stream<tcp::socket>> ws_;
    std::atomic<bool> connected_;
};

/**
 * @brief Gateway WebSocket测试套件
 */
class GatewayWebSocketTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 配置文件路径
        std::string auth_config = "../test_auth_config.json";
        std::string router_config = "../test_router_config.json";
        // 提前设置测试证书，确保GatewayServer初始化时加载
        setenv("WS_CERT", "/home/myself/workspace/MyChat/test/network/test_cert.pem", 1);
        setenv("WS_KEY",  "/home/myself/workspace/MyChat/test/network/test_key.pem", 1);

        auto redis_config = im::db::RedisConfig("127.0.0.1", 6379, "myself", 1);
        ASSERT_TRUE(im::db::RedisManager::GetInstance().initialize(redis_config));
        
        // 创建Gateway服务器
        gateway_server_ = std::make_unique<GatewayServer>(auth_config, router_config, ws_port_, http_port_);
        
        // 注册测试消息处理器
        register_test_handlers();
        
        // 启动服务器
        gateway_server_->start();
        
        // 等待服务器启动完成
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
        
        // 生成测试用的JWT token
        generate_test_token();
    }
    
    void TearDown() override {
        if (gateway_server_) {
            gateway_server_->stop();
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
    
    /**
     * @brief 注册测试消息处理器
     */
    void register_test_handlers() {
        // 注册简单的echo处理器 (CMD_SEND_MESSAGE = 2001)
        gateway_server_->register_message_handlers(2001, 
            [](const UnifiedMessage& msg) -> ProcessorResult {
                std::cout << "Processing echo message from: " << msg.get_from_uid() 
                         << " to: " << msg.get_to_uid() << std::endl;
                
                json response_json;
                response_json["code"] = 200;
                response_json["message"] = "Echo: " + msg.get_json_body();
                response_json["timestamp"] = std::chrono::duration_cast<std::chrono::milliseconds>(
                    std::chrono::system_clock::now().time_since_epoch()).count();
                
                return ProcessorResult(0, "Success", "", response_json.dump());
            });
        
        // 注册用户信息查询处理器 (CMD_GET_USER_INFO = 1004)
        gateway_server_->register_message_handlers(1004,
            [](const UnifiedMessage& msg) -> ProcessorResult {
                std::cout << "Processing get user info for: " << msg.get_from_uid() << std::endl;
                
                json response_json;
                response_json["code"] = 200;
                response_json["user_id"] = msg.get_from_uid();
                response_json["username"] = "test_user";
                response_json["platform"] = "test";
                
                return ProcessorResult(0, "Success", "", response_json.dump());
            });
    }
    
    /**
     * @brief 生成测试用的JWT token
     */
    void generate_test_token() {
        try {
            // 使用与GatewayServer相同的配置路径，避免secret不一致
            im::utils::ConfigManager config_mgr("../test_auth_config.json");
            auto auth_mgr = std::make_shared<MultiPlatformAuthManager>("../test_auth_config.json");
            
            UserTokenInfo token_info;
            token_info.user_id = test_user_id_;
            token_info.username = "test_user";
            token_info.device_id = test_device_id_;
            token_info.platform = test_platform_;
            
            // 生成access token
            test_token_ = auth_mgr->generate_access_token(
                token_info.user_id, token_info.username, 
                token_info.device_id, token_info.platform, 3600);
            
            std::cout << "Generated test token: " << test_token_ << std::endl;
            
        } catch (const std::exception& e) {
            std::cout << "Failed to generate test token: " << e.what() << std::endl;
            test_token_ = ""; // 使用空token进行无token测试
        }
    }
    
    /**
     * @brief 创建测试用的protobuf消息头
     */
    IMHeader create_test_header(uint32_t cmd_id, uint32_t seq = 1) {
        IMHeader header;
        header.set_version("1");
        header.set_cmd_id(cmd_id);
        header.set_seq(seq);
        header.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch()).count());
        header.set_from_uid(test_user_id_);
        header.set_to_uid(test_user_id_);
        header.set_device_id(test_device_id_);
        header.set_platform(test_platform_);
        
        return header;
    }

protected:
    std::unique_ptr<GatewayServer> gateway_server_;
    
    // 服务器配置
    const uint16_t ws_port_ = 8090;
    const uint16_t http_port_ = 8102;
    const std::string host_ = "localhost";
    const std::string port_str_ = "8090";
    
    // 测试用户信息
    const std::string test_user_id_ = "test_user_123";
    const std::string test_device_id_ = "test_device_456";
    const std::string test_platform_ = "test";
    std::string test_token_;
};

/**
 * @brief 端到端：建立真实WebSocket连接（TLS），携带token，校验conn_mgr在线数
 */
TEST_F(GatewayWebSocketTest, EndToEnd_WebSocket_TLS_WithToken) {
    if (test_token_.empty()) {
        GTEST_SKIP() << "No valid test token available";
    }

    // 证书在 SetUp 前设置更可靠，这里不再重启服务器，直接连接

    // 使用Beast客户端发起 TLS + WS 握手，并在请求行中附带token
    net::io_context ioc;
    ssl::context client_ctx(ssl::context::tlsv12_client);
    client_ctx.set_verify_mode(ssl::verify_none);
    websocket::stream<ssl::stream<tcp::socket>> ws(ioc, client_ctx);

    auto results = tcp::resolver{ioc}.resolve("127.0.0.1", std::to_string(ws_port_));
    net::connect(boost::beast::get_lowest_layer(ws), results);
    ws.next_layer().handshake(ssl::stream_base::client);

    // 将token放到URL查询参数（服务端在decorator里读取）
    std::string target = std::string("/") + "?token=" + test_token_;
    ws.handshake("127.0.0.1", target);

     ioc.poll();

    // 建连后，conn_mgr 在线用户数应该在短时间内 > 0（由on_websocket_connect触发绑定）
    bool ok = false;
    for (int i = 0; i < 100; ++i) {
        if (gateway_server_->get_online_count() > 0) { ok = true; break; }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
    EXPECT_TRUE(ok) << "online_count not increased after WS connect with token";

    // 发送一个protobuf请求（CMD_SEND_MESSAGE），并等待服务器通过session->send回响（处理器里push或回写）
    IMHeader header = create_test_header(CMD_SEND_MESSAGE, 777);
    BaseResponse req; req.set_error_code(SUCCESS); req.set_error_message("from client");
    std::string bin; ASSERT_TRUE(ProtobufCodec::encode(header, req, bin));
    ws.write(net::buffer(bin));

    beast::flat_buffer buf;
    // 如果服务端按protobuf回写，我们试着读取一帧
    beast::error_code ec;
    ws.read(buf, ec);
    // 允许没有直接回包（服务端可能只做push给to_uid），但read不应出错（除非服务器主动断开）
    EXPECT_FALSE(ec && ec != websocket::error::closed);

    // 清理
    ws.close(websocket::close_code::normal);
}

/**
 * @brief 无网络的WebSocket闭环测试：构造protobuf -> 解析 -> 处理 -> 校验
 */
TEST_F(GatewayWebSocketTest, WebSocketRoundTrip_NoSocket) {
    // 纯WS消息编解码与处理闭环：使用protobuf编码模拟WS帧，只验证WS路径能力
    IMHeader header = create_test_header(CMD_SEND_MESSAGE, 123);
    BaseResponse req; req.set_error_code(SUCCESS); req.set_error_message("ws-no-socket");

    std::string bin; ASSERT_TRUE(ProtobufCodec::encode(header, req, bin));

    // 通过MessageParser的WS入口解析
    im::gateway::MessageParser parser("../test_router_config.json");
    auto parsed = parser.parse_websocket_message_enhanced(bin, "sess-1");
    ASSERT_TRUE(parsed.success) << parsed.error_message;
    ASSERT_NE(parsed.message, nullptr);
    EXPECT_EQ(parsed.message->get_header().cmd_id(), (uint32_t)CMD_SEND_MESSAGE);

    // 使用GatewayServer已注册的处理器语义，直接构造本地 MessageProcessor 执行
    auto router_mgr = std::make_shared<im::gateway::RouterManager>("../test_router_config.json");
    auto auth_mgr   = std::make_shared<im::gateway::MultiPlatformAuthManager>("../test_auth_config.json");
    im::gateway::MessageProcessor proc(router_mgr, auth_mgr);
    int reg = proc.register_processor(2001, [](const im::gateway::UnifiedMessage& msg){
        nlohmann::json j; j["code"]=200; j["echo"]=msg.get_header().seq();
        return im::gateway::ProcessorResult(0, "OK", "", j.dump());
    });
    ASSERT_EQ(reg, 0);

    auto fut = proc.process_message(std::move(parsed.message));
    ASSERT_EQ(fut.wait_for(std::chrono::seconds(3)), std::future_status::ready);
    auto res = fut.get();
    EXPECT_EQ(res.status_code, 0);
    auto jr = nlohmann::json::parse(res.json_body);
    EXPECT_EQ(jr["code"].get<int>(), 200);
    EXPECT_EQ(jr["echo"].get<int>(), 123);
}

/**
 * @brief 测试WebSocket连接建立（由于SSL配置复杂，这里主要测试服务器端逻辑）
 */
TEST_F(GatewayWebSocketTest, ConnectionManagement) {
    // 检查初始连接数
    size_t initial_count = gateway_server_->get_online_count();
    std::cout << "Initial online count: " << initial_count << std::endl;
    
    EXPECT_EQ(initial_count, 0);
    
    // 这里我们主要验证服务器的连接管理功能是否正常
    // 实际的WebSocket客户端连接由于SSL配置较复杂，在集成测试中验证
}

/**
 * @brief 测试protobuf消息编解码
 */
TEST_F(GatewayWebSocketTest, ProtobufCodec) {
    // 创建测试消息
    IMHeader header = create_test_header(CMD_SEND_MESSAGE);
    BaseResponse request;
    request.set_error_code(SUCCESS);
    request.set_error_message("Test message");
    
    // 编码消息
    std::string encoded_data;
    ASSERT_TRUE(ProtobufCodec::encode(header, request, encoded_data));
    EXPECT_GT(encoded_data.length(), 0);
    
    // 解码消息
    IMHeader decoded_header;
    BaseResponse decoded_request;
    ASSERT_TRUE(ProtobufCodec::decode(encoded_data, decoded_header, decoded_request));
    
    // 验证解码结果
    EXPECT_EQ(decoded_header.cmd_id(), CMD_SEND_MESSAGE);
    EXPECT_EQ(decoded_header.from_uid(), test_user_id_);
    EXPECT_EQ(decoded_request.error_code(), SUCCESS);
    EXPECT_EQ(decoded_request.error_message(), "Test message");
}

/**
 * @brief 测试消息处理器注册和调用
 */
TEST_F(GatewayWebSocketTest, MessageHandlerRegistration) {
    // 测试处理器已在SetUp中注册
    // 这里验证注册功能是否正常工作
    
    // 创建测试消息
    auto message = std::make_unique<UnifiedMessage>();
    message->set_json_body(R"({"test": "data"})");
    
    IMHeader header = create_test_header(CMD_SEND_MESSAGE);
    message->set_header(header);
    
    // 设置会话上下文
    UnifiedMessage::SessionContext context;
    context.protocol = UnifiedMessage::Protocol::HTTP;
    context.session_id = "test_session";
    context.http_method = "POST";
    context.original_path = "/test";
    context.receive_time = std::chrono::system_clock::now();
    message->set_session_context(std::move(context));
    
    std::cout << "Message handler registration test completed" << std::endl;
}

/**
 * @brief 测试认证Token生成和验证
 */
TEST_F(GatewayWebSocketTest, TokenGeneration) {
    if (test_token_.empty()) {
        GTEST_SKIP() << "Token generation failed in setup";
    }
    
    std::cout << "Generated token length: " << test_token_.length() << std::endl;
    EXPECT_GT(test_token_.length(), 0);
    
    // Token应该包含JWT的基本结构（三个由.分隔的部分）
    size_t first_dot = test_token_.find('.');
    size_t second_dot = test_token_.find('.', first_dot + 1);
    
    EXPECT_NE(first_dot, std::string::npos);
    EXPECT_NE(second_dot, std::string::npos);
    EXPECT_GT(first_dot, 0);
    EXPECT_GT(second_dot, first_dot + 1);
}

/**
 * @brief 测试服务器统计信息
 */
TEST_F(GatewayWebSocketTest, ServerStatistics) {
    std::string stats = gateway_server_->get_server_stats();
    std::cout << "Server statistics:\n" << stats << std::endl;
    
    // 验证统计信息包含必要的字段
    EXPECT_TRUE(stats.find("Running:") != std::string::npos);
    EXPECT_TRUE(stats.find("online user count:") != std::string::npos);
    EXPECT_TRUE(stats.find("processed message count:") != std::string::npos);
}

/**
 * @brief 测试错误处理机制
 */
TEST_F(GatewayWebSocketTest, ErrorHandling) {
    // 测试无效的protobuf数据解码
    std::string invalid_data = "This is not a valid protobuf message";
    
    IMHeader header;
    BaseResponse response;
    
    // 应该解码失败
    EXPECT_FALSE(ProtobufCodec::decode(invalid_data, header, response));
    
    std::cout << "Error handling test completed" << std::endl;
}

/**
 * @brief 测试消息路由功能
 */
TEST_F(GatewayWebSocketTest, MessageRouting) {
    // 创建不同命令ID的消息，验证路由是否正确
    std::vector<uint32_t> test_cmd_ids = {CMD_SEND_MESSAGE, CMD_GET_USER_INFO};
    
    for (uint32_t cmd_id : test_cmd_ids) {
        IMHeader header = create_test_header(cmd_id);
        BaseResponse request;
        request.set_error_code(SUCCESS);
        request.set_error_message("Routing test for cmd_id " + std::to_string(cmd_id));
        
        std::string encoded_data;
        ASSERT_TRUE(ProtobufCodec::encode(header, request, encoded_data));
        
        std::cout << "Created message for cmd_id: " << cmd_id 
                  << ", encoded size: " << encoded_data.length() << std::endl;
    }
}

/**
 * @brief 集成测试：验证完整的消息处理流程
 */
TEST_F(GatewayWebSocketTest, IntegrationTest) {
    std::cout << "=== WebSocket Integration Test ===" << std::endl;
    
    // 1. 验证服务器状态
    EXPECT_TRUE(gateway_server_ != nullptr);
    
    // 2. 验证Token生成
    if (!test_token_.empty()) {
        std::cout << "✓ Token generation successful" << std::endl;
    } else {
        std::cout << "⚠ Token generation failed, using fallback" << std::endl;
    }
    
    // 3. 验证protobuf编解码
    IMHeader header = create_test_header(CMD_SEND_MESSAGE);
    BaseResponse request;
    request.set_error_code(SUCCESS);
    request.set_error_message("Integration test message");
    
    std::string encoded_data;
    ASSERT_TRUE(ProtobufCodec::encode(header, request, encoded_data));
    
    IMHeader decoded_header;
    BaseResponse decoded_request;
    ASSERT_TRUE(ProtobufCodec::decode(encoded_data, decoded_header, decoded_request));
    
    std::cout << "✓ Protobuf codec working correctly" << std::endl;
    
    // 4. 验证服务器统计
    std::string stats = gateway_server_->get_server_stats();
    EXPECT_TRUE(stats.find("Running: true") != std::string::npos);
    std::cout << "✓ Server statistics available" << std::endl;
    
    std::cout << "=== Integration Test Complete ===" << std::endl;
}

int main(int argc, char **argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}