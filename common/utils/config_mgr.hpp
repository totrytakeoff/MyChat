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

    // 获取数组大小
    size_t get_array_size(const std::string& key) const {
        try {
            std::string path = "/" + key;
            boost::replace_all(path, ".", "/");
            if (json_.contains(json::json_pointer(path)) && json_[json::json_pointer(path)].is_array()) {
                return json_[json::json_pointer(path)].size();
            }
        } catch (...) {
            // 转换失败返回0
        }
        return 0;
    }

    // 获取数组中的单个元素
    template<typename T>
    T get_array_item(const std::string& key, size_t index, const T& default_value = T{}) const {
        try {
            std::string path = "/" + key + "/" + std::to_string(index);
            boost::replace_all(path, ".", "/");
            if (json_.contains(json::json_pointer(path))) {
                return json_[json::json_pointer(path)].get<T>();
            }
        } catch (...) {
            // 转换失败返回默认值
        }
        return default_value;
    }

    // 获取整个数组
    template<typename T>
    std::vector<T> get_array(const std::string& key) const {
        std::vector<T> result;
        try {
            std::string path = "/" + key;
            boost::replace_all(path, ".", "/");
            if (json_.contains(json::json_pointer(path)) && json_[json::json_pointer(path)].is_array()) {
                for (const auto& item : json_[json::json_pointer(path)]) {
                    result.push_back(item.get<T>());
                }
            }
        } catch (...) {
            // 转换失败返回空数组
        }
        return result;
    }

    // 获取对象数组中的特定字段
    template<typename T>
    std::vector<T> get_array_field(const std::string& array_key, const std::string& field_key) const {
        std::vector<T> result;
        try {
            std::string path = "/" + array_key;
            boost::replace_all(path, ".", "/");
            if (json_.contains(json::json_pointer(path)) && json_[json::json_pointer(path)].is_array()) {
                for (const auto& item : json_[json::json_pointer(path)]) {
                    if (item.contains(field_key)) {
                        result.push_back(item[field_key].get<T>());
                    }
                }
            }
        } catch (...) {
            // 转换失败返回空数组
        }
        return result;
    }

    // 检查键是否存在
    bool has_key(const std::string& key) const {
        try {
            std::string path = "/" + key;
            boost::replace_all(path, ".", "/");
            return json_.contains(json::json_pointer(path));
        } catch (...) {
            return false;
        }
    }

    // 直接获取JSON对象value（返回nlohmann::json对象）
    json get_json_value(const std::string& key) const {
        try {
            std::string path = "/" + key;
            boost::replace_all(path, ".", "/");
            if (json_.contains(json::json_pointer(path))) {
                return json_[json::json_pointer(path)];
            }
        } catch (...) {
            // 转换失败返回null json
        }
        return json(); // 返回null json对象
    }

    // 获取指定路径的JSON对象（如果不存在返回默认的JSON对象）
    json get_json_object(const std::string& key, const json& default_value = json::object()) const {
        try {
            std::string path = "/" + key;
            boost::replace_all(path, ".", "/");
            if (json_.contains(json::json_pointer(path))) {
                const auto& value = json_[json::json_pointer(path)];
                return value.is_object() ? value : default_value;
            }
        } catch (...) {
            // 转换失败返回默认值
        }
        return default_value;
    }

    // 获取整个配置文件的裸JSON对象
    const json& get_raw_json() const {
        return json_;
    }

    // 获取整个配置文件的JSON字符串表示
    std::string get_json_string(int indent = -1) const {
        if (indent >= 0) {
            return json_.dump(indent);
        }
        return json_.dump();
    }

    // 获取指定路径的JSON字符串表示
    std::string get_json_string(const std::string& key, int indent = -1) const {
        try {
            std::string path = "/" + key;
            boost::replace_all(path, ".", "/");
            if (json_.contains(json::json_pointer(path))) {
                const auto& value = json_[json::json_pointer(path)];
                if (indent >= 0) {
                    return value.dump(indent);
                }
                return value.dump();
            }
        } catch (...) {
            // 转换失败返回空字符串
        }
        return "{}";
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