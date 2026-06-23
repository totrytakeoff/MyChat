#ifndef IM_SERVICE_USER_USER_GRPC_SERVICE_HPP
#define IM_SERVICE_USER_USER_GRPC_SERVICE_HPP

#include <grpcpp/support/status.h>

#include "user.grpc.pb.h"

namespace grpc {
class ServerContext;
}

namespace im::service::user {

class UserService;

class UserGrpcService final : public im::user::UserService::Service {
public:
    explicit UserGrpcService(UserService* user_service);

    ::grpc::Status ForwardPacket(::grpc::ServerContext* context,
                                 const im::user::UserPacketRequest* request,
                                 im::user::UserPacketResponse* response) override;

    ::grpc::Status Register(::grpc::ServerContext* context,
                            const im::user::RegisterRequest* request,
                            im::user::RegisterResponse* response) override;

    ::grpc::Status Login(::grpc::ServerContext* context,
                         const im::user::LoginRequest* request,
                         im::user::LoginResponse* response) override;

    ::grpc::Status GetUserInfo(::grpc::ServerContext* context,
                               const im::user::GetUserInfoRequest* request,
                               im::user::GetUserInfoResponse* response) override;

    ::grpc::Status SearchUsers(::grpc::ServerContext* context,
                               const im::user::SearchUsersRequest* request,
                               im::user::SearchUsersResponse* response) override;

    ::grpc::Status UpdateUserInfo(::grpc::ServerContext* context,
                                  const im::user::UpdateUserInfoRequest* request,
                                  im::user::UpdateUserInfoResponse* response) override;

private:
    UserService* user_service_;
};

}  // namespace im::service::user

#endif  // IM_SERVICE_USER_USER_GRPC_SERVICE_HPP
