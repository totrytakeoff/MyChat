#include "simple_router.hpp"
#include <fstream>
#include <iostream>

namespace im {
namespace gateway {

SimpleRouter::SimpleRouter() : api_prefix_("/api/v1") {
}

bool SimpleRouter::load_config(const std::string& config_file) {
    try {
        // 1. 读取JSON文件
        std::ifstream file(config_file);
        if (!file.is_open()) {
            std::cerr << "无法打开配置文件: " << config_file << std::endl;
            return false;
        }

        nlohmann::json config;
        file >> config;

        // 2. 读取API前缀
        if (config.contains("api_prefix")) {
            api_prefix_ = config["api_prefix"];
        }
        std::cout << "API前缀: " << api_prefix_ << std::endl;

        // 3. 读取路由列表
        if (!config.contains("routes")) {
            std::cerr << "配置文件中没有找到routes字段" << std::endl;
            return false;
        }

        auto routes = config["routes"];
        for (const auto& route : routes) {
            // 获取路径和命令ID
            std::string pattern = route["pattern"];
            uint32_t cmd_id = route["cmd_id"];
            std::string service = route["service"];

            // 完整路径 = API前缀 + 路径模式
            std::string full_path = api_prefix_ + pattern;
            
            // 存储映射关系
            routes_[full_path] = cmd_id;
            services_[full_path] = service;

            std::cout << "加载路由: " << full_path << " -> CMD_ID: " << cmd_id 
                      << ", Service: " << service << std::endl;
        }

        std::cout << "成功加载 " << routes_.size() << " 个路由" << std::endl;
        return true;

    } catch (const std::exception& e) {
        std::cerr << "加载配置文件失败: " << e.what() << std::endl;
        return false;
    }
}

SimpleRouter::RouteInfo SimpleRouter::find_route(const std::string& path) {
    RouteInfo info;

    // 在路由表中查找
    auto it = routes_.find(path);
    if (it != routes_.end()) {
        info.cmd_id = it->second;
        info.service_name = services_[path];
        info.found = true;
        
        std::cout << "找到路由: " << path << " -> CMD_ID: " << info.cmd_id 
                  << ", Service: " << info.service_name << std::endl;
    } else {
        std::cout << "未找到路由: " << path << std::endl;
    }

    return info;
}

void SimpleRouter::print_routes() {
    std::cout << "\n=== 所有路由信息 ===" << std::endl;
    std::cout << "API前缀: " << api_prefix_ << std::endl;
    std::cout << "路由数量: " << routes_.size() << std::endl;
    
    for (const auto& [path, cmd_id] : routes_) {
        std::cout << path << " -> CMD_ID: " << cmd_id 
                  << ", Service: " << services_[path] << std::endl;
    }
    std::cout << "===================" << std::endl;
}

}  // namespace gateway  
}  // namespace im