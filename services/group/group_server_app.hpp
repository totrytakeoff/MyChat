#ifndef IM_SERVICE_GROUP_GROUP_SERVER_APP_HPP
#define IM_SERVICE_GROUP_GROUP_SERVER_APP_HPP

#include <memory>
#include <string>

#include <grpcpp/server.h>
#include <spdlog/logger.h>

#include "group_grpc_service.hpp"
#include "group_message_service.hpp"
#include "group_service.hpp"

namespace odb::pgsql {
class database;
}

namespace im::service::user {
class UserService;
}

namespace im::service::group {

struct GroupServerConfig {
    std::string listen_address = "0.0.0.0:9004";
    std::string postgres_connection_string;
};

class GroupServerApp {
public:
    explicit GroupServerApp(GroupServerConfig config);
    ~GroupServerApp();

    GroupServerApp(const GroupServerApp&) = delete;
    GroupServerApp& operator=(const GroupServerApp&) = delete;

    bool start();
    void wait();
    void shutdown();

    const std::string& listen_address() const { return config_.listen_address; }
    int selected_port() const { return selected_port_; }
    bool is_running() const { return static_cast<bool>(server_); }

private:
    GroupServerConfig config_;
    std::shared_ptr<odb::pgsql::database> db_;
    std::shared_ptr<im::service::user::UserService> user_service_;
    std::shared_ptr<GroupService> group_service_;
    std::unique_ptr<GroupMessageService> group_message_service_;
    std::unique_ptr<GroupGrpcService> grpc_service_;
    std::unique_ptr<::grpc::Server> server_;
    std::shared_ptr<spdlog::logger> logger_;
    int selected_port_ = 0;
};

}  // namespace im::service::group

#endif  // IM_SERVICE_GROUP_GROUP_SERVER_APP_HPP
