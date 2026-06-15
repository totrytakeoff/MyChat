# CLIParser 命令行参数解析器详细指南

## 概述

`CLIParser` 是一个功能强大的命令行参数解析器，支持回调机制、自动类型转换、参数验证和帮助信息生成。它基于 `getopt_long` 实现，提供现代化的 C++ 接口。

## 核心特性

- **回调驱动**: 每个参数都可以注册处理回调函数
- **类型安全**: 支持多种参数类型和自动验证
- **自动帮助**: 自动生成格式化的帮助信息
- **参数分组**: 支持参数分组显示，提高可读性
- **灵活配置**: 支持长短选项、必需/可选参数
- **错误处理**: 详细的错误信息和验证机制

## API 参考

### 核心类型

```cpp
namespace im::utils {
    enum class ArgumentType {
        FLAG,           // 标志参数（无值）
        STRING,         // 字符串参数
        INTEGER,        // 整数参数
        FLOAT,          // 浮点数参数
        BOOLEAN         // 布尔参数
    };
    
    using ArgumentCallback = std::function<bool(const std::string& value)>;
    
    struct ParseResult {
        bool success;
        std::string error_message;
        std::vector<std::string> remaining_args;
    };
}
```

### CLIParser 类

```cpp
class CLIParser {
public:
    explicit CLIParser(const std::string& program_name, 
                      const std::string& program_description = "");
    
    bool addArgument(const std::string& long_name, char short_name, 
                    ArgumentType type, bool required, 
                    const std::string& description,
                    const std::string& default_value = "",
                    const std::string& group = "General",
                    ArgumentCallback callback = nullptr);
    
    void addVersionArgument(const std::string& version, 
                           ArgumentCallback callback = nullptr);
    
    ParseResult parse(int argc, char* argv[]);
    
    void printHelp() const;
    void printVersion() const;
};
```

### 方法详解

#### `CLIParser()` 构造函数
创建命令行解析器实例。

```cpp
CLIParser parser("myapp", "My Application Description");
```

#### `addArgument()` 添加参数
注册一个命令行参数。

**参数**:
- `long_name`: 长选项名 (如 "help")
- `short_name`: 短选项名 (如 'h'，0表示无短选项)
- `type`: 参数类型
- `required`: 是否必需
- `description`: 参数描述
- `default_value`: 默认值
- `group`: 参数分组
- `callback`: 处理回调函数

```cpp
parser.addArgument("port", 'p', ArgumentType::INTEGER, false,
                  "Server port number", "8080", "Network",
                  [](const std::string& value) -> bool {
                      int port = std::stoi(value);
                      if (port < 1 || port > 65535) {
                          std::cerr << "Invalid port range" << std::endl;
                          return false;
                      }
                      g_config.port = port;
                      return true;
                  });
```

#### `addVersionArgument()` 添加版本参数
添加版本信息参数。

```cpp
parser.addVersionArgument("1.0.0");
// 或者自定义版本显示
parser.addVersionArgument("1.0.0", [](const std::string&) -> bool {
    std::cout << "MyApp v1.0.0 (Build 20250918)" << std::endl;
    return true;
});
```

#### `parse()` 解析参数
解析命令行参数。

```cpp
auto result = parser.parse(argc, argv);
if (!result.success) {
    std::cerr << "Error: " << result.error_message << std::endl;
    return 1;
}

// 处理剩余参数
for (const auto& arg : result.remaining_args) {
    std::cout << "Remaining arg: " << arg << std::endl;
}
```

## 使用示例

### 基本用法

