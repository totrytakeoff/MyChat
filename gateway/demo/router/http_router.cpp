#include "http_router.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace im {
namespace gateway {

HttpRouter::HttpRouter() 
    : cache_hits_(0)
    , cache_misses_(0)
    , max_cache_size_(1000)
    , cache_ttl_(std::chrono::minutes(10)) {
    
    // 初始化日志管理器
    log_mgr_ = im::utils::LogManager::get_instance();
}

HttpRouter::~HttpRouter() = default;

bool HttpRouter::load_config(const std::string& config_file) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    try {
        std::ifstream file(config_file);
        if (!file.is_open()) {
            if (log_mgr_) {
                log_mgr_->log_error("HttpRouter", "Failed to open config file: " + config_file);
            }
            return false;
        }
        
        nlohmann::json config_json;
        file >> config_json;
        
        return load_config_from_json(config_json);
        
    } catch (const std::exception& e) {
        if (log_mgr_) {
            log_mgr_->log_error("HttpRouter", "Failed to parse config file: " + std::string(e.what()));
        }
        return false;
    }
}

bool HttpRouter::load_config_from_json(const nlohmann::json& config_json) {
    try {
        // 清除现有配置
        route_patterns_.clear();
        clear_cache();
        
        // 读取API前缀
        if (config_json.contains("api_prefix")) {
            api_prefix_ = config_json["api_prefix"].get<std::string>();
        } else {
            api_prefix_ = "/api/v1";  // 默认前缀
        }
        
        // 读取缓存配置
        if (config_json.contains("cache_config")) {
            const auto& cache_config = config_json["cache_config"];
            if (cache_config.contains("max_size")) {
                max_cache_size_ = cache_config["max_size"].get<size_t>();
            }
            if (cache_config.contains("ttl_minutes")) {
                cache_ttl_ = std::chrono::minutes(cache_config["ttl_minutes"].get<int>());
            }
        }
        
        // 读取路由配置
        if (!config_json.contains("routes")) {
            if (log_mgr_) {
                log_mgr_->log_error("HttpRouter", "No routes section found in config");
            }
            return false;
        }
        
        const auto& routes = config_json["routes"];
        if (!routes.is_array()) {
            if (log_mgr_) {
                log_mgr_->log_error("HttpRouter", "Routes section must be an array");
            }
            return false;
        }
        
        // 解析每个路由
        for (const auto& route : routes) {
            if (!validate_route_config(route)) {
                continue;  // 跳过无效的路由配置
            }
            
            RoutePattern pattern;
            
            // 基本信息
            pattern.pattern = route["pattern"].get<std::string>();
            pattern.cmd_id = route["cmd_id"].get<uint32_t>();
            pattern.service_name = route["service"].get<std::string>();
            
            // HTTP方法
            if (route.contains("methods")) {
                pattern.methods = route["methods"].get<std::vector<std::string>>();
            } else {
                pattern.methods = {"GET", "POST"};  // 默认支持GET和POST
            }
            
            // 优先级
            if (route.contains("priority")) {
                pattern.priority = route["priority"].get<int>();
            } else {
                pattern.priority = 0;
            }
            
            // 元数据
            if (route.contains("metadata")) {
                pattern.metadata = route["metadata"].get<std::unordered_map<std::string, std::string>>();
            }
            
            // 编译正则表达式
            try {
                pattern.compiled_regex = compile_pattern(pattern.pattern, pattern.param_names);
            } catch (const std::exception& e) {
                if (log_mgr_) {
                    log_mgr_->log_error("HttpRouter", 
                        "Failed to compile pattern '" + pattern.pattern + "': " + e.what());
                }
                continue;
            }
            
            route_patterns_.push_back(std::move(pattern));
        }
        
        // 按优先级排序（优先级数字越小越优先）
        std::sort(route_patterns_.begin(), route_patterns_.end(),
            [](const RoutePattern& a, const RoutePattern& b) {
                return a.priority < b.priority;
            });
        
        if (log_mgr_) {
            log_mgr_->log_info("HttpRouter", 
                "Loaded " + std::to_string(route_patterns_.size()) + " routes with prefix: " + api_prefix_);
        }
        
        return true;
        
    } catch (const std::exception& e) {
        if (log_mgr_) {
            log_mgr_->log_error("HttpRouter", "Failed to load config: " + std::string(e.what()));
        }
        return false;
    }
}

