#ifndef CONFIG_MGR_HPP
#define CONFIG_MGR_HPP

/******************************************************************************
 *
 * @file       config_mgr.hpp
 * @brief      配置管理器(模板支持)
 *
 * @author     myselfs
 * @date       2025/08/12
 * 
 *****************************************************************************/

#include <fstream>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <filesystem>
#include <type_traits>

namespace im {
namespace utils {

using json = nlohmann::json;

class ConfigManager {
public:
    ConfigManager(std::string path) : path_(std::filesystem::absolute(path)) {
        std::ifstream file(path_);
        if (file.is_open()) {
            file >> json_;
            flatten_json(json_, "", config_);
        }
    }

    // 保留原有的字符串获取方法以保持向后兼容
    std::string get(std::string key) {
        auto it = config_.find(key);
        if (it != config_.end()) {
            return it->second;
        }
        return "";
    }

    // 添加模板方法以支持不同类型
    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        try {
            std::string path = "/" + key;
            boost::replace_all(path, ".", "/");
            if (json_.contains(json::json_pointer(path))) {
                return json_[json::json_pointer(path)].get<T>();
            }
        } catch (...) {
            // 如果转换失败，返回默认值
        }
        return default_value;
    }

    // 重载set方法以支持不同类型
    template<typename T>
    void set(const std::string& key, const T& value) {
        // 更新内部JSON对象
        std::string path = "/" + key;
        boost::replace_all(path, ".", "/");
        json_[json::json_pointer(path)] = value;
        
        // 更新扁平化的字符串映射（用于向后兼容）
        if constexpr (std::is_arithmetic_v<T>) {
            config_[key] = std::to_string(value);
        } else if constexpr (std::is_same_v<T, std::string>) {
            config_[key] = value;
        } else {
            config_[key] = json(value).dump();
        }
    }

    void save() {
        std::ofstream file(path_);
        if (file.is_open()) {
            file << json_.dump(4);
        }
    }

private:
    // 递归展开嵌套的JSON对象
    void flatten_json(const json& j, const std::string& parent_key, 
                     std::unordered_map<std::string, std::string>& result) {
        if (j.is_object()) {
            for (auto& [key, value] : j.items()) {
                std::string new_key = parent_key.empty() ? key : parent_key + "." + key;
                flatten_json(value, new_key, result);
            }
        } else {
            // 处理不同类型的值
            if (j.is_string()) {
                result[parent_key] = j.get<std::string>();
            } else {
                result[parent_key] = j.dump();
            }
        }
    }

    std::string path_;
    std::unordered_map<std::string, std::string> config_;
    json json_;
};

} // namespace utils
} // namespace im


#endif // CONFIG_MGR_HPP