```cpp
#include "common/utils/cli_parser.hpp"
#include <iostream>

using namespace im::utils;

struct Config {
    std::string host = "localhost";
    int port = 8080;
    bool verbose = false;
    std::string log_level = "info";
};

Config g_config;

int main(int argc, char* argv[]) {
    CLIParser parser("server", "HTTP Server Application");
    
    // 添加版本信息
    parser.addVersionArgument("1.0.0");
    
    // 网络配置
    parser.addArgument("host", 'H', ArgumentType::STRING, false,
                      "Server host address", "localhost", "Network",
                      [](const std::string& value) -> bool {
                          g_config.host = value;
                          return true;
                      });
    
    parser.addArgument("port", 'p', ArgumentType::INTEGER, false,
                      "Server port number", "8080", "Network",
                      [](const std::string& value) -> bool {
                          try {
                              int port = std::stoi(value);
                              if (port < 1 || port > 65535) {
                                  std::cerr << "Port must be between 1-65535" << std::endl;
                                  return false;
                              }
                              g_config.port = port;
                              return true;
                          } catch (...) {
                              std::cerr << "Invalid port number" << std::endl;
                              return false;
                          }
                      });
    
    // 日志配置
    parser.addArgument("verbose", 'v', ArgumentType::FLAG, false,
                      "Enable verbose output", "", "Logging",
                      [](const std::string&) -> bool {
                          g_config.verbose = true;
                          return true;
                      });
    
    parser.addArgument("log-level", 'l', ArgumentType::STRING, false,
                      "Set log level", "info", "Logging",
                      [](const std::string& value) -> bool {
                          if (value == "debug" || value == "info" || 
                              value == "warn" || value == "error") {
                              g_config.log_level = value;
                              return true;
                          }
                          std::cerr << "Invalid log level: " << value << std::endl;
                          return false;
                      });
    
    // 解析参数
    auto result = parser.parse(argc, argv);
    if (!result.success) {
        std::cerr << "Error: " << result.error_message << std::endl;
        return 1;
    }
    
    // 显示最终配置
    std::cout << "Server Configuration:" << std::endl;
    std::cout << "  Host: " << g_config.host << std::endl;
    std::cout << "  Port: " << g_config.port << std::endl;
    std::cout << "  Verbose: " << (g_config.verbose ? "yes" : "no") << std::endl;
    std::cout << "  Log Level: " << g_config.log_level << std::endl;
    
    return 0;
}
```

### 高级用法 - 复杂应用配置

