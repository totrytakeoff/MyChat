#ifndef IM_SERVICE_USER_USER_SERVER_APP_HPP
#define IM_SERVICE_USER_USER_SERVER_APP_HPP

#include <memory>
#include <string>

#include <grpcpp/server.h>
#include <spdlog/logger.h>

#include "user_grpc_service.hpp"
#include "user_service.hpp"

namespace odb::pgsql {
class database;
}

namespace im::service::user {

struct UserServerConfig {
    std::string listen_address = "0.0.0.0:9001";
    std::string postgres_connection_string;
};

class UserServerApp {
public:
    explicit UserServerApp(UserServerConfig config);
    ~UserServerApp();

    UserServerApp(const UserServerApp&) = delete;
    UserServerApp& operator=(const UserServerApp&) = delete;

    bool start();
    void wait();
    void shutdown();

    const std::string& listen_address() const { return config_.listen_address; }
    int selected_port() const { return selected_port_; }
    bool is_running() const { return static_cast<bool>(server_); }

private:
    UserServerConfig config_;
    std::shared_ptr<odb::pgsql::database> db_;
    std::unique_ptr<UserService> user_service_;
    std::unique_ptr<UserGrpcService> grpc_service_;
    std::unique_ptr<::grpc::Server> server_;
    std::shared_ptr<spdlog::logger> logger_;
    int selected_port_ = 0;
};

}  // namespace im::service::user

#endif  // IM_SERVICE_USER_USER_SERVER_APP_HPP
