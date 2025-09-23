# 配置管理器 (ConfigManager) 文档

## 概述

`ConfigManager` 是一个功能强大的配置管理器类，支持从 JSON 配置文件和环境变量加载配置，并提供类型安全的访问接口。该类使用 nlohmann/json 库处理 JSON 数据，支持嵌套配置、数组操作以及环境变量集成。

## 功能特性

- **JSON 配置文件支持**：从 JSON 文件加载和保存配置
- **环境变量集成**：支持从系统环境变量加载配置
- **.env 文件支持**：可以从 .env 文件加载环境变量
- **类型安全**：提供模板化的获取和设置方法，支持多种数据类型
- **嵌套配置**：支持点号分隔的嵌套配置键（如 `database.host`）
- **数组操作**：支持获取数组大小、数组元素和数组字段
- **JSON 原生访问**：可以直接获取 JSON 对象和字符串
- **智能类型转换**：自动将字符串转换为适当的数据类型
- **配置优先级**：环境变量优先于配置文件

## 构造函数

### 基本构造函数

```cpp
ConfigManager(std::string path)
```

- **参数**：
  - `path`：配置文件路径
- **功能**：从指定的 JSON 配置文件初始化配置管理器

### 高级构造函数

```cpp
ConfigManager(std::string path, bool load_env, const std::string& env_prefix = "")
```

- **参数**：
  - `path`：配置文件路径
  - `load_env`：是否加载系统环境变量
  - `env_prefix`：环境变量前缀（可选）
- **功能**：从配置文件和环境变量初始化配置管理器

## 主要方法

### 配置获取方法

#### 基本获取方法

```cpp
std::string get(std::string key)
```

- **参数**：
  - `key`：配置键名
- **返回值**：配置值的字符串表示
- **功能**：获取配置值（向后兼容方法）

#### 模板化获取方法

```cpp
template<typename T>
T get(const std::string& key, const T& default_value = T{}) const
```

- **参数**：
  - `key`：配置键名
  - `default_value`：默认值（可选）
- **返回值**：指定类型的配置值
- **功能**：类型安全地获取配置值，支持多种数据类型

#### 环境变量优先获取方法

```cpp
template<typename T>
T getWithEnv(const std::string& key, const std::string& env_key = "", 
             const T& default_value = T{}) const
```

- **参数**：
  - `key`：配置键名
  - `env_key`：环境变量名（如果为空则使用 key）
  - `default_value`：默认值（可选）
- **返回值**：配置值（环境变量优先，其次配置文件，最后默认值）
- **功能**：获取配置值，优先使用环境变量

### 配置设置方法

#### 模板化设置方法

```cpp
template<typename T>
void set(const std::string& key, const T& value)
```

- **参数**：
  - `key`：配置键名
  - `value`：要设置的值
- **功能**：设置配置值，支持多种数据类型

### 配置保存方法

```cpp
void save()
```

- **功能**：将当前配置保存到 JSON 文件

### 数组操作方法

#### 获取数组大小

```cpp
size_t get_array_size(const std::string& key) const
```

- **参数**：
  - `key`：数组键名
- **返回值**：数组大小
- **功能**：获取指定数组的大小

#### 获取数组元素

```cpp
template<typename T>
T get_array_item(const std::string& key, size_t index, const T& default_value = T{}) const
```

- **参数**：
  - `key`：数组键名
  - `index`：元素索引
  - `default_value`：默认值（可选）
- **返回值**：指定类型的数组元素
- **功能**：获取数组中的指定元素

#### 获取整个数组

```cpp
template<typename T>
std::vector<T> get_array(const std::string& key) const
```

- **参数**：
  - `key`：数组键名
- **返回值**：包含所有元素的向量
- **功能**：获取整个数组

#### 获取对象数组中的特定字段

```cpp
template<typename T>
std::vector<T> get_array_field(const std::string& array_key, const std::string& field_key) const
```

- **参数**：
  - `array_key`：数组键名
  - `field_key`：字段名
- **返回值**：包含所有指定字段值的向量
- **功能**：从对象数组中提取特定字段的所有值

### JSON 原生访问方法

#### 检查键是否存在

```cpp
bool has_key(const std::string& key) const
```

- **参数**：
  - `key`：配置键名
- **返回值**：键是否存在
- **功能**：检查指定的配置键是否存在

#### 获取 JSON 值对象

```cpp
json get_json_value(const std::string& key) const
```

- **参数**：
  - `key`：配置键名
- **返回值**：JSON 值对象
- **功能**：获取指定键的 JSON 值对象

#### 获取 JSON 对象

```cpp
json get_json_object(const std::string& key, const json& default_value = json::object()) const
```

- **参数**：
  - `key`：配置键名
  - `default_value`：默认值（可选）
- **返回值**：JSON 对象
- **功能**：获取指定键的 JSON 对象

#### 获取原始 JSON 对象

```cpp
const json& get_raw_json() const
```

- **返回值**：整个配置的原始 JSON 对象
- **功能**：获取整个配置的原始 JSON 对象

#### 获取 JSON 字符串

```cpp
std::string get_json_string(int indent = -1) const
std::string get_json_string(const std::string& key, int indent = -1) const
```

- **参数**：
  - `key`：配置键名（可选）
  - `indent`：缩进空格数（-1 表示无缩进）
- **返回值**：JSON 字符串表示
- **功能**：获取 JSON 对象的字符串表示