```cpp
#include "common/utils/cli_parser.hpp"
#include <vector>
#include <fstream>

using namespace im::utils;

struct DatabaseConfig {
    std::string host = "localhost";
    int port = 3306;
    std::string username;
    std::string password;
    std::string database;
};

struct ServerConfig {
    std::string bind_address = "0.0.0.0";
    int http_port = 8080;
    int https_port = 8443;
    bool enable_ssl = false;
    std::string ssl_cert;
    std::string ssl_key;
    int worker_threads = 4;
};

struct AppConfig {
    DatabaseConfig db;
    ServerConfig server;
    std::string config_file;
    std::string log_file;
    bool daemon_mode = false;
    std::string pid_file;
};

AppConfig g_config;

bool validateConfigFile(const std::string& path) {
    std::ifstream file(path);
    return file.good();
}

int main(int argc, char* argv[]) {
    CLIParser parser("webapp", "Web Application Server");
    parser.addVersionArgument("2.1.0");
    
    // 配置文件
    parser.addArgument("config", 'c', ArgumentType::STRING, false,
                      "Configuration file path", "app.conf", "Configuration",
                      [](const std::string& value) -> bool {
                          if (!validateConfigFile(value)) {
                              std::cerr << "Configuration file not found: " << value << std::endl;
                              return false;
                          }
                          g_config.config_file = value;
                          return true;
                      });
    
    // 数据库配置组
    parser.addArgument("db-host", 0, ArgumentType::STRING, false,
                      "Database host", "localhost", "Database",
                      [](const std::string& value) -> bool {
                          g_config.db.host = value;
                          return true;
                      });
    
    parser.addArgument("db-port", 0, ArgumentType::INTEGER, false,
                      "Database port", "3306", "Database",
                      [](const std::string& value) -> bool {
                          try {
                              int port = std::stoi(value);
                              if (port < 1 || port > 65535) {
                                  std::cerr << "Invalid database port" << std::endl;
                                  return false;
                              }
                              g_config.db.port = port;
                              return true;
                          } catch (...) {
                              std::cerr << "Invalid database port number" << std::endl;
                              return false;
                          }
                      });
    
    parser.addArgument("db-user", 'u', ArgumentType::STRING, true,
                      "Database username", "", "Database",
                      [](const std::string& value) -> bool {
                          if (value.empty()) {
                              std::cerr << "Database username cannot be empty" << std::endl;
                              return false;
                          }
                          g_config.db.username = value;
                          return true;
                      });
    
    parser.addArgument("db-password", 'P', ArgumentType::STRING, false,
                      "Database password", "", "Database",
                      [](const std::string& value) -> bool {
                          g_config.db.password = value;
                          return true;
                      });
    
    parser.addArgument("db-name", 'd', ArgumentType::STRING, true,
                      "Database name", "", "Database",
                      [](const std::string& value) -> bool {
                          g_config.db.database = value;
                          return true;
                      });
    
    // 服务器配置组
    parser.addArgument("bind", 'b', ArgumentType::STRING, false,
                      "Bind address", "0.0.0.0", "Server",
                      [](const std::string& value) -> bool {
                          g_config.server.bind_address = value;
                          return true;
                      });
    
    parser.addArgument("http-port", 0, ArgumentType::INTEGER, false,
                      "HTTP port", "8080", "Server",
                      [](const std::string& value) -> bool {
                          try {
                              g_config.server.http_port = std::stoi(value);
                              return true;
                          } catch (...) {
                              return false;
                          }
                      });
    
    parser.addArgument("https-port", 0, ArgumentType::INTEGER, false,
                      "HTTPS port", "8443", "Server",
                      [](const std::string& value) -> bool {
                          try {
                              g_config.server.https_port = std::stoi(value);
                              return true;
                          } catch (...) {
                              return false;
                          }
                      });
    
    parser.addArgument("enable-ssl", 0, ArgumentType::FLAG, false,
                      "Enable SSL/TLS", "", "Server",
                      [](const std::string&) -> bool {
                          g_config.server.enable_ssl = true;
                          return true;
                      });
    
    parser.addArgument("ssl-cert", 0, ArgumentType::STRING, false,
                      "SSL certificate file", "", "Server",
                      [](const std::string& value) -> bool {
                          if (!value.empty() && !validateConfigFile(value)) {
                              std::cerr << "SSL certificate file not found" << std::endl;
                              return false;
                          }
                          g_config.server.ssl_cert = value;
                          return true;
                      });
    
    parser.addArgument("ssl-key", 0, ArgumentType::STRING, false,
                      "SSL private key file", "", "Server",
                      [](const std::string& value) -> bool {
                          if (!value.empty() && !validateConfigFile(value)) {
                              std::cerr << "SSL key file not found" << std::endl;
                              return false;
                          }
                          g_config.server.ssl_key = value;
                          return true;
                      });
    
    parser.addArgument("workers", 'w', ArgumentType::INTEGER, false,
                      "Number of worker threads", "4", "Server",
                      [](const std::string& value) -> bool {
                          try {
                              int workers = std::stoi(value);
                              if (workers < 1 || workers > 64) {
                                  std::cerr << "Worker threads must be 1-64" << std::endl;
                                  return false;
                              }
                              g_config.server.worker_threads = workers;
                              return true;
                          } catch (...) {
                              return false;
                          }
                      });
    
    // 运行时配置组
    parser.addArgument("daemon", 0, ArgumentType::FLAG, false,
                      "Run as daemon", "", "Runtime",
                      [](const std::string&) -> bool {
                          g_config.daemon_mode = true;
                          return true;
                      });
    
    parser.addArgument("pid-file", 0, ArgumentType::STRING, false,
                      "PID file path", "", "Runtime",
                      [](const std::string& value) -> bool {
                          g_config.pid_file = value;
                          return true;
                      });
    
    parser.addArgument("log-file", 0, ArgumentType::STRING, false,
                      "Log file path", "", "Runtime",
                      [](const std::string& value) -> bool {
                          g_config.log_file = value;
                          return true;
                      });
    
    // 解析参数
    auto result = parser.parse(argc, argv);
    if (!result.success) {
        std::cerr << "Error: " << result.error_message << std::endl;
        parser.printHelp();
        return 1;
    }
    
    // 验证SSL配置
    if (g_config.server.enable_ssl) {
        if (g_config.server.ssl_cert.empty() || g_config.server.ssl_key.empty()) {
            std::cerr << "SSL enabled but certificate or key file not specified" << std::endl;
            return 1;
        }
    }
    
    std::cout << "Application configured successfully!" << std::endl;
    return 0;
}
```

