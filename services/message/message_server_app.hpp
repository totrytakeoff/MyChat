#ifndef IM_SERVICE_MESSAGE_MESSAGE_SERVER_APP_HPP
#define IM_SERVICE_MESSAGE_MESSAGE_SERVER_APP_HPP

#include <memory>
#include <string>

#include <grpcpp/server.h>
#include <spdlog/logger.h>

#include "message_grpc_service.hpp"
#include "message_service.hpp"

namespace odb::pgsql {
class database;
}

namespace im::service::message {

struct MessageServerConfig {
    std::string listen_address = "0.0.0.0:9002";
    std::string postgres_connection_string;
};

class MessageServerApp {
public:
    explicit MessageServerApp(MessageServerConfig config);
    ~MessageServerApp();

    MessageServerApp(const MessageServerApp&) = delete;
    MessageServerApp& operator=(const MessageServerApp&) = delete;

    bool start();
    void wait();
    void shutdown();

    const std::string& listen_address() const { return config_.listen_address; }
    int selected_port() const { return selected_port_; }
    bool is_running() const { return static_cast<bool>(server_); }

private:
    MessageServerConfig config_;
    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<MessageService> message_service_;
    std::unique_ptr<MessageGrpcService> grpc_service_;
    std::unique_ptr<::grpc::Server> server_;
    std::shared_ptr<spdlog::logger> logger_;
    int selected_port_ = 0;
};

}  // namespace im::service::message

#endif  // IM_SERVICE_MESSAGE_MESSAGE_SERVER_APP_HPP
