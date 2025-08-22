#ifndef SIMPLE_ROUTER_HPP
#define SIMPLE_ROUTER_HPP

/******************************************************************************
 *
 * @file       simple_router.hpp  
 * @brief      简单的HTTP路由器 - 新手友好版本
 *
 * @author     myself
 * @date       2025/01/20
 *
 *****************************************************************************/

#include <string>
#include <unordered_map>
#include <nlohmann/json.hpp>

namespace im {
namespace gateway {

/**
 * @class SimpleRouter
 * @brief 简单的HTTP路由器，专注基础功能
 */
class SimpleRouter {
public:
    /**
     * @struct RouteInfo
     * @brief 路由信息
     */
    struct RouteInfo {
        uint32_t cmd_id;           // 命令ID
        std::string service_name;  // 服务名称
        bool found;                // 是否找到匹配路由
        
        RouteInfo() : cmd_id(0), found(false) {}
    };

    /**
     * @brief 构造函数
     */
    SimpleRouter();

    /**
     * @brief 加载路由配置文件
     * @param config_file 配置文件路径
     * @return 是否成功
     */
    bool load_config(const std::string& config_file);

    /**
     * @brief 根据URL路径查找路由
     * @param path URL路径，如：/api/v1/auth/login
     * @return 路由信息
     */
    RouteInfo find_route(const std::string& path);

    /**
     * @brief 打印所有路由（调试用）
     */
    void print_routes();

private:
    std::string api_prefix_;                              // API前缀，如：/api/v1
    std::unordered_map<std::string, uint32_t> routes_;    // 路径 -> 命令ID映射
    std::unordered_map<std::string, std::string> services_;  // 路径 -> 服务名映射
};

}  // namespace gateway
}  // namespace im

#endif  // SIMPLE_ROUTER_HPP