### 与其他工具类集成

```cpp
#include "common/utils/cli_parser.hpp"
#include "common/utils/config_mgr.hpp"
#include "common/utils/signal_handler.hpp"

using namespace im::utils;

struct AppConfig {
    std::string config_file = "app.json";
    std::string env_file = ".env";
    bool daemon_mode = false;
};

AppConfig g_config;
std::unique_ptr<ConfigManager> g_config_mgr;

int main(int argc, char* argv[]) {
    // 1. 命令行解析
    CLIParser parser("integrated_app", "Integrated Application Example");
    
    parser.addArgument("config", 'c', ArgumentType::STRING, false,
                      "Config file", "app.json", "Configuration",
                      [](const std::string& value) -> bool {
                          g_config.config_file = value;
                          return true;
                      });
    
    parser.addArgument("env-file", 'e', ArgumentType::STRING, false,
                      "Environment file", ".env", "Configuration",
                      [](const std::string& value) -> bool {
                          g_config.env_file = value;
                          return true;
                      });
    
    parser.addArgument("daemon", 'd', ArgumentType::FLAG, false,
                      "Run as daemon", "", "Runtime",
                      [](const std::string&) -> bool {
                          g_config.daemon_mode = true;
                          return true;
                      });
    
    auto result = parser.parse(argc, argv);
    if (!result.success) {
        std::cerr << result.error_message << std::endl;
        return 1;
    }
    
    // 2. 配置管理
    g_config_mgr = std::make_unique<ConfigManager>(g_config.config_file, true);
    g_config_mgr->loadEnvFile(g_config.env_file);
    
    // 3. 信号处理
    auto& signal_handler = SignalHandler::getInstance();
    signal_handler.registerGracefulShutdown([](int sig, const std::string& name) {
        std::cout << "Shutting down by " << name << std::endl;
    });
    
    // 应用逻辑
    std::cout << "Application started with integrated tools" << std::endl;
    signal_handler.waitForShutdown();
    
    return 0;
}
```

## 参数类型详解

### ArgumentType::FLAG
标志参数，不需要值。

```cpp
parser.addArgument("verbose", 'v', ArgumentType::FLAG, false,
                  "Enable verbose mode", "", "Debug",
                  [](const std::string& value) -> bool {
                      // value 总是 "true"
                      g_verbose = true;
                      return true;
                  });
```

### ArgumentType::STRING
字符串参数。

```cpp
parser.addArgument("output", 'o', ArgumentType::STRING, false,
                  "Output file", "output.txt", "Files",
                  [](const std::string& value) -> bool {
                      if (value.empty()) {
                          std::cerr << "Output file cannot be empty" << std::endl;
                          return false;
                      }
                      g_output_file = value;
                      return true;
                  });
```

### ArgumentType::INTEGER
整数参数，自动验证数字格式。

```cpp
parser.addArgument("count", 'n', ArgumentType::INTEGER, false,
                  "Number of items", "10", "Parameters",
                  [](const std::string& value) -> bool {
                      try {
                          int count = std::stoi(value);
                          if (count < 0) {
                              std::cerr << "Count must be non-negative" << std::endl;
                              return false;
                          }
                          g_count = count;
                          return true;
                      } catch (...) {
                          std::cerr << "Invalid integer: " << value << std::endl;
                          return false;
                      }
                  });
```

### ArgumentType::FLOAT
浮点数参数。

```cpp
parser.addArgument("threshold", 't', ArgumentType::FLOAT, false,
                  "Threshold value", "0.5", "Parameters",
                  [](const std::string& value) -> bool {
                      try {
                          float threshold = std::stof(value);
                          if (threshold < 0.0f || threshold > 1.0f) {
                              std::cerr << "Threshold must be 0.0-1.0" << std::endl;
                              return false;
                          }
                          g_threshold = threshold;
                          return true;
                      } catch (...) {
                          std::cerr << "Invalid float: " << value << std::endl;
                          return false;
                      }
                  });
```

