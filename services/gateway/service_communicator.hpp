#ifndef SERVICE_COMMUNICATOR_HPP
#define SERVICE_COMMUNICATOR_HPP

/******************************************************************************
 *
 * @file       service_communicator.hpp
 * @brief      服务通信器，负责与后端微服务通信
 *
 * @author     MyChat Development Team
 * @date       2025/08/06
 *
 *****************************************************************************/

#include <string>
#include <memory>

namespace im {
namespace gateway {

class ServiceCommunicator {
public:
    /**
     * @brief 发送消息到后端服务
     * @param service_name 服务名称
     * @param message 消息内容
     */
    void send_to_service(const std::string& service_name, const std::string& message);
    
    /**
     * @brief 处理来自后端服务的响应
     * @param response 响应内容
     */
    void handle_service_response(const std::string& response);
    
private:
    // 可以使用ConnectionPool或其他连接管理机制
    // std::unique_ptr<ConnectionPool> connection_pool_;
};

} // namespace gateway
} // namespace im

#endif // SERVICE_COMMUNICATOR_HPP