#ifndef ROUTER_MGR_HPP
#define ROUTER_MGR_HPP

/******************************************************************************
 *
 * @file       router_mgr.hpp
 * @brief      统一路由管理器 - 负责HTTP路由解析和服务发现
 *
 * @details    本模块提供完整的路由管理功能，包括：
 *             1. HTTP路由解析：将HTTP请求URL映射到命令ID和服务名
 *             2. 服务路由发现：将服务名映射到具体的服务端点
 *             3. 完整路由流程：从HTTP请求直接获取服务端点信息
 *             4. 配置管理：支持动态重新加载路由配置
 *
 * @author     myself
 * @date       2025/08/21
 * @version    1.0.0
 *
 *****************************************************************************/

#include "../../common/proto/base.pb.h"
#include "../../common/utils/global.hpp"

#include <httplib.h>
#include <memory>
#include <stdexcept>
#include <string>
#include <unordered_map>



namespace im {
namespace gateway {

// 为std::pair<uint32_t, uint32_t>定义哈希函数
struct PairHash {
    std::size_t operator()(const std::pair<uint32_t, uint32_t>& p) const {
        // 使用std::hash组合两个值
        auto h1 = std::hash<uint32_t>{}(p.first);
        auto h2 = std::hash<uint32_t>{}(p.second);
        return h1 ^ (h2 << 1);
    }
};

/**
 * @brief HTTP路由解析结果
 *
 * @details 封装HTTP路由解析的结果，包含命令ID、服务名称和状态信息
 */
struct HttpRouteResult {
    uint32_t cmd_id;           ///< 命令ID，用于标识具体的业务操作
    std::string service_name;  ///< 服务名称，用于后续服务发现
    bool is_valid;             ///< 路由是否有效
    std::string err_msg;       ///< 错误信息（当is_valid为false时）
    int status_code;           ///< HTTP状态码

    /**
     * @brief 默认构造函数 - 表示失败状态
     *
     * @details 创建一个表示路由失败的结果对象，默认状态码为404
     */
    HttpRouteResult()
            : cmd_id(0), service_name(""), is_valid(false), err_msg(""), status_code(404) {}

    /**
     * @brief 成功构造函数
     *
     * @param id 命令ID
     * @param service 服务名称
     *
     * @details 创建一个表示路由成功的结果对象，默认状态码为200
     */
    HttpRouteResult(uint32_t id, const std::string& service)
            : cmd_id(id), service_name(service), is_valid(true), err_msg(""), status_code(200) {}
};

/**
 * @brief 服务路由发现结果
 *
 * @details 封装服务发现的结果，包含服务端点信息和连接配置
 */
struct ServiceRouteResult {
    std::string service_name;  ///< 服务名称
    std::string endpoint;      ///< 服务端点 (host:port格式)
    uint32_t timeout_ms;       ///< 请求超时时间(毫秒)
    uint32_t max_connections;  ///< 最大连接数
    bool is_valid;             ///< 服务信息是否有效
    std::string err_msg;       ///< 错误信息（当is_valid为false时）
    // std::pair<uint32_t, uint32_t> cmd_range;  ///< 支持的命令ID范围 [min, max]

    /**
     * @brief 默认构造函数 - 表示失败状态
     *
     * @details 创建一个表示服务发现失败的结果对象，使用默认配置
     */
    ServiceRouteResult() : timeout_ms(3000), max_connections(10), is_valid(false) {}

    /**
     * @brief 成功构造函数
     *
     * @param name 服务名称
     * @param ep 服务端点
     * @param timeout 超时时间（毫秒）
     * @param max_conn 最大连接数
     *
     * @details 创建一个表示服务发现成功的结果对象
     */
    ServiceRouteResult(const std::string& name, const std::string& ep, uint32_t timeout,
                       uint32_t max_conn)
            : service_name(name)
            , endpoint(ep)
            , timeout_ms(timeout)
            , max_connections(max_conn)
            , is_valid(true) {}
};

/**
 * @brief HTTP路由器
 *
 * @details 负责HTTP请求的路由解析，将URL路径映射到命令ID和服务名称
 *          支持API前缀配置和动态配置加载
 */
class HttpRouter {
public:
    /**
     * @brief 构造函数
     *
     * @details 创建HTTP路由器实例，需要后续调用load_config加载配置
     */
    explicit HttpRouter();

    /**
     * @brief 加载路由配置
     *
     * @param config_file 配置文件路径
     * @return true 加载成功，false 加载失败
     *
     * @details 从JSON配置文件中加载HTTP路由规则，支持容错处理
     */
    bool load_config(const std::string& config_file);

    /**
     * @brief 解析HTTP路由
     *
     * @param method HTTP请求方法（GET、POST等）
     * @param path HTTP请求路径
     * @return 路由解析结果
     *
     * @details 根据请求路径查找对应的命令ID和服务名称
     */
    std::unique_ptr<HttpRouteResult> parse_route(const std::string& method,
                                                 const std::string& path);