### ArgumentType::BOOLEAN
布尔参数，支持多种格式。

```cpp
parser.addArgument("enabled", 0, ArgumentType::BOOLEAN, false,
                  "Enable feature", "true", "Features",
                  [](const std::string& value) -> bool {
                      // 支持: true/false, 1/0, yes/no, on/off
                      std::string lower = value;
                      std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
                      
                      bool enabled = (lower == "true" || lower == "1" || 
                                     lower == "yes" || lower == "on");
                      g_feature_enabled = enabled;
                      return true;
                  });
```

## 帮助信息自定义

### 自动生成的帮助信息
```bash
$ ./myapp --help
myapp - My Application Description

USAGE:
    myapp [OPTIONS]

General OPTIONS:
    -h, --help                    Show this help message
    -v, --version                 Show version information

Network OPTIONS:
    -H, --host <STRING>           Server host address (default: localhost)
    -p, --port <INT>              Server port number (default: 8080)

Logging OPTIONS:
        --verbose                 Enable verbose output
    -l, --log-level <STRING>      Set log level (default: info)
```

### 参数分组
使用 `group` 参数对选项进行分组：

```cpp
// 网络相关
parser.addArgument("host", 'H', ArgumentType::STRING, false, "Host", "localhost", "Network", callback);
parser.addArgument("port", 'p', ArgumentType::INTEGER, false, "Port", "8080", "Network", callback);

// 数据库相关
parser.addArgument("db-host", 0, ArgumentType::STRING, false, "DB Host", "localhost", "Database", callback);
parser.addArgument("db-port", 0, ArgumentType::INTEGER, false, "DB Port", "3306", "Database", callback);

// 日志相关
parser.addArgument("log-level", 'l', ArgumentType::STRING, false, "Log Level", "info", "Logging", callback);
parser.addArgument("log-file", 0, ArgumentType::STRING, false, "Log File", "", "Logging", callback);
```

## 错误处理最佳实践

### 1. 参数验证
```cpp
parser.addArgument("port", 'p', ArgumentType::INTEGER, false,
                  "Port number", "8080", "Network",
                  [](const std::string& value) -> bool {
                      try {
                          int port = std::stoi(value);
                          
                          // 范围检查
                          if (port < 1 || port > 65535) {
                              std::cerr << "Port must be in range 1-65535" << std::endl;
                              return false;
                          }
                          
                          // 权限检查
                          if (port < 1024 && getuid() != 0) {
                              std::cerr << "Ports < 1024 require root privileges" << std::endl;
                              return false;
                          }
                          
                          g_port = port;
                          return true;
                          
                      } catch (const std::invalid_argument&) {
                          std::cerr << "Invalid port number format" << std::endl;
                          return false;
                      } catch (const std::out_of_range&) {
                          std::cerr << "Port number out of range" << std::endl;
                          return false;
                      }
                  });
```

### 2. 文件路径验证
```cpp
parser.addArgument("config", 'c', ArgumentType::STRING, false,
                  "Config file", "app.conf", "Configuration",
                  [](const std::string& value) -> bool {
                      // 检查文件是否存在
                      if (!std::filesystem::exists(value)) {
                          std::cerr << "Config file does not exist: " << value << std::endl;
                          return false;
                      }
                      
                      // 检查是否可读
                      std::ifstream file(value);
                      if (!file.good()) {
                          std::cerr << "Cannot read config file: " << value << std::endl;
                          return false;
                      }
                      
                      g_config_file = value;
                      return true;
                  });
```

### 3. 依赖关系检查
```cpp
// 在解析完成后进行依赖关系检查
auto result = parser.parse(argc, argv);
if (!result.success) {
    std::cerr << result.error_message << std::endl;
    return 1;
}

// 检查SSL配置依赖
if (g_enable_ssl && (g_ssl_cert.empty() || g_ssl_key.empty())) {
    std::cerr << "SSL enabled but certificate files not specified" << std::endl;
    parser.printHelp();
    return 1;
}

// 检查数据库配置
if (g_use_database && g_db_connection_string.empty()) {
    std::cerr << "Database enabled but connection string not provided" << std::endl;
    return 1;
}
```

