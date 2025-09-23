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
#include <cstdlib>
#include <algorithm>
#include <regex>

namespace im {
namespace utils {

using json = nlohmann::json;

class ConfigManager {
public:
    /**
     * @brief 构造函数 - 从配置文件初始化
     * @param path 配置文件路径
     */
    ConfigManager(std::string path) : path_(std::filesystem::absolute(path)) {
        std::ifstream file(path_);
        if (file.is_open()) {
            file >> json_;
            flatten_json(json_, "", config_);
        }
    }

    /**
     * @brief 构造函数 - 从配置文件和环境变量初始化
     * @param path 配置文件路径
     * @param load_env 是否加载环境变量
     * @param env_prefix 环境变量前缀（可选）
     */
    ConfigManager(std::string path, bool load_env, const std::string& env_prefix = "") 
        : path_(std::filesystem::absolute(path)) {
        
        // 加载配置文件
        std::ifstream file(path_);
        if (file.is_open()) {
            file >> json_;
            flatten_json(json_, "", config_);
        }
        
        // 加载环境变量
        if (load_env) {
            loadEnvironmentVariables(env_prefix);
        }
    }

    /**
     * @brief 从.env文件加载环境变量
     * @param env_file .env文件路径
     * @param override_existing 是否覆盖已存在的配置
     * @return 加载是否成功
     */
    bool loadEnvFile(const std::string& env_file, bool override_existing = false) {
        std::ifstream file(env_file);
        if (!file.is_open()) {
            return false;
        }

        std::string line;
        while (std::getline(file, line)) {
            // 跳过空行和注释
            boost::trim(line);
            if (line.empty() || line[0] == '#') {
                continue;
            }

            // 解析 KEY=VALUE 格式
            auto pos = line.find('=');
            if (pos != std::string::npos) {
                std::string key = line.substr(0, pos);
                std::string value = line.substr(pos + 1);
                
                boost::trim(key);
                boost::trim(value);
                
                // 移除引号
                if (value.size() >= 2 && 
                    ((value.front() == '"' && value.back() == '"') ||
                     (value.front() == '\'' && value.back() == '\''))) {
                    value = value.substr(1, value.size() - 2);
                }
                
                // 设置到配置中
                if (override_existing || !has_key(key)) {
                    setEnvironmentValue(key, value);
                }
            }
        }
        return true;
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

    // ==================== 环境变量相关方法 ====================

    /**
     * @brief 从系统环境变量获取值
     * @param key 环境变量名
     * @param default_value 默认值
     * @return 环境变量值或默认值
     */
    template<typename T>
    T getEnv(const std::string& key, const T& default_value = T{}) const {
        const char* env_val = std::getenv(key.c_str());
        if (!env_val) {
            return default_value;
        }
        
        std::string str_val(env_val);
        return convertStringToType<T>(str_val, default_value);
    }

    /**
     * @brief 获取配置值，优先使用环境变量
     * @param key 配置键名
     * @param env_key 环境变量名（如果为空则使用key）
     * @param default_value 默认值
     * @return 配置值（环境变量优先，其次配置文件，最后默认值）
     */
    template<typename T>
    T getWithEnv(const std::string& key, const std::string& env_key = "", 
                 const T& default_value = T{}) const {
        
        std::string actual_env_key = env_key.empty() ? key : env_key;
        
        // 首先检查环境变量
        const char* env_val = std::getenv(actual_env_key.c_str());
        if (env_val) {
            std::string str_val(env_val);
            return convertStringToType<T>(str_val, default_value);
        }

        // 其次检查配置文件中是否存在以环境变量名为键的值（用于 .env 文件回退）
        if (!env_key.empty() && has_key(env_key)) {
            return get<T>(env_key, default_value);
        }

        // 最后检查配置键
        return get<T>(key, default_value);
    }

    /**
     * @brief 设置环境变量到配置中
     * @param key 配置键名
     * @param value 值
     */
    void setEnvironmentValue(const std::string& key, const std::string& value) {
        // 更新扁平化配置
        config_[key] = value;
        
        // 尝试更新JSON对象
        try {
            std::string path = "/" + key;
            boost::replace_all(path, ".", "/");
            
            // 尝试智能类型转换
            if (isInteger(value)) {
                json_[json::json_pointer(path)] = std::stoll(value);
            } else if (isFloat(value)) {
                json_[json::json_pointer(path)] = std::stod(value);
            } else if (isBool(value)) {
                json_[json::json_pointer(path)] = toBool(value);
            } else {
                json_[json::json_pointer(path)] = value;
            }
        } catch (...) {
            // JSON更新失败，保持字符串格式
        }
    }

    /**
     * @brief 加载系统环境变量
     * @param prefix 环境变量前缀（可选）
     */
    void loadEnvironmentVariables(const std::string& prefix = "") {
        // 注意：这里只能获取已知的环境变量，因为C++标准库不提供遍历所有环境变量的方法
        // 实际使用中，应该预定义需要读取的环境变量列表
        
        // 常用的配置环境变量
        std::vector<std::string> common_env_vars = {
            "PORT", "HOST", "DEBUG", "LOG_LEVEL", "DATABASE_URL", "REDIS_URL",
            "WS_PORT", "HTTP_PORT", "REDIS_HOST", "REDIS_PORT", "REDIS_PASSWORD",
            "SSL_CERT", "SSL_KEY", "JWT_SECRET", "ENVIRONMENT", "SERVICE_NAME"
        };
        
        for (const auto& var : common_env_vars) {
            std::string full_var = prefix.empty() ? var : prefix + "_" + var;
            const char* value = std::getenv(full_var.c_str());
            if (value) {
                std::string key = prefix.empty() ? 
                    toLowerCase(var) : 
                    toLowerCase(prefix) + "." + toLowerCase(var);
                setEnvironmentValue(key, value);
            }
        }
    }

    /**
     * @brief 从预定义列表加载环境变量
     * @param env_vars 环境变量名列表
     * @param prefix 环境变量前缀（可选）
     */
    void loadEnvironmentVariables(const std::vector<std::string>& env_vars, 
                                 const std::string& prefix = "") {
        for (const auto& var : env_vars) {
            std::string full_var = prefix.empty() ? var : prefix + "_" + var;
            const char* value = std::getenv(full_var.c_str());
            if (value) {
                std::string key = prefix.empty() ? 
                    toLowerCase(var) : 
                    toLowerCase(prefix) + "." + toLowerCase(var);
                setEnvironmentValue(key, value);
            }
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

    // ==================== 环境变量辅助方法 ====================

    /**
     * @brief 字符串类型转换
     */
    template<typename T>
    T convertStringToType(const std::string& str, const T& default_value) const {
        try {
            if constexpr (std::is_same_v<T, std::string>) {
                return str;
            } else if constexpr (std::is_same_v<T, int>) {
                return std::stoi(str);
            } else if constexpr (std::is_same_v<T, long>) {
                return std::stol(str);
            } else if constexpr (std::is_same_v<T, long long>) {
                return std::stoll(str);
            } else if constexpr (std::is_same_v<T, float>) {
                return std::stof(str);
            } else if constexpr (std::is_same_v<T, double>) {
                return std::stod(str);
            } else if constexpr (std::is_same_v<T, bool>) {
                return toBool(str);
            } else {
                // 对于其他类型，尝试JSON转换
                return json::parse(str).get<T>();
            }
        } catch (...) {
            return default_value;
        }
    }

    /**
     * @brief 判断字符串是否为整数
     */
    bool isInteger(const std::string& str) const {
        if (str.empty()) return false;
        
        size_t start = 0;
        if (str[0] == '-' || str[0] == '+') start = 1;
        if (start >= str.length()) return false;
        
        return std::all_of(str.begin() + start, str.end(), ::isdigit);
    }

    /**
     * @brief 判断字符串是否为浮点数
     */
    bool isFloat(const std::string& str) const {
        if (str.empty()) return false;
        
        try {
            std::stod(str);
            return str.find('.') != std::string::npos || 
                   str.find('e') != std::string::npos || 
                   str.find('E') != std::string::npos;
        } catch (...) {
            return false;
        }
    }

    /**
     * @brief 判断字符串是否为布尔值
     */
    bool isBool(const std::string& str) const {
        std::string lower_str = str;
        std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
        
        return lower_str == "true" || lower_str == "false" || 
               lower_str == "1" || lower_str == "0" ||
               lower_str == "yes" || lower_str == "no" ||
               lower_str == "on" || lower_str == "off";
    }

    /**
     * @brief 字符串转布尔值
     */
    bool toBool(const std::string& str) const {
        std::string lower_str = str;
        std::transform(lower_str.begin(), lower_str.end(), lower_str.begin(), ::tolower);
        
        return lower_str == "true" || lower_str == "1" || 
               lower_str == "yes" || lower_str == "on";
    }

    /**
     * @brief 字符串转小写
     */
    std::string toLowerCase(const std::string& str) const {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(), ::tolower);
        return result;
    }

private:
    std::string path_;
    std::unordered_map<std::string, std::string> config_;
    json json_;
};

} // namespace utils
} // namespace im


#endif // CONFIG_MGR_HPP