    /**
     * @brief 获取路由数量
     *
     * @return 当前加载的路由规则数量
     */
    size_t get_route_count() const { return routes_.size(); }

private:
    std::string api_prefix_;                                   ///< API前缀，默认为"/api/v1"
    std::unordered_map<std::string, HttpRouteResult> routes_;  ///< 路由映射表：路径 -> 路由结果
};

/**
 * @brief 服务路由器
 *
 * @details 负责服务发现，将服务名称映射到具体的服务端点信息
 *          包含服务地址、超时配置、连接数限制等
 */
class ServiceRouter {
public:
    /**
     * @brief 构造函数
     *
     * @details 创建服务路由器实例，需要后续调用load_config加载配置
     */
    explicit ServiceRouter();

    /**
     * @brief 加载服务配置
     *
     * @param config_file 配置文件路径
     * @return true 加载成功，false 加载失败
     *
     * @details 从JSON配置文件中加载服务端点信息，支持容错处理
     */
    bool load_config(const std::string& config_file);

    /**
     * @brief 查找服务端点
     *
     * @param service_name 服务名称
     * @param service_id 服务ID
     * @return 服务发现结果
     *
     * @details 根据服务名称查找对应的端点信息和配置参数
     */
    std::unique_ptr<ServiceRouteResult> find_service(const std::string& service_name);


    std::unique_ptr<ServiceRouteResult> find_service(uint32_t service_id);

    /**
     * @brief 获取服务数量
     *
     * @return 当前加载的服务数量
     */
    size_t get_service_count() const { return services_.size(); }

private:
    std::unordered_map<std::string, ServiceRouteResult>
            services_;  ///< 服务映射表：服务名 -> 服务信息

    std::unordered_map<std::pair<uint32_t, uint32_t>, std::string, PairHash>
            cmds_{};  ///< cmd映射表 cmdRange -> 服务名
};

/**
 * @brief 统一路由管理器
 *
 * @details 整合HTTP路由器和服务路由器，提供完整的路由管理功能
 *          支持从HTTP请求直接获取完整的服务端点信息，简化上层调用
 */
class RouterManager {
public:
    /**
     * @brief 构造函数
     *
     * @param config_file 配置文件路径
     *
     * @details 创建路由管理器实例，自动初始化HTTP路由器和服务路由器
     */
    explicit RouterManager(const std::string& config_file);

    /**
     * @brief 重新加载配置
     *
     * @return true 加载成功，false 加载失败
     *
     * @details 重新加载HTTP路由配置和服务配置，支持动态更新
     */
    bool reload_config();

    /**
     * @brief HTTP路由解析
     *
     * @param method HTTP请求方法
     * @param path HTTP请求路径
     * @return HTTP路由解析结果
     *
     * @details 将HTTP请求路径解析为命令ID和服务名称
     */
    std::unique_ptr<HttpRouteResult> parse_http_route(const std::string& method,
                                                      const std::string& path);

    /**
     * @brief 服务路由查找
     *
     * @param service_name 服务名称
     * @param service_id 服务ID
     * @return 服务发现结果
     *
     * @details 根据服务名称查找服务端点信息
     */
    std::unique_ptr<ServiceRouteResult> find_service(const std::string& service_name);

    std::unique_ptr<ServiceRouteResult> find_service(uint32_t service_id);



    /**
     * @brief 完整路由结果
     *
     * @details 包含从HTTP请求到服务端点的完整路由信息
     */
    struct CompleteRouteResult {
        uint32_t cmd_id;           ///< 命令ID
        std::string service_name;  ///< 服务名称
        std::string endpoint;      ///< 服务端点
        uint32_t timeout_ms;       ///< 超时时间（毫秒）
        uint32_t max_connections;  ///< 最大连接数
        bool is_valid;             ///< 路由是否成功
        std::string err_msg;       ///< 错误信息
        int http_status_code;      ///< HTTP状态码

        /**
         * @brief 默认构造函数
         *
         * @details 创建失败状态的完整路由结果
         */
        CompleteRouteResult()
                : cmd_id(0)
                , timeout_ms(3000)
                , max_connections(10)
                , is_valid(false)
                , http_status_code(404) {}
    };

    /**
     * @brief 完整路由处理
     *
     * @param method HTTP请求方法
     * @param path HTTP请求路径
     * @return 完整路由结果
     *
     * @details 一站式处理：HTTP路由解析 + 服务发现，直接返回服务端点信息
     */
    std::unique_ptr<CompleteRouteResult> route_request(const std::string& method,
                                                       const std::string& path);

    /**
     * @brief 路由管理器统计信息
     */
    struct RouterStats {
        size_t http_route_count;  ///< HTTP路由数量
        size_t service_count;     ///< 服务数量
        std::string config_file;  ///< 配置文件路径
    };

    /**
     * @brief 获取统计信息
     *
     * @return 路由管理器统计信息
     *
     * @details 返回当前加载的路由和服务数量等统计信息
     */
    RouterStats get_stats() const;

private:
    std::string config_file_;                        ///< 配置文件路径
    std::unique_ptr<HttpRouter> http_router_;        ///< HTTP路由器实例
    std::unique_ptr<ServiceRouter> service_router_;  ///< 服务路由器实例
};



};  // namespace gateway
};  // namespace im



#endif  // ROUTER_MGR_HPP