#include <fstream>
#include <unordered_map>
#include <string>
#include <nlohmann/json.hpp>
#include <boost/algorithm/string.hpp>
#include <filesystem>

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

    std::string get(std::string key) {
        auto it = config_.find(key);
        if (it != config_.end()) {
            return it->second;
        }
        return "";
    }

    void set(std::string key, std::string value) {
        config_[key] = value;
        json_pointer_set(json_, key, value);
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
            result[parent_key] = j.get<std::string>();
        }
    }

    // 使用JSON指针设置嵌套值
    void json_pointer_set(json& j, const std::string& key, const std::string& value) {
        std::string path = "/" + key;
        boost::replace_all(path, ".", "/");
        j[json::json_pointer(path)] = value;
    }

    std::string path_;
    std::unordered_map<std::string, std::string> config_;
    json json_;
};

} // namespace utils
} // namespace im