### 环境变量方法

#### 从 .env 文件加载

```cpp
bool loadEnvFile(const std::string& env_file, bool override_existing = false)
```

- **参数**：
  - `env_file`：.env 文件路径
  - `override_existing`：是否覆盖已存在的配置
- **返回值**：加载是否成功
- **功能**：从 .env 文件加载环境变量

#### 获取环境变量

```cpp
template<typename T>
T getEnv(const std::string& key, const T& default_value = T{}) const
```

- **参数**：
  - `key`：环境变量名
  - `default_value`：默认值（可选）
- **返回值**：环境变量值
- **功能**：获取系统环境变量

#### 设置环境变量值

```cpp
void setEnvironmentValue(const std::string& key, const std::string& value)
```

- **参数**：
  - `key`：配置键名
  - `value`：值
- **功能**：设置环境变量到配置中

#### 加载系统环境变量

```cpp
void loadEnvironmentVariables(const std::string& prefix = "")
void loadEnvironmentVariables(const std::vector<std::string>& env_vars, 
                             const std::string& prefix = "")
```

- **参数**：
  - `prefix`：环境变量前缀（可选）
  - `env_vars`：环境变量名列表
- **功能**：加载系统环境变量到配置中

## 使用示例

### 基本使用

```cpp
#include "common/utils/config_mgr.hpp"

int main() {
    // 从配置文件初始化
    im::utils::ConfigManager config("config.json");
    
    // 获取配置值
    std::string host = config.get<std::string>("database.host", "localhost");
    int port = config.get<int>("database.port", 3306);
    bool debug = config.get<bool>("debug", false);
    
    // 设置配置值
    config.set("database.host", "newhost");
    config.set("database.port", 5432);
    
    // 保存配置
    config.save();
    
    return 0;
}
```

### 环境变量集成

```cpp
#include "common/utils/config_mgr.hpp"

int main() {
    // 从配置文件和环境变量初始化
    im::utils::ConfigManager config("config.json", true, "MYAPP");
    
    // 获取配置值（环境变量优先）
    std::string host = config.getWithEnv<std::string>("database.host");
    int port = config.getWithEnv<int>("database.port", 3306);
    
    // 从 .env 文件加载
    config.loadEnvFile(".env");
    
    return 0;
}
```

### 数组操作

```cpp
#include "common/utils/config_mgr.hpp"

int main() {
    im::utils::ConfigManager config("config.json");
    
    // 获取数组大小
    size_t server_count = config.get_array_size("servers");
    
    // 获取数组元素
    std::string first_server = config.get_array_item<std::string>("servers", 0);
    
    // 获取整个数组
    std::vector<std::string> servers = config.get_array<std::string>("servers");
    
    // 获取对象数组中的特定字段
    std::vector<int> server_ports = config.get_array_field<int>("servers", "port");
    
    return 0;
}
```

### JSON 原生访问

```cpp
#include "common/utils/config_mgr.hpp"

int main() {
    im::utils::ConfigManager config("config.json");
    
    // 检查键是否存在
    if (config.has_key("database")) {
        // 获取 JSON 对象
        nlohmann::json db_config = config.get_json_object("database");
        
        // 获取 JSON 字符串
        std::string json_str = config.get_json_string("database", 4);
        
        // 获取原始 JSON 对象
        const nlohmann::json& raw_json = config.get_raw_json();
    }
    
    return 0;
}
```

### 复杂配置示例

```json
// config.json
{
    "database": {
        "host": "localhost",
        "port": 3306,
        "name": "myapp",
        "pool": {
            "min": 5,
            "max": 20
        }
    },
    "servers": [
        {
            "name": "server1",
            "port": 8080,
            "ssl": true
        },
        {
            "name": "server2",
            "port": 8081,
            "ssl": false
        }
    ],
    "logging": {
        "level": "info",
        "file": "/var/log/myapp.log"
    }
}
```

```cpp
#include "common/utils/config_mgr.hpp"

int main() {
    im::utils::ConfigManager config("config.json", true, "MYAPP");
    
    // 访问嵌套配置
    std::string db_host = config.get<std::string>("database.host");
    int db_port = config.get<int>("database.port");
    int pool_min = config.get<int>("database.pool.min");
    
    // 访问数组
    std::string first_server = config.get_array_item<std::string>("servers", 0, "default");
    std::vector<int> server_ports = config.get_array_field<int>("servers", "port");
    
    // 环境变量优先
    std::string log_level = config.getWithEnv<std::string>("logging.level", "info");
    
    return 0;
}
```

## 注意事项

1. **路径处理**：配置文件路径会被转换为绝对路径
2. **类型转换**：模板方法会尝试自动转换类型，转换失败时返回默认值
3. **环境变量**：C++ 标准库不提供遍历所有环境变量的方法，需要预定义环境变量列表
4. **JSON 指针**：内部使用 JSON 指针访问嵌套配置，键名中的点号会被转换为路径分隔符
5. **线程安全**：当前实现不是线程安全的，多线程环境下需要外部同步
6. **内存管理**：所有方法都使用值语义，返回的对象是副本
7. **错误处理**：方法内部捕获所有异常，确保不会抛出异常

## 依赖项

- nlohmann/json：JSON 处理库
- boost/algorithm：字符串处理
- C++17 或更高版本（需要 std::filesystem）