std::unique_ptr<HttpRouter::RouteResult> HttpRouter::parse_route(const std::string& method, const std::string& path) {
    std::lock_guard<std::mutex> lock(mutex_);
    
    // 标准化路径
    std::string normalized_path = normalize_path(path);
    
    // 检查缓存
    std::string cache_key = generate_cache_key(method, normalized_path);
    auto cache_it = route_cache_.find(cache_key);
    if (cache_it != route_cache_.end()) {
        // 检查缓存是否过期
        auto now = std::chrono::steady_clock::now();
        if (now - cache_it->second.timestamp < cache_ttl_) {
            cache_hits_++;
            return std::make_unique<RouteResult>(cache_it->second.result);
        } else {
            // 移除过期缓存
            route_cache_.erase(cache_it);
        }
    }
    
    cache_misses_++;
    
    // 创建结果对象
    auto result = std::make_unique<RouteResult>();
    
    // 检查API前缀
    if (!api_prefix_.empty() && normalized_path.find(api_prefix_) != 0) {
        result->error_message = "Path does not match API prefix: " + api_prefix_;
        return result;
    }
    
    // 移除API前缀
    std::string route_path = normalized_path;
    if (!api_prefix_.empty()) {
        route_path = normalized_path.substr(api_prefix_.length());
        if (route_path.empty()) {
            route_path = "/";
        }
    }
    
    // 遍历路由模式进行匹配
    for (const auto& pattern : route_patterns_) {
        // 检查HTTP方法
        if (!match_method(pattern.methods, method)) {
            continue;
        }
        
        // 进行正则匹配
        std::smatch match_result;
        if (std::regex_match(route_path, match_result, pattern.compiled_regex)) {
            // 匹配成功
            result->cmd_id = pattern.cmd_id;
            result->service_name = pattern.service_name;
            result->path_params = extract_path_params(match_result, pattern.param_names);
            result->metadata = pattern.metadata;
            result->is_valid = true;
            
            // 添加到缓存
            if (route_cache_.size() < max_cache_size_) {
                route_cache_[cache_key] = CacheEntry(*result);
            } else {
                // 缓存已满，清理过期缓存
                cleanup_expired_cache();
                if (route_cache_.size() < max_cache_size_) {
                    route_cache_[cache_key] = CacheEntry(*result);
                }
            }
            
            if (log_mgr_) {
                log_mgr_->log_debug("HttpRouter", 
                    "Route matched: " + method + " " + path + " -> CMD_ID: " + std::to_string(result->cmd_id));
            }
            
            return result;
        }
    }
    
    // 没有找到匹配的路由
    result->error_message = "No matching route found for: " + method + " " + path;
    
    if (log_mgr_) {
        log_mgr_->log_warn("HttpRouter", result->error_message);
    }
    
    return result;
}

std::regex HttpRouter::compile_pattern(const std::string& pattern, std::vector<std::string>& param_names) {
    param_names.clear();
    
    std::string regex_pattern = pattern;
    std::regex param_regex(R"(\{([^}]+)\})");
    std::sregex_iterator iter(pattern.begin(), pattern.end(), param_regex);
    std::sregex_iterator end;
    
    // 从后往前替换，避免位置偏移问题
    std::vector<std::pair<size_t, size_t>> replacements;
    std::vector<std::string> temp_param_names;
    
    for (auto it = iter; it != end; ++it) {
        replacements.push_back({it->position(), it->length()});
        temp_param_names.push_back((*it)[1].str());
    }
    
    // 反向替换
    for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
        regex_pattern.replace(it->first, it->second, "([^/]+)");
    }
    
    param_names = std::move(temp_param_names);
    
    // 确保模式以^开头，以$结尾
    if (regex_pattern.front() != '^') {
        regex_pattern = "^" + regex_pattern;
    }
    if (regex_pattern.back() != '$') {
        regex_pattern = regex_pattern + "$";
    }
    
    return std::regex(regex_pattern);
}

bool HttpRouter::match_method(const std::vector<std::string>& route_methods, const std::string& request_method) {
    if (route_methods.empty()) {
        return true;  // 空列表表示支持所有方法
    }
    
    return std::find(route_methods.begin(), route_methods.end(), request_method) != route_methods.end();
}

std::unordered_map<std::string, std::string> HttpRouter::extract_path_params(
    const std::smatch& match_result, 
    const std::vector<std::string>& param_names) {
    
    std::unordered_map<std::string, std::string> params;
    
    // match_result[0]是整个匹配，从[1]开始是捕获组
    for (size_t i = 1; i < match_result.size() && i - 1 < param_names.size(); ++i) {
        params[param_names[i - 1]] = match_result[i].str();
    }
    
    return params;
}

