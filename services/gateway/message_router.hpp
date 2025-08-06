#ifndef MESSAGE_ROUTER_HPP
#define MESSAGE_ROUTER_HPP

/******************************************************************************
 *
 * @file       message_router.hpp
 * @brief      消息路由器，负责解析和路由消息
 *
 * @author     MyChat Development Team
 * @date       2025/08/06
 *
 *****************************************************************************/

#include <memory>
#include <string>

#include "common/network/websocket_session.hpp"
#include "common/proto/base.pb.h"
#include "common/network/protobuf_codec.hpp"

namespace im {
namespace gateway {

using SessionPtr = network::SessionPtr;

// 前置声明
class ServiceCommunicator;

class MessageRouter {
public:
    /**
     * @brief 路由消息
     * @param session WebSocket会话
     * @param raw_message 原始消息数据
     */
    void route_message(SessionPtr session, const std::string& raw_message);
    
private:
    /**
     * @brief 处理认证消息
     * @param session WebSocket会话
     * @param header 消息头
     * @param message 消息体
     */
    void handle_auth_message(SessionPtr session, const im::base::IMHeader& header, 
                           const google::protobuf::Message& message);
    
    /**
     * @brief 处理心跳消息
     * @param session WebSocket会话
     */
    void handle_heartbeat_message(SessionPtr session);
    
    /**
     * @brief 路由到后端服务
     * @param header 消息头
     * @param raw_message 原始消息数据
     */
    void route_to_service(const im::base::IMHeader& header, const std::string& raw_message);
    
private:
    std::unique_ptr<ServiceCommunicator> service_communicator_;
};

} // namespace gateway
} // namespace im

#endif // MESSAGE_ROUTER_HPP