## 性能优化

### 1. 回调函数优化
- 保持回调函数简短
- 避免复杂的计算
- 尽早返回错误

```cpp
// 好的做法
parser.addArgument("threads", 't', ArgumentType::INTEGER, false,
                  "Thread count", "4", "Performance",
                  [](const std::string& value) -> bool {
                      int threads = std::stoi(value);  // 可能抛出异常，但被parser捕获
                      if (threads < 1 || threads > 64) return false;
                      g_thread_count = threads;
                      return true;
                  });
```

### 2. 内存使用
- 解析完成后可以销毁parser对象
- 使用移动语义传递大对象

```cpp
{
    CLIParser parser("app", "description");
    // 添加参数...
    auto result = parser.parse(argc, argv);
    if (!result.success) return 1;
} // parser在这里被销毁，释放内存

// 继续应用逻辑...
```

## 高级技巧

### 1. 动态参数生成
```cpp
void setupDatabaseOptions(CLIParser& parser, const std::vector<std::string>& db_types) {
    for (const auto& type : db_types) {
        std::string option_name = type + "-config";
        std::string description = type + " database configuration";
        
        parser.addArgument(option_name, 0, ArgumentType::STRING, false,
                          description, "", "Database", 
                          [type](const std::string& value) -> bool {
                              g_db_configs[type] = value;
                              return true;
                          });
    }
}
```

### 2. 条件参数
```cpp
bool development_mode = false;

parser.addArgument("dev", 0, ArgumentType::FLAG, false,
                  "Development mode", "", "Debug",
                  [&](const std::string&) -> bool {
                      development_mode = true;
                      return true;
                  });

if (development_mode) {
    parser.addArgument("debug-port", 0, ArgumentType::INTEGER, false,
                      "Debug port", "9999", "Debug", debug_callback);
}
```

### 3. 参数别名
```cpp
// 为同一个功能提供多个选项名
auto callback = [](const std::string& value) -> bool {
    g_log_level = value;
    return true;
};

parser.addArgument("log-level", 'l', ArgumentType::STRING, false, "Log level", "info", "Logging", callback);
parser.addArgument("loglevel", 0, ArgumentType::STRING, false, "Log level (alias)", "info", "Logging", callback);
parser.addArgument("verbosity", 'v', ArgumentType::STRING, false, "Verbosity level", "info", "Logging", callback);
```

## 测试和调试

### 1. 单元测试
```cpp
#include <gtest/gtest.h>
#include "common/utils/cli_parser.hpp"

TEST(CLIParserTest, BasicParsing) {
    CLIParser parser("test", "Test app");
    
    bool callback_called = false;
    parser.addArgument("test", 't', ArgumentType::STRING, false, "Test arg", "", "Test",
                      [&](const std::string& value) -> bool {
                          callback_called = true;
                          EXPECT_EQ(value, "hello");
                          return true;
                      });
    
    const char* argv[] = {"test", "--test", "hello"};
    auto result = parser.parse(3, const_cast<char**>(argv));
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(callback_called);
}
```

### 2. 调试技巧
```cpp
// 启用调试输出
#ifdef DEBUG
parser.addArgument("debug-cli", 0, ArgumentType::FLAG, false,
                  "Debug CLI parsing", "", "Debug",
                  [](const std::string&) -> bool {
                      std::cout << "CLI debug mode enabled" << std::endl;
                      return true;
                  });
#endif

// 打印解析结果
auto result = parser.parse(argc, argv);
std::cout << "Parse result: " << (result.success ? "SUCCESS" : "FAILED") << std::endl;
if (!result.success) {
    std::cout << "Error: " << result.error_message << std::endl;
}
```

## 相关文档

- [Common Utils 总览](common-utils-guide.md)
- [SignalHandler 指南](signal-handler-guide.md)
- [ConfigManager 指南](config-manager-guide.md)
- [使用示例](../common/utils/example_usage.cpp)

---

*本文档版本: v1.0.0 | 最后更新: 2025-09-18*