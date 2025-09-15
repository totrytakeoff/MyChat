#include <chrono>
#include <iostream>
#include <memory>
#include <thread>

#include <boost/asio/connect.hpp>
#include <boost/asio/ip/tcp.hpp>
#include <boost/asio/ssl.hpp>
#include <boost/asio/ssl/context.hpp>
#include <boost/asio/ssl/stream.hpp>
#include <boost/beast/core.hpp>
#include <boost/beast/websocket.hpp>
#include <boost/beast/websocket/ssl.hpp>
#include <iostream>
#include <string>
#include <cstdlib>

#include <google/protobuf/message.h>

#include "../../common/database/redis_mgr.hpp"
#include "../../common/network/protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"
#include "../../gateway/gateway_server/gateway_server.hpp"
#include "boost/asio/ssl/verify_mode.hpp"
#include "boost/beast/core/buffers_to_string.hpp"
#include "boost/beast/core/flat_buffer.hpp"

using namespace std;
using nlohmann::json;
using namespace boost::asio;
namespace beast = boost::beast;          // from <boost/beast.hpp>
namespace http = beast::http;            // from <boost/beast/http.hpp>
namespace websocket = beast::websocket;  // from <boost/beast/websocket.hpp>
namespace net = boost::asio;             // from <boost/asio.hpp>
using tcp = net::ip::tcp;
using namespace im::base;  // from <boost/asio/ip/tcp.hpp>
int main() {
    try {
        // Ensure Redis is healthy with correct credentials for this environment
        im::db::RedisConfig redis_cfg;
        redis_cfg.host = "127.0.0.1";
        redis_cfg.port = 6379;
        redis_cfg.password = "myself";  // adjust to your local redis password
        redis_cfg.db = 1;
        im::db::RedisManager::GetInstance().initialize(redis_cfg);

        uint16_t ws_port = 8101;
        uint16_t http_port = 8102;
        const std::string platform_cfg = "../test_auth_config.json";
        const std::string router_cfg = "../test_router_config.json";

        auto auth_mgr = std::make_shared<im::gateway::MultiPlatformAuthManager>(platform_cfg);

        // Ensure WS cert/key are available for the gateway's WSS listener
        if (!std::getenv("WS_CERT")) {
            setenv("WS_CERT", "/home/myself/workspace/MyChat/test/network/test_cert.pem", 1);
        }
        if (!std::getenv("WS_KEY")) {
            setenv("WS_KEY", "/home/myself/workspace/MyChat/test/network/test_key.pem", 1);
        }


        auto server = std::make_shared<im::gateway::GatewayServer>(platform_cfg, router_cfg,
                                                                   ws_port, http_port);


        server->register_message_handlers(
                1004, [](const im::gateway::UnifiedMessage& msg) -> im::gateway::ProcessorResult {
                    json j;
                    j["code"] = 200;
                    j["body"] = "admin user info";
                    j["err_msg"] = "ok";
                    return im::gateway::ProcessorResult(0, "OK", "", j.dump());
                });
        server->register_message_handlers(
                1005, [](const im::gateway::UnifiedMessage& msg) -> im::gateway::ProcessorResult {
                    google::protobuf::Message* messagess = google::protobuf::ParseFromString(msg.get_protobuf_payload());
                    auto message = static_cast<const im::base::BaseRequest*>(messagess);

                    std::cout << "--receive message: " << message->payload() << std::endl;

                    im::base::IMHeader header = im::network::ProtobufCodec::returnHeaderBuilder(
                            msg.get_header(), "gateway_server", "linux_x86_64");

                    std::string output;
                    im::base::BaseResponse response;
                    response.set_error_code(im::base::ErrorCode::SUCCESS);
                    response.set_payload(" hello user!");
                    im::network::ProtobufCodec::encode(header, response, output);

                    return im::gateway::ProcessorResult(0, "OK", output, "");
                });

        server->start();

        auto token = auth_mgr->generate_access_token("1", "admin", "test-dev", "Windows", 600);
        httplib::Client cli("127.0.0.1", http_port);
        cli.set_keep_alive(false);
        cli.set_connection_timeout(2, 0);
        cli.set_read_timeout(30, 0);  // 增加超时时间至30秒，确保服务器响应
        cli.set_write_timeout(5, 0);

        // 注意这里的headers，需要设置Authorization，X-Device-ID，X-Platform三个字段进行验证
        httplib::Headers headerss = {{"Authorization", std::string("Bearer ") + token},
                                     {"X-Device-ID", "test-dev"},
                                     {"X-Platform", "Windows"}};

        std::cout << "Gateway server running on http://127.0.0.1:" << http_port << std::endl;
        std::cout << "Press Ctrl+C to exit" << std::endl;
        std::cout << "temp token: " << token << std::endl;

        net::io_context ioc;
        // SSL 上下文（测试环境，禁用证书校验以避免自签名证书报错）
        net::ssl::context ctx{net::ssl::context::tlsv12_client};
        ctx.set_verify_mode(ssl::verify_none);
        // 如需校验证书，可改为 verify_peer 并加载受信任的CA或服务端证书
        // ctx.set_default_verify_paths();
        // ctx.load_verify_file("/home/myself/workspace/MyChat/test/network/test_cert.pem");
        // TCP 解析与连接
        tcp::resolver resolver{ioc};
        websocket::stream<net::ssl::stream<tcp::socket>> ws{ioc, ctx};

        const char* host = "127.0.0.1";
        const char* port = "8101";

        // 解析并连接
        auto const results = resolver.resolve(host, port);
        net::connect(boost::beast::get_lowest_layer(ws), results);

        // 可选：设置SNI主机名（有些服务端/库需要）
        if (!SSL_set_tlsext_host_name(ws.next_layer().native_handle(), host)) {
            beast::error_code ec{static_cast<int>(::ERR_get_error()),
                                 net::error::get_ssl_category()};
            std::cerr << "Error setting SNI host: " << ec.message() << std::endl;
        }

        // TLS 握手
        ws.next_layer().handshake(net::ssl::stream_base::client);

        // WebSocket 握手（将token放到URL查询参数，服务端在decorator里读取）
        std::string target = std::string("/") + "?token=" + token;
        ws.handshake(host, target);
        ws.binary(true); // 使用二进制帧传输protobuf

        im::base::IMHeader headers;
        headers.set_version("1");
        headers.set_cmd_id(1005);
        headers.set_seq(1);
        headers.set_timestamp(std::chrono::duration_cast<std::chrono::milliseconds>(
                                      std::chrono::system_clock::now().time_since_epoch())
                                      .count());
        headers.set_from_uid("1");
        headers.set_to_uid("0");
        headers.set_device_id("test-dev");
        headers.set_platform("Windows");
        headers.set_token(token);

        std::string output;
        // 注意：当前服务端解析期望 BaseResponse 类型，这里使用 BaseResponse 进行发送
        std::shared_ptr<im::base::BaseRequest> out_msg = std::make_shared<im::base::BaseRequest>();
        out_msg->set_payload("hello server!");

        im::network::ProtobufCodec::encode(headers, *out_msg, output);

        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            std::cout << "send message bytes: " << output.size() << std::endl;
            ws.write(net::buffer(output));
            std::this_thread::sleep_for(std::chrono::seconds(1));
            beast::flat_buffer input;
            ws.read(input);
            std::string received_data = beast::buffers_to_string(input.data());
            BaseResponse response;
            IMHeader header;
            if (!im::network::ProtobufCodec::decode(received_data, header, response)) {
                std::cout << "decode failed, bytes: " << received_data.size() << std::endl;
                continue;
            }
            std::cout << "receive payload: " << response.payload() << std::endl;
            std::cout << "receive err_message: " << response.error_message() << std::endl;
            std::cout << "receive err_code: " << response.error_code() << std::endl;
            // auto res = cli.Post("/api/v1/auth/info", headers, "{}", "application/json");
            // cout << "Response status: " << (res ? res->status : -1) << endl;
            // if (res && res->body.size() > 0) cout << "Response body: " << res->body << endl;

            // if (!res) {
            //     std::cout << "http req no response!" << std::endl;
            // }
        }
        server->stop();
        

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
