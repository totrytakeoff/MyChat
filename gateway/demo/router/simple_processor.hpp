#ifndef SIMPLE_PROCESSOR_HPP
#define SIMPLE_PROCESSOR_HPP

/******************************************************************************
 *
 * @file       simple_processor.hpp
 * @brief      简单的请求处理器
 *
 * @author     myself
 * @date       2025/01/20
 *
 *****************************************************************************/

#include "simple_router.hpp"
#include "simple_message.hpp"
#include <httplib.h>
#include <future>
#include <functional>

namespace im {
namespace gateway {

/**
 * @class SimpleProcessor
 * @brief 简单的HTTP请求处理器
 */
class SimpleProcessor {
public:
    // 异步处理函数类型
    using AsyncHandler = std::function<SimpleResponse(const SimpleMessage&)>;

    /**
     * @brief 构造函数
     */
    SimpleProcessor();

    /**
     * @brief 加载路由配置
     * @param config_file 配置文件路径
     * @return 是否成功
     */
    bool load_config(const std::string& config_file);

    /**
     * @brief 设置异步处理函数
     * @param handler 处理函数
     */
    void set_async_handler(AsyncHandler handler);

    /**
     * @brief 处理HTTP请求（这是主要的处理函数）
     * @param req HTTP请求
     * @param res HTTP响应
     */
    void handle_request(const httplib::Request& req, httplib::Response& res);

private:
    /**
     * @brief 从HTTP请求提取信息，封装成SimpleMessage
     * @param req HTTP请求
     * @param route_info 路由信息
     * @return 封装后的消息
     */
    SimpleMessage extract_message_from_request(const httplib::Request& req, 
                                              const SimpleRouter::RouteInfo& route_info);

    /**
     * @brief 提取Token
     * @param req HTTP请求
     * @return Token字符串
     */
    std::string extract_token(const httplib::Request& req);

    /**
     * @brief 提取设备ID
     * @param req HTTP请求  
     * @return 设备ID
     */
    std::string extract_device_id(const httplib::Request& req);

    /**
     * @brief 提取平台信息
     * @param req HTTP请求
     * @return 平台信息
     */
    std::string extract_platform(const httplib::Request& req);

    /**
     * @brief 提取客户端IP
     * @param req HTTP请求
     * @return 客户端IP
     */
    std::string extract_client_ip(const httplib::Request& req);

    /**
     * @brief 生成会话ID
     * @return 会话ID
     */
    std::string generate_session_id();

private:
    SimpleRouter router_;           // 路由器
    AsyncHandler async_handler_;    // 异步处理函数
    static int session_counter_;    // 会话计数器
};

}  // namespace gateway
}  // namespace im

#endif  // SIMPLE_PROCESSOR_HPP