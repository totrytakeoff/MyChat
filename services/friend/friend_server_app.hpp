#ifndef IM_SERVICE_FRIEND_FRIEND_SERVER_APP_HPP
#define IM_SERVICE_FRIEND_FRIEND_SERVER_APP_HPP

#include <memory>
#include <string>

#include <grpcpp/server.h>
#include <spdlog/logger.h>

#include "friend_grpc_service.hpp"
#include "friend_service.hpp"

namespace odb::pgsql {
class database;
}

namespace im::service::user {
class UserService;
}

namespace im::service::friend_ {

struct FriendServerConfig {
    std::string listen_address = "0.0.0.0:9003";
    std::string postgres_connection_string;
};

class FriendServerApp {
public:
    explicit FriendServerApp(FriendServerConfig config);
    ~FriendServerApp();

    FriendServerApp(const FriendServerApp&) = delete;
    FriendServerApp& operator=(const FriendServerApp&) = delete;

    bool start();
    void wait();
    void shutdown();

    const std::string& listen_address() const { return config_.listen_address; }
    int selected_port() const { return selected_port_; }
    bool is_running() const { return static_cast<bool>(server_); }

private:
    FriendServerConfig config_;
    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<im::service::user::UserService> user_service_;
    std::unique_ptr<FriendService> friend_service_;
    std::unique_ptr<FriendGrpcService> grpc_service_;
    std::unique_ptr<::grpc::Server> server_;
    std::shared_ptr<spdlog::logger> logger_;
    int selected_port_ = 0;
};

}  // namespace im::service::friend_

#endif  // IM_SERVICE_FRIEND_FRIEND_SERVER_APP_HPP