std::string HttpRouter::normalize_path(const std::string& path) {
    if (path.empty()) {
        return "/";
    }
    
    std::string normalized = path;
    
    // 确保以/开头
    if (normalized.front() != '/') {
        normalized = "/" + normalized;
    }
    
    // 移除末尾的/（除非是根路径）
    if (normalized.length() > 1 && normalized.back() == '/') {
        normalized.pop_back();
    }
    
    // 移除重复的/
    std::string result;
    bool prev_slash = false;
    for (char c : normalized) {
        if (c == '/') {
            if (!prev_slash) {
                result += c;
                prev_slash = true;
            }
        } else {
            result += c;
            prev_slash = false;
        }
    }
    
    return result.empty() ? "/" : result;
}

bool HttpRouter::validate_route_config(const nlohmann::json& route_json) {
    // 检查必需字段
    if (!route_json.contains("pattern") || !route_json["pattern"].is_string()) {
        if (log_mgr_) {
            log_mgr_->log_error("HttpRouter", "Route config missing 'pattern' field");
        }
        return false;
    }
    
    if (!route_json.contains("cmd_id") || !route_json["cmd_id"].is_number_unsigned()) {
        if (log_mgr_) {
            log_mgr_->log_error("HttpRouter", "Route config missing 'cmd_id' field");
        }
        return false;
    }
    
    if (!route_json.contains("service") || !route_json["service"].is_string()) {
        if (log_mgr_) {
            log_mgr_->log_error("HttpRouter", "Route config missing 'service' field");
        }
        return false;
    }
    
    // 检查可选字段的类型
    if (route_json.contains("methods") && !route_json["methods"].is_array()) {
        if (log_mgr_) {
            log_mgr_->log_error("HttpRouter", "Route config 'methods' field must be an array");
        }
        return false;
    }
    
    if (route_json.contains("priority") && !route_json["priority"].is_number()) {
        if (log_mgr_) {
            log_mgr_->log_error("HttpRouter", "Route config 'priority' field must be a number");
        }
        return false;
    }
    
    return true;
}

std::string HttpRouter::generate_cache_key(const std::string& method, const std::string& path) {
    return method + ":" + path;
}

void HttpRouter::cleanup_expired_cache() {
    auto now = std::chrono::steady_clock::now();
    
    for (auto it = route_cache_.begin(); it != route_cache_.end();) {
        if (now - it->second.timestamp >= cache_ttl_) {
            it = route_cache_.erase(it);
        } else {
            ++it;
        }
    }
}

std::vector<std::string> HttpRouter::get_route_info() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::vector<std::string> info;
    info.reserve(route_patterns_.size() + 2);
    
    info.push_back("API Prefix: " + api_prefix_);
    info.push_back("Total Routes: " + std::to_string(route_patterns_.size()));
    
    for (const auto& pattern : route_patterns_) {
        std::ostringstream oss;
        oss << "Pattern: " << pattern.pattern 
            << ", CMD_ID: " << pattern.cmd_id 
            << ", Service: " << pattern.service_name
            << ", Methods: [";
        
        for (size_t i = 0; i < pattern.methods.size(); ++i) {
            if (i > 0) oss << ", ";
            oss << pattern.methods[i];
        }
        oss << "]";
        
        if (pattern.priority != 0) {
            oss << ", Priority: " << pattern.priority;
        }
        
        info.push_back(oss.str());
    }
    
    return info;
}

void HttpRouter::clear_cache() {
    std::lock_guard<std::mutex> lock(mutex_);
    route_cache_.clear();
    cache_hits_ = 0;
    cache_misses_ = 0;
}

std::unordered_map<std::string, uint64_t> HttpRouter::get_cache_stats() const {
    std::lock_guard<std::mutex> lock(mutex_);
    
    std::unordered_map<std::string, uint64_t> stats;
    stats["cache_hits"] = cache_hits_;
    stats["cache_misses"] = cache_misses_;
    stats["cache_size"] = route_cache_.size();
    stats["max_cache_size"] = max_cache_size_;
    
    uint64_t total_requests = cache_hits_ + cache_misses_;
    if (total_requests > 0) {
        stats["hit_rate_percent"] = (cache_hits_ * 100) / total_requests;
    } else {
        stats["hit_rate_percent"] = 0;
    }
    
    return stats;
}

}  // namespace gateway
}  // namespace im