# ConfigManager 配置读取类使用文档

## 目录
- [概述](#概述)
- [特性](#特性)
- [快速开始](#快速开始)
- [基础用法](#基础用法)
- [数组操作](#数组操作)
- [JSON对象操作](#json对象操作)
- [配置管理](#配置管理)
- [错误处理](#错误处理)
- [实际应用示例](#实际应用示例)
- [API参考](#api参考)
- [最佳实践](#最佳实践)

## 概述

ConfigManager是一个强大的C++配置文件读取和管理类，基于nlohmann/json库构建，支持JSON格式的配置文件。它提供了简单易用的API来读取、修改和保存配置数据。

### 主要特点
- 支持嵌套JSON对象和数组
- 类型安全的模板方法
- 点号路径访问方式
- 完整的数组操作支持
- 原生JSON对象访问
- 配置文件动态修改和保存

## 特性

### ✅ 支持的数据类型
- 基础类型：`string`, `int`, `double`, `bool`
- 复杂类型：`nlohmann::json`, `std::vector<T>`
- 自定义默认值支持

### ✅ 访问方式
- 点号路径：`"redis.host"`, `"servers.0.name"`
- 数组索引：`"array.0"`, `"array.1"`
- JSON指针：内部自动转换

### ✅ 数组操作
- 获取数组大小
- 按索引访问元素
- 批量提取字段
- 整个数组获取

## 快速开始

### 1. 包含头文件
```cpp
#include "common/utils/config_mgr.hpp"
using namespace im::utils;
```

### 2. 创建配置管理器
```cpp
ConfigManager config("path/to/config.json");
```

### 3. 读取配置
```cpp
std::string host = config.get<std::string>("redis.host");
int port = config.get<int>("redis.port");
```

## 基础用法

### 配置文件示例
```json
{
    "redis": {
        "host": "127.0.0.1",
        "port": 6379,
        "enabled": true
    },
    "mysql": {
        "host": "192.168.1.100",
        "port": "3306",
        "user": "root",
        "password": "secret",
        "database": "myapp"
    },
    "debug": true,
    "app_name": "MyApplication"
}
```

### 基础读取方法

#### 1. 字符串读取（向后兼容）
```cpp
ConfigManager config("config.json");

// 原有的字符串方法
std::string host = config.get("redis.host");        // "127.0.0.1"
std::string port = config.get("redis.port");        // "6379" (字符串)
std::string app = config.get("app_name");           // "MyApplication"
```

#### 2. 类型安全的模板方法
```cpp
// 推荐使用的模板方法
std::string host = config.get<std::string>("redis.host");     // "127.0.0.1"
int port = config.get<int>("redis.port");                     // 6379 (整数)
bool enabled = config.get<bool>("redis.enabled");             // true
bool debug = config.get<bool>("debug");                       // true
```

#### 3. 默认值支持
```cpp
// 如果键不存在，返回默认值
int timeout = config.get<int>("redis.timeout", 5000);         // 5000
std::string env = config.get<std::string>("environment", "production"); // "production"
bool ssl = config.get<bool>("redis.ssl", false);              // false
```

#### 4. 检查键是否存在
```cpp
if (config.has_key("redis.host")) {
    std::string host = config.get<std::string>("redis.host");
    // 处理逻辑
}

if (!config.has_key("optional_feature")) {
    // 使用默认配置
}
```

## 数组操作

### 配置文件示例
```json
{
    "servers": [
        {
            "name": "web-server-1",
            "host": "192.168.1.10",
            "port": 8080,
            "enabled": true
        },
        {
            "name": "web-server-2",
            "host": "192.168.1.11", 
            "port": 8081,
            "enabled": false
        },
        {
            "name": "api-server",
            "host": "192.168.1.20",
            "port": 9000,
            "enabled": true
        }
    ],
    "allowed_ips": ["127.0.0.1", "192.168.1.0/24", "10.0.0.0/8"],
    "ports": [8080, 8081, 9000, 3306, 6379],
    "tags": ["production", "web", "api", "database"]
}
```

### 1. 获取数组大小
```cpp
size_t server_count = config.get_array_size("servers");        // 3
size_t ip_count = config.get_array_size("allowed_ips");        // 3
size_t port_count = config.get_array_size("ports");            // 5
size_t tag_count = config.get_array_size("tags");              // 4
```

### 2. 按索引访问数组元素
```cpp
// 访问对象数组
nlohmann::json first_server = config.get_array_item<nlohmann::json>("servers", 0);
std::cout << first_server.dump(2) << std::endl;

// 访问基础类型数组
std::string first_ip = config.get_array_item<std::string>("allowed_ips", 0);    // "127.0.0.1"
int first_port = config.get_array_item<int>("ports", 0);                        // 8080
std::string first_tag = config.get_array_item<std::string>("tags", 0);          // "production"
```

### 3. 提取对象数组中的特定字段
```cpp
// 获取所有服务器名称
std::vector<std::string> server_names = config.get_array_field<std::string>("servers", "name");
// 结果: ["web-server-1", "web-server-2", "api-server"]

// 获取所有服务器主机
std::vector<std::string> server_hosts = config.get_array_field<std::string>("servers", "host");
// 结果: ["192.168.1.10", "192.168.1.11", "192.168.1.20"]

// 获取所有端口
std::vector<int> server_ports = config.get_array_field<int>("servers", "port");
// 结果: [8080, 8081, 9000]

// 获取所有状态
std::vector<bool> server_enabled = config.get_array_field<bool>("servers", "enabled");
// 结果: [true, false, true]
```

### 4. 获取整个数组
```cpp
// 获取字符串数组
std::vector<std::string> all_ips = config.get_array<std::string>("allowed_ips");
std::vector<std::string> all_tags = config.get_array<std::string>("tags");

// 获取数字数组
std::vector<int> all_ports = config.get_array<int>("ports");
```

### 5. 传统索引访问方式
```cpp
// 使用点号+索引的方式访问
size_t server_count = config.get_array_size("servers");
for (size_t i = 0; i < server_count; ++i) {
    std::string name = config.get<std::string>("servers." + std::to_string(i) + ".name");
    std::string host = config.get<std::string>("servers." + std::to_string(i) + ".host");
    int port = config.get<int>("servers." + std::to_string(i) + ".port");
    bool enabled = config.get<bool>("servers." + std::to_string(i) + ".enabled");
    
    std::cout << "Server: " << name << " @ " << host << ":" << port 
              << " (enabled: " << (enabled ? "yes" : "no") << ")" << std::endl;
}
```

## JSON对象操作

### 1. 获取JSON值
```cpp
// 获取任意类型的JSON值
nlohmann::json redis_config = config.get_json_value("redis");
nlohmann::json servers_array = config.get_json_value("servers");
nlohmann::json first_server = config.get_json_value("servers.0");

// 检查值的类型
if (redis_config.is_object()) {
    std::cout << "Redis config: " << redis_config.dump(2) << std::endl;
}
```

### 2. 获取JSON对象（确保是对象类型）
```cpp
// 只返回对象类型，非对象返回默认值
nlohmann::json redis_obj = config.get_json_object("redis");
nlohmann::json mysql_obj = config.get_json_object("mysql");

// 使用自定义默认值
nlohmann::json custom_default = nlohmann::json::object({{"error", "not_found"}});
nlohmann::json result = config.get_json_object("non_existent", custom_default);
```

### 3. 获取原始JSON对象
```cpp
// 获取整个配置文件的JSON引用
const nlohmann::json& raw_json = config.get_raw_json();

// 直接使用nlohmann::json的功能
for (const auto& server : raw_json["servers"]) {
    std::cout << "Server: " << server["name"] << std::endl;
}

// 遍历对象
for (const auto& [key, value] : raw_json["redis"].items()) {
    std::cout << key << " = " << value << std::endl;
}
```

### 4. JSON字符串输出
```cpp
// 获取紧凑格式的JSON字符串
std::string compact = config.get_json_string();

// 获取格式化的JSON字符串（4个空格缩进）
std::string formatted = config.get_json_string(4);

// 获取特定部分的JSON字符串
std::string redis_str = config.get_json_string("redis", 2);
std::string servers_str = config.get_json_string("servers", 2);
```

## 配置管理

### 1. 动态设置配置值
```cpp
ConfigManager config("config.json");

// 设置不同类型的值
config.set("app.name", std::string("MyApp"));
config.set("app.version", 1.0);
config.set("app.debug", true);
config.set("app.max_connections", 1000);

// 设置嵌套配置
config.set("database.host", std::string("localhost"));
config.set("database.port", 5432);
```

### 2. 保存配置到文件
```cpp
// 保存修改后的配置
config.save();

// 配置会以格式化的JSON格式保存
```

### 3. 配置验证
```cpp
// 检查必需的配置项
std::vector<std::string> required_keys = {
    "redis.host", "redis.port",
    "mysql.host", "mysql.user", "mysql.database"
};

bool all_present = true;
for (const std::string& key : required_keys) {
    if (!config.has_key(key)) {
        std::cerr << "Missing required config: " << key << std::endl;
        all_present = false;
    }
}

if (!all_present) {
    throw std::runtime_error("Configuration validation failed");
}
```

## 错误处理

### 1. 文件不存在处理
```cpp
try {
    ConfigManager config("non_existent.json");
    // 如果文件不存在，会创建空的配置对象
} catch (const std::exception& e) {
    std::cerr << "Config error: " << e.what() << std::endl;
}
```

### 2. 类型转换错误处理
```cpp
// 使用默认值避免类型转换错误
int port = config.get<int>("redis.port", 6379);  // 如果转换失败，使用6379

// 或者先检查键存在性
if (config.has_key("redis.port")) {
    try {
        int port = config.get<int>("redis.port");
    } catch (...) {
        // 处理转换错误
        int port = 6379;  // 使用默认值
    }
}
```

### 3. 数组越界处理
```cpp
// get_array_item 会自动处理越界情况，返回默认值
nlohmann::json item = config.get_array_item<nlohmann::json>("servers", 999);
if (item.is_null()) {
    std::cout << "Index out of bounds" << std::endl;
}
```

## 实际应用示例

### 1. 数据库连接池配置
```cpp
class DatabaseManager {
private:
    ConfigManager config_;
    
public:
    DatabaseManager(const std::string& config_file) : config_(config_file) {}
    
    void initialize() {
        // MySQL配置
        std::string mysql_host = config_.get<std::string>("mysql.host");
        int mysql_port = config_.get<int>("mysql.port", 3306);
        std::string mysql_user = config_.get<std::string>("mysql.user");
        std::string mysql_password = config_.get<std::string>("mysql.password");
        std::string mysql_database = config_.get<std::string>("mysql.database");
        
        // 创建MySQL连接池
        mysql_pool_ = std::make_unique<MySQLPool>(
            mysql_host, mysql_port, mysql_user, mysql_password, mysql_database
        );
        
        // Redis配置
        std::string redis_host = config_.get<std::string>("redis.host");
        int redis_port = config_.get<int>("redis.port", 6379);
        
        // 创建Redis连接
        redis_client_ = std::make_unique<RedisClient>(redis_host, redis_port);
    }
};
```

### 2. 服务器集群管理
```cpp
class ServerCluster {
private:
    ConfigManager config_;
    std::vector<std::unique_ptr<ServerConnection>> servers_;
    
public:
    ServerCluster(const std::string& config_file) : config_(config_file) {}
    
    void loadServers() {
        size_t server_count = config_.get_array_size("servers");
        
        for (size_t i = 0; i < server_count; ++i) {
            bool enabled = config_.get<bool>("servers." + std::to_string(i) + ".enabled");
            if (!enabled) continue;
            
            std::string name = config_.get<std::string>("servers." + std::to_string(i) + ".name");
            std::string host = config_.get<std::string>("servers." + std::to_string(i) + ".host");
            int port = config_.get<int>("servers." + std::to_string(i) + ".port");
            
            auto server = std::make_unique<ServerConnection>(name, host, port);
            if (server->connect()) {
                servers_.push_back(std::move(server));
                std::cout << "Connected to server: " << name << std::endl;
            } else {
                std::cerr << "Failed to connect to server: " << name << std::endl;
            }
        }
    }
    
    void updateServerStatus(const std::string& server_name, bool enabled) {
        size_t server_count = config_.get_array_size("servers");
        for (size_t i = 0; i < server_count; ++i) {
            std::string name = config_.get<std::string>("servers." + std::to_string(i) + ".name");
            if (name == server_name) {
                config_.set("servers." + std::to_string(i) + ".enabled", enabled);
                config_.save();
                break;
            }
        }
    }
};
```

### 3. 路由配置管理
```cpp
class RouterManager {
private:
    ConfigManager config_;
    std::unordered_map<std::string, std::string> url_to_service_;
    std::unordered_map<int, std::string> cmd_to_service_;
    
public:
    RouterManager(const std::string& config_file) : config_(config_file) {
        loadRoutes();
    }
    
    void loadRoutes() {
        // 加载HTTP路由
        size_t route_count = config_.get_array_size("http_router.routes");
        for (size_t i = 0; i < route_count; ++i) {
            std::string url = config_.get<std::string>("http_router.routes." + std::to_string(i) + ".url");
            std::string service = config_.get<std::string>("http_router.routes." + std::to_string(i) + ".service");
            url_to_service_[url] = service;
        }
        
        // 加载命令路由
        nlohmann::json cmd_router = config_.get_json_object("cmd_router");
        for (const auto& [cmd_str, service] : cmd_router.items()) {
            int cmd_id = std::stoi(cmd_str);
            cmd_to_service_[cmd_id] = service.get<std::string>();
        }
    }
    
    std::string getServiceByUrl(const std::string& url) {
        auto it = url_to_service_.find(url);
        return (it != url_to_service_.end()) ? it->second : "";
    }
    
    std::string getServiceByCmd(int cmd_id) {
        auto it = cmd_to_service_.find(cmd_id);
        return (it != cmd_to_service_.end()) ? it->second : "";
    }
};
```

### 4. 应用程序设置管理
```cpp
class AppSettings {
private:
    ConfigManager config_;
    
public:
    AppSettings(const std::string& config_file) : config_(config_file) {}
    
    // 日志配置
    struct LogConfig {
        std::string level;
        std::string file_path;
        int max_file_size;
        int max_files;
    };
    
    LogConfig getLogConfig() {
        return {
            config_.get<std::string>("logging.level", "INFO"),
            config_.get<std::string>("logging.file", "app.log"),
            config_.get<int>("logging.max_size", 10485760),  // 10MB
            config_.get<int>("logging.max_files", 5)
        };
    }
    
    // 性能配置
    struct PerformanceConfig {
        int thread_pool_size;
        int connection_pool_size;
        int request_timeout;
        bool enable_cache;
    };
    
    PerformanceConfig getPerformanceConfig() {
        return {
            config_.get<int>("performance.threads", std::thread::hardware_concurrency()),
            config_.get<int>("performance.connection_pool", 10),
            config_.get<int>("performance.timeout", 30000),  // 30秒
            config_.get<bool>("performance.cache", true)
        };
    }
    
    // 安全配置
    std::vector<std::string> getAllowedIPs() {
        return config_.get_array<std::string>("security.allowed_ips");
    }
    
    bool isIPAllowed(const std::string& ip) {
        auto allowed_ips = getAllowedIPs();
        return std::find(allowed_ips.begin(), allowed_ips.end(), ip) != allowed_ips.end();
    }
};
```

## API参考

### 构造函数
```cpp
ConfigManager(std::string path)
```
- **参数**: `path` - 配置文件路径
- **说明**: 创建配置管理器并加载指定的JSON配置文件

### 基础读取方法

#### get (字符串版本)
```cpp
std::string get(std::string key)
```
- **参数**: `key` - 配置键路径
- **返回**: 配置值的字符串表示，不存在时返回空字符串
- **示例**: `config.get("redis.host")`

#### get (模板版本)
```cpp
template<typename T>
T get(const std::string& key, const T& default_value = T{}) const
```
- **参数**: 
  - `key` - 配置键路径
  - `default_value` - 默认值（可选）
- **返回**: 指定类型的配置值
- **示例**: `config.get<int>("redis.port", 6379)`

#### has_key
```cpp
bool has_key(const std::string& key) const
```
- **参数**: `key` - 配置键路径
- **返回**: 键是否存在
- **示例**: `config.has_key("redis.host")`

### 数组操作方法

#### get_array_size
```cpp
size_t get_array_size(const std::string& key) const
```
- **参数**: `key` - 数组路径
- **返回**: 数组大小，不存在时返回0
- **示例**: `config.get_array_size("servers")`

#### get_array_item
```cpp
template<typename T>
T get_array_item(const std::string& key, size_t index, const T& default_value = T{}) const
```
- **参数**:
  - `key` - 数组路径
  - `index` - 数组索引
  - `default_value` - 默认值（可选）
- **返回**: 指定索引的数组元素
- **示例**: `config.get_array_item<std::string>("allowed_ips", 0)`

#### get_array
```cpp
template<typename T>
std::vector<T> get_array(const std::string& key) const
```
- **参数**: `key` - 数组路径
- **返回**: 整个数组的vector
- **示例**: `config.get_array<std::string>("tags")`

#### get_array_field
```cpp
template<typename T>
std::vector<T> get_array_field(const std::string& array_key, const std::string& field_key) const
```
- **参数**:
  - `array_key` - 对象数组路径
  - `field_key` - 要提取的字段名
- **返回**: 所有对象中指定字段的值组成的vector
- **示例**: `config.get_array_field<std::string>("servers", "name")`

### JSON对象操作方法

#### get_json_value
```cpp
nlohmann::json get_json_value(const std::string& key) const
```
- **参数**: `key` - 配置键路径
- **返回**: JSON值，不存在时返回null
- **示例**: `config.get_json_value("redis")`

#### get_json_object
```cpp
nlohmann::json get_json_object(const std::string& key, const nlohmann::json& default_value = nlohmann::json::object()) const
```
- **参数**:
  - `key` - 配置键路径
  - `default_value` - 默认值（可选）
- **返回**: JSON对象，非对象类型时返回默认值
- **示例**: `config.get_json_object("mysql")`

#### get_raw_json
```cpp
const nlohmann::json& get_raw_json() const
```
- **返回**: 整个配置文件的JSON对象引用
- **示例**: `const auto& json = config.get_raw_json()`

#### get_json_string
```cpp
std::string get_json_string(int indent = -1) const
std::string get_json_string(const std::string& key, int indent = -1) const
```
- **参数**:
  - `key` - 配置键路径（第二个重载）
  - `indent` - 缩进空格数，-1表示紧凑格式
- **返回**: JSON字符串表示
- **示例**: `config.get_json_string("servers", 2)`

### 配置修改方法

#### set
```cpp
template<typename T>
void set(const std::string& key, const T& value)
```
- **参数**:
  - `key` - 配置键路径
  - `value` - 要设置的值
- **示例**: `config.set("redis.port", 6380)`

#### save
```cpp
void save()
```
- **说明**: 将当前配置保存到文件
- **示例**: `config.save()`

## 最佳实践

### 1. 配置文件组织
```json
{
    "app": {
        "name": "MyApplication",
        "version": "1.0.0",
        "debug": false
    },
    "database": {
        "mysql": {
            "host": "localhost",
            "port": 3306,
            "user": "root",
            "password": "",
            "database": "myapp"
        },
        "redis": {
            "host": "127.0.0.1",
            "port": 6379,
            "db": 0
        }
    },
    "servers": [
        {
            "name": "web-1",
            "host": "192.168.1.10",
            "port": 8080,
            "enabled": true,
            "weight": 100
        }
    ],
    "security": {
        "allowed_ips": ["127.0.0.1", "192.168.1.0/24"],
        "rate_limit": {
            "requests_per_minute": 1000,
            "burst_size": 100
        }
    },
    "logging": {
        "level": "INFO",
        "file": "logs/app.log",
        "max_size": 10485760,
        "max_files": 5
    }
}
```

### 2. 配置验证
```cpp
class ConfigValidator {
public:
    static void validate(const ConfigManager& config) {
        // 检查必需配置
        std::vector<std::string> required_keys = {
            "app.name",
            "database.mysql.host",
            "database.redis.host"
        };
        
        for (const std::string& key : required_keys) {
            if (!config.has_key(key)) {
                throw std::runtime_error("Missing required config: " + key);
            }
        }
        
        // 检查数值范围
        int mysql_port = config.get<int>("database.mysql.port", 3306);
        if (mysql_port < 1 || mysql_port > 65535) {
            throw std::runtime_error("Invalid MySQL port: " + std::to_string(mysql_port));
        }
        
        // 检查数组不为空
        if (config.get_array_size("servers") == 0) {
            throw std::runtime_error("No servers configured");
        }
    }
};
```

### 3. 配置热重载
```cpp
class ConfigWatcher {
private:
    ConfigManager config_;
    std::filesystem::file_time_type last_write_time_;
    
public:
    ConfigWatcher(const std::string& config_file) 
        : config_(config_file) {
        updateLastWriteTime();
    }
    
    bool hasChanged() {
        auto current_time = std::filesystem::last_write_time(config_.getPath());
        return current_time != last_write_time_;
    }
    
    void reload() {
        if (hasChanged()) {
            config_ = ConfigManager(config_.getPath());  // 重新加载
            updateLastWriteTime();
            std::cout << "Configuration reloaded" << std::endl;
        }
    }
    
private:
    void updateLastWriteTime() {
        last_write_time_ = std::filesystem::last_write_time(config_.getPath());
    }
};
```

### 4. 环境相关配置
```cpp
class EnvironmentConfig {
private:
    ConfigManager config_;
    std::string env_;
    
public:
    EnvironmentConfig(const std::string& config_file) : config_(config_file) {
        env_ = std::getenv("APP_ENV") ? std::getenv("APP_ENV") : "production";
    }
    
    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) {
        // 先尝试环境特定的配置
        std::string env_key = env_ + "." + key;
        if (config_.has_key(env_key)) {
            return config_.get<T>(env_key, default_value);
        }
        
        // 回退到通用配置
        return config_.get<T>(key, default_value);
    }
};
```

### 5. 配置缓存
```cpp
class CachedConfigManager {
private:
    ConfigManager config_;
    mutable std::unordered_map<std::string, std::string> cache_;
    mutable std::mutex cache_mutex_;
    
public:
    CachedConfigManager(const std::string& config_file) : config_(config_file) {}
    
    template<typename T>
    T get(const std::string& key, const T& default_value = T{}) const {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        
        auto it = cache_.find(key);
        if (it != cache_.end()) {
            // 从缓存返回（需要类型转换）
            return convertFromString<T>(it->second);
        }
        
        // 从配置读取并缓存
        T value = config_.get<T>(key, default_value);
        cache_[key] = convertToString(value);
        return value;
    }
    
    void clearCache() {
        std::lock_guard<std::mutex> lock(cache_mutex_);
        cache_.clear();
    }
};
```

---

## 总结

ConfigManager提供了一个功能完整、易于使用的配置管理解决方案。它支持：

- **类型安全**: 模板方法确保类型正确性
- **灵活访问**: 支持点号路径和数组索引
- **数组操作**: 完整的数组处理功能
- **JSON原生**: 直接访问底层JSON对象
- **动态修改**: 运行时修改和保存配置
- **错误处理**: 优雅的错误处理和默认值支持

通过合理使用这些功能，你可以构建一个健壮、可维护的配置管理系统。

---

**版本**: 1.0  
**作者**: MyChat项目组  
**更新日期**: 2024年8月22日