#ifndef HTTP_ROUTER_HPP
#define HTTP_ROUTER_HPP

/******************************************************************************
 *
 * @file       http_router.hpp
 * @brief      HTTP路由器，负责HTTP请求的路由解析和参数提取
 *
 * @author     myself
 * @date       2025/01/20
 *
 * @details    该类提供了以下核心功能：
 *             1. 配置文件驱动的路由规则管理
 *             2. 支持路径参数提取（如：/user/{id}）
 *             3. HTTP方法匹配验证
 *             4. 高性能的路由匹配算法
 *             5. 路由缓存优化
 *
 * @note       配置文件格式：
 *             {
 *               "api_prefix": "/api/v1",
 *               "routes": [
 *                 {
 *                   "pattern": "/auth/login",
 *                   "methods": ["POST"],
 *                   "cmd_id": 1001,
 *                   "service": "user_service"
 *                 }
 *               ]
 *             }
 *
 *****************************************************************************/

#include <memory>
#include <regex>
#include <string>
#include <unordered_map>
#include <vector>

#include "../../common/utils/log_manager.hpp"
#include <nlohmann/json.hpp>

namespace im {
namespace gateway {

/**
 * @class HttpRouter
 * @brief HTTP路由器类，负责HTTP请求的路由解析
 *
 * 该类支持灵活的路由配置，包括路径参数、HTTP方法匹配、
 * 以及高效的路由查找算法。
 */
class HttpRouter {
public:
    /**
     * @struct RouteResult
     * @brief 路由解析结果
     */
    struct RouteResult {
        uint32_t cmd_id;                                    ///< 命令ID
        std::string service_name;                           ///< 目标微服务名称
        std::unordered_map<std::string, std::string> path_params;  ///< 路径参数
        std::unordered_map<std::string, std::string> metadata;     ///< 额外元数据
        bool is_valid;                                      ///< 是否有效
        std::string error_message;                          ///< 错误信息
        
        RouteResult() : cmd_id(0), is_valid(false) {}
    };

    /**
     * @brief 构造函数
     */
    HttpRouter();

    /**
     * @brief 析构函数
     */
    ~HttpRouter();

    /**
     * @brief 从配置文件加载路由规则
     * @param config_file 配置文件路径
     * @return 是否加载成功
     */
    bool load_config(const std::string& config_file);

    /**
     * @brief 从JSON对象加载路由规则
     * @param config_json JSON配置对象
     * @return 是否加载成功
     */
    bool load_config_from_json(const nlohmann::json& config_json);

    /**
     * @brief 解析HTTP路由
     * @param method HTTP方法（GET, POST, PUT, DELETE等）
     * @param path 请求路径
     * @return 路由解析结果
     */
    std::unique_ptr<RouteResult> parse_route(const std::string& method, const std::string& path);

    /**
     * @brief 获取所有路由信息（用于调试和监控）
     * @return 路由信息列表
     */
    std::vector<std::string> get_route_info() const;

    /**
     * @brief 清除路由缓存
     */
    void clear_cache();

    /**
     * @brief 获取缓存统计信息
     * @return 缓存命中率等统计信息
     */
    std::unordered_map<std::string, uint64_t> get_cache_stats() const;

private:
    /**
     * @struct RoutePattern
     * @brief 路由模式定义
     */
    struct RoutePattern {
        std::string pattern;                    ///< 路由模式字符串
        std::regex compiled_regex;              ///< 编译后的正则表达式
        std::vector<std::string> param_names;   ///< 参数名称列表
        std::vector<std::string> methods;       ///< 支持的HTTP方法
        uint32_t cmd_id;                        ///< 命令ID
        std::string service_name;               ///< 目标服务名称
        std::unordered_map<std::string, std::string> metadata;  ///< 元数据
        int priority;                           ///< 优先级（数字越小优先级越高）
        
        RoutePattern() : cmd_id(0), priority(0) {}
    };

    /**
     * @struct CacheEntry
     * @brief 缓存条目
     */
    struct CacheEntry {
        RouteResult result;
        std::chrono::steady_clock::time_point timestamp;
        
        CacheEntry() = default;
        CacheEntry(const RouteResult& r) : result(r), timestamp(std::chrono::steady_clock::now()) {}
    };

    /**
     * @brief 编译路由模式为正则表达式
     * @param pattern 路由模式（如：/user/{id}/posts/{post_id}）
     * @param param_names 输出参数名称列表
     * @return 编译后的正则表达式
     */
    std::regex compile_pattern(const std::string& pattern, std::vector<std::string>& param_names);

    /**
     * @brief 验证HTTP方法是否匹配
     * @param route_methods 路由支持的方法列表
     * @param request_method 请求的方法
     * @return 是否匹配
     */
    bool match_method(const std::vector<std::string>& route_methods, const std::string& request_method);

    /**
     * @brief 提取路径参数
     * @param match_result 正则匹配结果
     * @param param_names 参数名称列表
     * @return 参数键值对
     */
    std::unordered_map<std::string, std::string> extract_path_params(
        const std::smatch& match_result, 
        const std::vector<std::string>& param_names);

    /**
     * @brief 标准化路径（移除多余斜杠、处理相对路径等）
     * @param path 原始路径
     * @return 标准化后的路径
     */
    std::string normalize_path(const std::string& path);

    /**
     * @brief 验证路由配置的合法性
     * @param route_json 路由配置JSON
     * @return 是否合法
     */
    bool validate_route_config(const nlohmann::json& route_json);

    /**
     * @brief 生成缓存键
     * @param method HTTP方法
     * @param path 请求路径
     * @return 缓存键
     */
    std::string generate_cache_key(const std::string& method, const std::string& path);

    /**
     * @brief 清理过期缓存
     */
    void cleanup_expired_cache();

private:
    std::string api_prefix_;                                ///< API前缀
    std::vector<RoutePattern> route_patterns_;              ///< 路由模式列表
    std::unordered_map<std::string, CacheEntry> route_cache_;  ///< 路由缓存
    
    // 缓存统计
    mutable uint64_t cache_hits_;                           ///< 缓存命中次数
    mutable uint64_t cache_misses_;                         ///< 缓存未命中次数
    
    // 配置参数
    size_t max_cache_size_;                                 ///< 最大缓存大小
    std::chrono::minutes cache_ttl_;                        ///< 缓存TTL
    
    mutable std::mutex mutex_;                              ///< 线程安全锁
    
    // 日志管理器
    std::shared_ptr<im::utils::LogManager> log_mgr_;
};

}  // namespace gateway
}  // namespace im

#endif  // HTTP_ROUTER_HPP