/******************************************************************************
 *
 * @file       router.mgr.cpp
 * @brief      统一路由管理器实现
 *
 * @details    实现HTTP路由解析、服务发现和统一路由管理功能
 *
 * @author     myself
 * @date       2025/08/21
 * @version    1.0.0
 *
 *****************************************************************************/

#include <cstdint>
#include <nlohmann/json.hpp>
#include "../../common/utils/config_mgr.hpp"
#include "../../common/utils/log_manager.hpp"
#include "router_mgr.hpp"


namespace im {
namespace gateway {

using im::utils::ConfigManager;
using im::utils::LogManager;
using json = nlohmann::json;


// ===== HttpRouter 实现 =====

HttpRouter::HttpRouter() {
    // 默认构造函数，需要后续调用load_config加载配置
}

std::unique_ptr<HttpRouteResult> HttpRouter::parse_route(const std::string& method,
                                                         const std::string& path) {
    auto result = std::make_unique<HttpRouteResult>();
    try {
        // 判断url是否以前缀开头
        if (!path.starts_with(api_prefix_)) {
            result->status_code = 404;
            result->err_msg = "Path does not match API prefix: " + api_prefix_;
            throw std::runtime_error(result->err_msg);
        }

        // 截取去除前缀的部分url - 修复变量名冲突
        std::string route_path = path.substr(api_prefix_.size());

        // 确保路径以/开头
        if (route_path.empty() || route_path[0] != '/') {
            route_path = "/" + route_path;
        }

        if (!routes_.contains(route_path)) {
            result->status_code = 404;
            result->err_msg = "Route not found: " + route_path;
            throw std::runtime_error(result->err_msg);
        }

        // 找到路由，返回成功结果
        const auto& route_result = routes_[route_path];
        return std::make_unique<HttpRouteResult>(route_result.cmd_id, route_result.service_name);

    } catch (const std::exception& e) {
        LogManager::GetLogger("http_router")->error("Failed to parse route {}: {}", path, e.what());
        result->is_valid = false;
        result->err_msg = e.what();
        return result;
    }
}

bool HttpRouter::load_config(const std::string& config_file) {
    try {
        ConfigManager config(config_file);
        api_prefix_ = config.get<std::string>("http_router.api_prefix", "/api/v1");

        LogManager::GetLogger("http_router")
                ->info("Loading HTTP router config, API prefix: {}", api_prefix_);

        int routes_size = config.get_array_size("http_router.routes");
        LogManager::GetLogger("http_router")->info("Found {} routes in config", routes_size);

        for (int i = 0; i < routes_size; i++) {
            auto route = config.get_array_item<nlohmann::json>("http_router.routes", i, "");
            if (route.empty()) {
                LogManager::GetLogger("http_router")
                        ->warn("Invalid route config at index {}, skipping", i);
                continue;
            }

            std::string path = route.value("path", "");
            uint32_t cmd_id = route.value("cmd_id", 0);
            std::string service_name = route.value("service_name", "");

            if (path.empty() || cmd_id == 0 || service_name.empty()) {
                LogManager::GetLogger("http_router")
                        ->warn("Invalid route config at index {}, skipping", i);
                continue;
            }

            // 创建成功的路由结果
            HttpRouteResult routeResult(cmd_id, service_name);
            routes_.emplace(path, routeResult);

            LogManager::GetLogger("http_router")
                    ->debug("Added route: {} -> CMD_ID: {}, Service: {}", path, cmd_id,
                            service_name);
        }

        LogManager::GetLogger("http_router")->info("Successfully loaded {} routes", routes_.size());
        return true;

    } catch (const std::exception& e) {
        LogManager::GetLogger("http_router")
                ->error("Failed to load config file {}: {}", config_file, e.what());
        return false;
    }
}

// ===== ServiceRouter 实现 =====

ServiceRouter::ServiceRouter() : cmds_() {
    // 默认构造函数，需要后续调用load_config
}

bool ServiceRouter::load_config(const std::string& config_file) {
    try {
        ConfigManager config(config_file);

        LogManager::GetLogger("service_router")
                ->info("Loading service router config from: {}", config_file);

        int services_size = config.get_array_size("service_router.services");
        LogManager::GetLogger("service_router")->info("Found {} services in config", services_size);

        for (int i = 0; i < services_size; i++) {
            auto service = config.get_array_item<nlohmann::json>("service_router.services", i, "");
            if (service.empty()) {
                LogManager::GetLogger("service_router")
                        ->warn("Invalid service config at index {}, skipping", i);
                continue;
            }

            std::string service_name = service.value("service_name", "");
            std::string endpoint = service.value("endpoint", "");
            uint32_t timeout_ms = service.value("timeout_ms", 3000);
            uint32_t max_connections = service.value("max_connections", 10);

            nlohmann::json cmd_range_json = service.value("cmd_range", json::array());

            std::pair<uint32_t, uint32_t> cmd_range(0, 0);
            if (cmd_range_json.is_array()) {
                cmd_range.first = cmd_range_json[0];
                cmd_range.second = cmd_range_json[1];
            }


            if (service_name.empty() || endpoint.empty()) {
                LogManager::GetLogger("service_router")
                        ->warn("Invalid service config at index {} (missing name or endpoint), "
                               "skipping",
                               i);
                continue;
            }

            // 创建成功的服务结果
            ServiceRouteResult serviceResult(service_name, endpoint, timeout_ms, max_connections);
            services_.emplace(service_name, serviceResult);
            cmds_.emplace(cmd_range, service_name);

            LogManager::GetLogger("service_router")
                    ->debug("Added service: {} -> Endpoint: {}, Timeout: {}ms, MaxConn: {}",
                            service_name, endpoint, timeout_ms, max_connections);
        }

        LogManager::GetLogger("service_router")
                ->info("Successfully loaded {} services", services_.size());
        return true;

    } catch (const std::exception& e) {
        LogManager::GetLogger("service_router")
                ->error("Failed to load service config from {}: {}", config_file, e.what());
        return false;
    }
}

std::unique_ptr<ServiceRouteResult> ServiceRouter::find_service(const std::string& service_name) {
    auto result = std::make_unique<ServiceRouteResult>();

    try {
        if (service_name.empty()) {
            result->err_msg = "Service name is empty";
            throw std::runtime_error(result->err_msg);
        }

        if (!services_.contains(service_name)) {
            result->err_msg = "Service not found: " + service_name;
            throw std::runtime_error(result->err_msg);
        }

        // 找到服务，返回成功结果
        const auto& service_result = services_[service_name];
        return std::make_unique<ServiceRouteResult>(service_result.service_name,
                                                    service_result.endpoint,
                                                    service_result.timeout_ms,
                                                    service_result.max_connections);

    } catch (const std::exception& e) {
        LogManager::GetLogger("service_router")
                ->error("Failed to find service {}: {}", service_name, e.what());
        result->is_valid = false;
        result->err_msg = e.what();
        return result;
    }
}

std::unique_ptr<ServiceRouteResult> ServiceRouter::find_service(uint32_t cmd) {
    auto result = std::make_unique<ServiceRouteResult>();

    std::string service_name;
    try {
        for (const auto& route : cmds_) {
            const auto& range = route.first;
            if (range.first <= cmd && cmd <= range.second) {
                service_name = route.second;
                break;
            }
        }
        if (service_name.empty()) {
            result->err_msg = "command not found";
            return result;
        }

        // 找到服务，返回成功结果
        const auto& service_result = services_[service_name];
        return std::make_unique<ServiceRouteResult>(service_result.service_name,
                                                    service_result.endpoint,
                                                    service_result.timeout_ms,
                                                    service_result.max_connections);

    } catch (const std::exception& e) {
        LogManager::GetLogger("service_router")
                ->error("Failed to find service {}: {}", service_name, e.what());
        result->is_valid = false;
        result->err_msg = e.what();
        return result;
    }
}


// ===== RouterManager 实现 =====

RouterManager::RouterManager(const std::string& config_file) : config_file_(config_file) {
    http_router_ = std::make_unique<HttpRouter>();
    service_router_ = std::make_unique<ServiceRouter>();

    // 加载配置
    reload_config();
}

bool RouterManager::reload_config() {
    LogManager::GetLogger("router_manager")
            ->info("Reloading router configuration from: {}", config_file_);

    bool http_ok = http_router_->load_config(config_file_);
    bool service_ok = service_router_->load_config(config_file_);

    if (http_ok && service_ok) {
        LogManager::GetLogger("router_manager")->info("Router configuration reloaded successfully");
        return true;
    } else {
        LogManager::GetLogger("router_manager")
                ->error("Failed to reload router configuration - HTTP: {}, Service: {}",
                        http_ok ? "OK" : "FAILED", service_ok ? "OK" : "FAILED");
        return false;
    }
}

std::unique_ptr<HttpRouteResult> RouterManager::parse_http_route(const std::string& method,
                                                                 const std::string& path) {
    return http_router_->parse_route(method, path);
}

std::unique_ptr<ServiceRouteResult> RouterManager::find_service(const std::string& service_name) {
    return service_router_->find_service(service_name);
}
std::unique_ptr<ServiceRouteResult> RouterManager::find_service(uint32_t service_id) {
    return service_router_->find_service(service_id);
}

std::unique_ptr<RouterManager::CompleteRouteResult> RouterManager::route_request(
        const std::string& method, const std::string& path) {
    auto complete_result = std::make_unique<CompleteRouteResult>();

    try {
        LogManager::GetLogger("router_manager")->debug("Routing request: {} {}", method, path);

        // 第一步：HTTP路由解析
        auto http_result = parse_http_route(method, path);
        if (!http_result || !http_result->is_valid) {
            complete_result->err_msg =
                    http_result ? http_result->err_msg : "HTTP route parsing failed";
            complete_result->http_status_code = http_result ? http_result->status_code : 404;
            throw std::runtime_error(complete_result->err_msg);
        }

        // 第二步：服务路由查找
        auto service_result = find_service(http_result->service_name);
        if (!service_result || !service_result->is_valid) {
            complete_result->err_msg =
                    service_result ? service_result->err_msg : "Service routing failed";
            complete_result->http_status_code = 503;  // Service Unavailable
            throw std::runtime_error(complete_result->err_msg);
        }

        // 成功：合并结果
        complete_result->cmd_id = http_result->cmd_id;
        complete_result->service_name = http_result->service_name;
        complete_result->endpoint = service_result->endpoint;
        complete_result->timeout_ms = service_result->timeout_ms;
        complete_result->max_connections = service_result->max_connections;
        complete_result->is_valid = true;
        complete_result->http_status_code = 200;

        LogManager::GetLogger("router_manager")
                ->debug("Route complete: {} {} -> CMD_ID: {}, Service: {} ({})", method, path,
                        complete_result->cmd_id, complete_result->service_name,
                        complete_result->endpoint);

        return complete_result;

    } catch (const std::exception& e) {
        LogManager::GetLogger("router_manager")
                ->error("Complete routing failed for {} {}: {}", method, path, e.what());
        complete_result->is_valid = false;
        complete_result->err_msg = e.what();
        return complete_result;
    }
}

RouterManager::RouterStats RouterManager::get_stats() const {
    RouterStats stats;
    stats.http_route_count = http_router_->get_route_count();
    stats.service_count = service_router_->get_service_count();
    stats.config_file = config_file_;
    return stats;
}

}  // namespace gateway
}  // namespace im
