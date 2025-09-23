/**
 * @file test_config_mgr_extended.cpp
 * @brief ConfigManager 扩展功能的单元测试
 * @details 测试配置管理器的环境变量加载、.env文件解析等新增功能
 */

#include <gtest/gtest.h>
#include "config_mgr.hpp"
#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <unordered_map>

using namespace im::utils;

class ConfigManagerExtendedTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试目录
        test_dir_ = std::filesystem::temp_directory_path() / "config_test";
        std::filesystem::create_directories(test_dir_);
        
        // 创建测试配置文件
        config_file_ = test_dir_ / "test_config.json";
        env_file_ = test_dir_ / "test.env";
        
        createTestConfigFile();
        createTestEnvFile();
        
        // 保存原始环境变量
        saveOriginalEnvVars();
    }
    
    void TearDown() override {
        // 恢复原始环境变量
        restoreOriginalEnvVars();
        
        // 清理测试文件
        std::filesystem::remove_all(test_dir_);
    }
    
    void createTestConfigFile() {
        std::ofstream config_file(config_file_);
        config_file << R"({
            "database": {
                "host": "localhost",
                "port": 5432,
                "name": "testdb",
                "ssl": true
            },
            "server": {
                "port": 8080,
                "timeout": 30.5,
                "workers": 4
            },
            "logging": {
                "level": "info",
                "file": "app.log"
            }
        })";
        config_file.close();
    }
    
    void createTestEnvFile() {
        std::ofstream env_file(env_file_);
        env_file << R"(# Test environment file
DATABASE_HOST=prod.database.com
DATABASE_PORT=3306
DATABASE_PASSWORD=secret123
SERVER_PORT=9000
SERVER_DEBUG=true
LOGGING_LEVEL=debug
EMPTY_VALUE=
QUOTED_VALUE="quoted string"
MULTILINE_VALUE="line1\nline2"
SPECIAL_CHARS=!@#$%^&*()
UNICODE_VALUE=你好世界🌍
)";
        env_file.close();
    }
    
    void saveOriginalEnvVars() {
        const std::vector<std::string> env_vars = {
            "DATABASE_HOST", "DATABASE_PORT", "DATABASE_PASSWORD",
            "SERVER_PORT", "SERVER_DEBUG", "LOGGING_LEVEL",
            "TEST_STRING", "TEST_INT", "TEST_FLOAT", "TEST_BOOL"
        };
        
        for (const auto& var : env_vars) {
            const char* value = std::getenv(var.c_str());
            if (value) {
                original_env_vars_[var] = value;
            }
        }
    }
    
    void restoreOriginalEnvVars() {
        // 清除测试设置的环境变量
        const std::vector<std::string> test_vars = {
            "DATABASE_HOST", "DATABASE_PORT", "DATABASE_PASSWORD",
            "SERVER_PORT", "SERVER_DEBUG", "LOGGING_LEVEL",
            "TEST_STRING", "TEST_INT", "TEST_FLOAT", "TEST_BOOL"
        };
        
        for (const auto& var : test_vars) {
            if (original_env_vars_.find(var) != original_env_vars_.end()) {
                setenv(var.c_str(), original_env_vars_[var].c_str(), 1);
            } else {
                unsetenv(var.c_str());
            }
        }
    }
    
    void setTestEnvVar(const std::string& name, const std::string& value) {
        setenv(name.c_str(), value.c_str(), 1);
    }
    
    void unsetTestEnvVar(const std::string& name) {
        unsetenv(name.c_str());
    }
    
    std::filesystem::path test_dir_;
    std::filesystem::path config_file_;
    std::filesystem::path env_file_;
    std::unordered_map<std::string, std::string> original_env_vars_;
};

// 测试基本构造函数（无环境变量加载）
TEST_F(ConfigManagerExtendedTest, BasicConstruction) {
    ConfigManager config(config_file_.string());
    
    EXPECT_EQ(config.get<std::string>("database.host"), "localhost");
    EXPECT_EQ(config.get<int>("database.port"), 5432);
    EXPECT_EQ(config.get<std::string>("database.name"), "testdb");
    EXPECT_TRUE(config.get<bool>("database.ssl"));
}

// 测试带环境变量加载的构造函数
TEST_F(ConfigManagerExtendedTest, ConstructionWithEnvLoading) {
    // 设置一些常用的环境变量（ConfigManager预定义的）
    setTestEnvVar("TEST_HOST", "env_host");
    setTestEnvVar("TEST_PORT", "9999");
    setTestEnvVar("TEST_DEBUG", "true");
    setTestEnvVar("TEST_LOG_LEVEL", "debug");
    
    ConfigManager config(config_file_.string(), true, "TEST");
    
    // 验证环境变量被加载（注意键名会转换为小写并加前缀）
    EXPECT_EQ(config.get<std::string>("test.host"), "env_host");
    EXPECT_EQ(config.get<int>("test.port"), 9999);
    EXPECT_TRUE(config.get<bool>("test.debug"));
    EXPECT_EQ(config.get<std::string>("test.log_level"), "debug");
    
    // 验证原有配置仍然存在
    EXPECT_EQ(config.get<std::string>("database.host"), "localhost");
}

// 测试 .env 文件加载
TEST_F(ConfigManagerExtendedTest, EnvFileLoading) {
    ConfigManager config(config_file_.string());
    
    bool success = config.loadEnvFile(env_file_.string());
    EXPECT_TRUE(success);
    
    // 验证 .env 文件中的值被加载
    EXPECT_EQ(config.get<std::string>("DATABASE_HOST"), "prod.database.com");
    EXPECT_EQ(config.get<int>("DATABASE_PORT"), 3306);
    EXPECT_EQ(config.get<std::string>("DATABASE_PASSWORD"), "secret123");
    EXPECT_EQ(config.get<int>("SERVER_PORT"), 9000);
    EXPECT_TRUE(config.get<bool>("SERVER_DEBUG"));
    EXPECT_EQ(config.get<std::string>("LOGGING_LEVEL"), "debug");
}

// 测试 .env 文件覆盖现有配置
TEST_F(ConfigManagerExtendedTest, EnvFileOverride) {
    ConfigManager config(config_file_.string());
    
    // 原始值
    EXPECT_EQ(config.get<int>("server.port"), 8080);
    
    // 加载 .env 文件但不覆盖
    config.loadEnvFile(env_file_.string(), false);
    EXPECT_EQ(config.get<int>("server.port"), 8080);  // 原值保持不变
    EXPECT_EQ(config.get<int>("SERVER_PORT"), 9000);  // 新值被添加
    
    // 加载 .env 文件并覆盖
    config.loadEnvFile(env_file_.string(), true);
    EXPECT_EQ(config.get<int>("SERVER_PORT"), 9000);  // 覆盖值
}

// 测试直接从系统环境变量获取
TEST_F(ConfigManagerExtendedTest, DirectEnvAccess) {
    setTestEnvVar("TEST_DIRECT", "direct_value");
    setTestEnvVar("TEST_DIRECT_INT", "123");
    setTestEnvVar("TEST_DIRECT_FLOAT", "45.67");
    setTestEnvVar("TEST_DIRECT_BOOL", "false");
    
    ConfigManager config(config_file_.string());
    
    // 测试 getEnv 方法
    EXPECT_EQ(config.getEnv<std::string>("TEST_DIRECT"), "direct_value");
    EXPECT_EQ(config.getEnv<int>("TEST_DIRECT_INT"), 123);
    EXPECT_FLOAT_EQ(config.getEnv<float>("TEST_DIRECT_FLOAT"), 45.67f);
    EXPECT_FALSE(config.getEnv<bool>("TEST_DIRECT_BOOL"));
    
    // 测试不存在的环境变量使用默认值
    EXPECT_EQ(config.getEnv<std::string>("NONEXISTENT", "default"), "default");
    EXPECT_EQ(config.getEnv<int>("NONEXISTENT", 999), 999);
}

// 测试 getWithEnv 方法（环境变量优先）
TEST_F(ConfigManagerExtendedTest, GetWithEnvPriority) {
    setTestEnvVar("DATABASE_HOST", "env.database.com");
    setTestEnvVar("DATABASE_PORT", "5433");
    
    ConfigManager config(config_file_.string());
    
    // 环境变量存在时，优先使用环境变量
    EXPECT_EQ(config.getWithEnv<std::string>("database.host", "DATABASE_HOST"), "env.database.com");
    EXPECT_EQ(config.getWithEnv<int>("database.port", "DATABASE_PORT"), 5433);
    
    // 环境变量不存在时，使用配置文件值
    EXPECT_EQ(config.getWithEnv<std::string>("database.name", "DATABASE_NAME"), "testdb");
    
    // 都不存在时，使用默认值
    EXPECT_EQ(config.getWithEnv<std::string>("nonexistent.key", "NONEXISTENT_ENV", "default_value"), 
              "default_value");
}

// 测试环境变量批量加载
TEST_F(ConfigManagerExtendedTest, BatchEnvLoading) {
    setTestEnvVar("APP_HOST", "app.server.com");
    setTestEnvVar("APP_PORT", "8443");
    setTestEnvVar("APP_DEBUG", "true");
    setTestEnvVar("OTHER_VALUE", "should_not_load");
    
    ConfigManager config(config_file_.string());
    
    // 加载带前缀的环境变量
    config.loadEnvironmentVariables("APP");
    
    // 验证带前缀的变量被加载（注意键名格式：app.host）
    EXPECT_EQ(config.get<std::string>("app.host"), "app.server.com");
    EXPECT_EQ(config.get<int>("app.port"), 8443);
    EXPECT_TRUE(config.get<bool>("app.debug"));
    
    // 验证不带前缀的变量未被加载（默认值为空字符串）
    EXPECT_EQ(config.get<std::string>("OTHER_VALUE"), "");
}

// 测试指定环境变量列表加载
TEST_F(ConfigManagerExtendedTest, SpecificEnvLoading) {
    setTestEnvVar("VAR1", "value1");
    setTestEnvVar("VAR2", "42");
    setTestEnvVar("VAR3", "true");
    setTestEnvVar("VAR4", "should_not_load");
    
    ConfigManager config(config_file_.string());
    
    std::vector<std::string> env_vars = {"VAR1", "VAR2", "VAR3"};
    config.loadEnvironmentVariables(env_vars);
    
    // 验证指定的变量被加载（注意键名会转换为小写）
    EXPECT_EQ(config.get<std::string>("var1"), "value1");
    EXPECT_EQ(config.get<int>("var2"), 42);
    EXPECT_TRUE(config.get<bool>("var3"));
    
    // 验证未指定的变量未被加载（默认值为空字符串）
    EXPECT_EQ(config.get<std::string>("var4"), "");
}

// 测试类型转换功能
TEST_F(ConfigManagerExtendedTest, TypeConversion) {
    ConfigManager config(config_file_.string());
    
    // 测试字符串到各种类型的转换
    config.setEnvironmentValue("test_int", "123");
    config.setEnvironmentValue("test_float", "45.67");
    config.setEnvironmentValue("test_bool_true", "true");
    config.setEnvironmentValue("test_bool_false", "false");
    config.setEnvironmentValue("test_bool_1", "true");
    config.setEnvironmentValue("test_bool_0", "false");
    config.setEnvironmentValue("test_string", "hello");
    
    EXPECT_EQ(config.get<int>("test_int"), 123);
    EXPECT_FLOAT_EQ(config.get<float>("test_float"), 45.67f);
    EXPECT_TRUE(config.get<bool>("test_bool_true"));
    EXPECT_FALSE(config.get<bool>("test_bool_false"));
    EXPECT_TRUE(config.get<bool>("test_bool_1"));
    EXPECT_FALSE(config.get<bool>("test_bool_0"));
    EXPECT_EQ(config.get<std::string>("test_string"), "hello");
}

// 测试无效类型转换
TEST_F(ConfigManagerExtendedTest, InvalidTypeConversion) {
    ConfigManager config(config_file_.string());
    
    config.setEnvironmentValue("invalid_int", "not_a_number");
    config.setEnvironmentValue("invalid_float", "not_a_float");
    config.setEnvironmentValue("invalid_bool", "maybe");
    
    // 无效转换应该保持原字符串值
    EXPECT_EQ(config.get<std::string>("invalid_int"), "not_a_number");
    EXPECT_EQ(config.get<std::string>("invalid_float"), "not_a_float");
    EXPECT_EQ(config.get<std::string>("invalid_bool"), "maybe");
}

// 测试特殊字符处理
TEST_F(ConfigManagerExtendedTest, SpecialCharacters) {
    ConfigManager config(config_file_.string());
    
    // 测试加载包含特殊字符的 .env 文件
    config.loadEnvFile(env_file_.string());
    
    EXPECT_EQ(config.get<std::string>("EMPTY_VALUE"), "");
    EXPECT_EQ(config.get<std::string>("QUOTED_VALUE"), "quoted string");
    EXPECT_EQ(config.get<std::string>("MULTILINE_VALUE"), "line1\\nline2");
    EXPECT_EQ(config.get<std::string>("SPECIAL_CHARS"), "!@#$%^&*()");
    EXPECT_EQ(config.get<std::string>("UNICODE_VALUE"), "你好世界🌍");
}

// 测试 .env 文件格式错误处理
TEST_F(ConfigManagerExtendedTest, MalformedEnvFile) {
    // 创建格式错误的 .env 文件
    std::filesystem::path bad_env_file = test_dir_ / "bad.env";
    std::ofstream bad_file(bad_env_file);
    bad_file << R"(VALID_KEY=valid_value
INVALID LINE WITHOUT EQUALS
=MISSING_KEY
KEY_WITH_SPACES IN NAME=value
)";
    bad_file.close();
    
    ConfigManager config(config_file_.string());
    
    // 应该能够加载，但跳过无效行
    bool success = config.loadEnvFile(bad_env_file.string());
    EXPECT_TRUE(success);
    
    // 验证有效行被加载
    EXPECT_EQ(config.get<std::string>("VALID_KEY"), "valid_value");
    
    // 验证无效行被跳过（返回默认值）
    EXPECT_EQ(config.get<std::string>("INVALID"), "");
}

// 测试不存在的 .env 文件
TEST_F(ConfigManagerExtendedTest, NonexistentEnvFile) {
    ConfigManager config(config_file_.string());
    
    bool success = config.loadEnvFile("nonexistent.env");
    EXPECT_FALSE(success);
    
    // 原有配置应该保持不变
    EXPECT_EQ(config.get<std::string>("database.host"), "localhost");
}

// 测试大型 .env 文件
TEST_F(ConfigManagerExtendedTest, LargeEnvFile) {
    std::filesystem::path large_env_file = test_dir_ / "large.env";
    std::ofstream large_file(large_env_file);
    
    // 创建包含大量变量的 .env 文件
    for (int i = 0; i < 1000; ++i) {
        large_file << "VAR" << i << "=value" << i << "\n";
    }
    large_file.close();
    
    ConfigManager config(config_file_.string());
    
    auto start_time = std::chrono::high_resolution_clock::now();
    bool success = config.loadEnvFile(large_env_file.string());
    auto end_time = std::chrono::high_resolution_clock::now();
    
    EXPECT_TRUE(success);
    
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end_time - start_time);
    EXPECT_LT(duration.count(), 1000);  // 应该在1秒内完成
    
    // 验证一些值
    EXPECT_EQ(config.get<std::string>("VAR0"), "value0");
    EXPECT_EQ(config.get<std::string>("VAR500"), "value500");
    EXPECT_EQ(config.get<std::string>("VAR999"), "value999");
    
    std::cout << "Loaded 1000 environment variables in " << duration.count() << "ms" << std::endl;
}

// 测试环境变量优先级
TEST_F(ConfigManagerExtendedTest, EnvironmentPriority) {
    // 设置系统环境变量
    setTestEnvVar("PRIORITY_TEST", "system_env");
    
    ConfigManager config(config_file_.string());
    
    // 1. 通过 setEnvironmentValue 设置
    config.setEnvironmentValue("PRIORITY_TEST", "set_env_value");
    EXPECT_EQ(config.get<std::string>("PRIORITY_TEST"), "set_env_value");
    
    // 2. 通过 .env 文件加载（应该覆盖）
    std::filesystem::path priority_env = test_dir_ / "priority.env";
    std::ofstream priority_file(priority_env);
    priority_file << "PRIORITY_TEST=env_file_value\n";
    priority_file.close();
    
    config.loadEnvFile(priority_env.string(), true);
    EXPECT_EQ(config.get<std::string>("PRIORITY_TEST"), "env_file_value");
    
    // 3. getEnv 应该返回系统环境变量
    EXPECT_EQ(config.getEnv<std::string>("PRIORITY_TEST"), "system_env");
    
    // 4. getWithEnv 应该优先使用系统环境变量
    EXPECT_EQ(config.getWithEnv<std::string>("some.key", "PRIORITY_TEST"), "system_env");
}

// 测试复杂的真实场景
TEST_F(ConfigManagerExtendedTest, RealWorldScenario) {
    // 模拟生产环境配置场景
    
    // 1. 设置系统环境变量（通常由容器编排工具设置）
    setTestEnvVar("DATABASE_URL", "postgresql://user:pass@prod.db:5432/myapp");
    setTestEnvVar("REDIS_URL", "redis://prod.cache:6379/0");
    setTestEnvVar("LOG_LEVEL", "warn");
    setTestEnvVar("ENVIRONMENT", "production");
    
    // 2. 创建应用特定的 .env 文件
    std::filesystem::path app_env = test_dir_ / "app.env";
    std::ofstream app_file(app_env);
    app_file << R"(# Application configuration
APP_NAME=MyAwesomeApp
APP_VERSION=1.2.3
SERVER_HOST=0.0.0.0
SERVER_PORT=8080
SERVER_WORKERS=8
CACHE_TTL=3600
FEATURE_FLAG_NEW_UI=true
FEATURE_FLAG_ANALYTICS=false
API_RATE_LIMIT=1000
SMTP_HOST=smtp.company.com
SMTP_PORT=587
)";
    app_file.close();
    
    // 3. 创建配置管理器，加载配置文件和环境变量
    ConfigManager config(config_file_.string(), true, "APP");
    bool env_loaded = config.loadEnvFile(app_env.string());
    EXPECT_TRUE(env_loaded);  // 确保.env文件加载成功
    
    // 4. 验证配置优先级和类型转换
    
    // 系统环境变量优先
    EXPECT_EQ(config.getWithEnv<std::string>("database.host", "DATABASE_URL"), 
              "postgresql://user:pass@prod.db:5432/myapp");
    EXPECT_EQ(config.getWithEnv<std::string>("logging.level", "LOG_LEVEL"), "warn");
    
    // .env 文件中的应用配置（使用简单的字符串get方法）
    EXPECT_EQ(config.get("APP_NAME"), "MyAwesomeApp");
    EXPECT_EQ(config.get("APP_VERSION"), "1.2.3");
    EXPECT_EQ(config.get("SERVER_HOST"), "0.0.0.0");
    EXPECT_EQ(config.get<int>("SERVER_PORT"), 8080);
    EXPECT_EQ(config.get<int>("SERVER_WORKERS"), 8);
    EXPECT_EQ(config.get<int>("CACHE_TTL"), 3600);
    
    // 功能标志
    EXPECT_TRUE(config.get<bool>("FEATURE_FLAG_NEW_UI"));
    EXPECT_FALSE(config.get<bool>("FEATURE_FLAG_ANALYTICS"));
    
    // 数值配置
    EXPECT_EQ(config.get<int>("API_RATE_LIMIT"), 1000);
    EXPECT_EQ(config.get<int>("SMTP_PORT"), 587);
    
    // 原始 JSON 配置仍然可用
    EXPECT_EQ(config.get<std::string>("database.name"), "testdb");
    EXPECT_TRUE(config.get<bool>("database.ssl"));
    
    // 使用环境变量覆盖配置文件值
    EXPECT_EQ(config.getWithEnv<int>("server.port", "SERVER_PORT"), 8080);  // 环境变量优先
    EXPECT_EQ(config.getWithEnv<float>("server.timeout", "SERVER_TIMEOUT", 30.5f), 30.5f);  // 使用默认值
}

// 测试边界情况和错误处理
TEST_F(ConfigManagerExtendedTest, EdgeCasesAndErrorHandling) {
    ConfigManager config(config_file_.string());
    
    // 测试空字符串键
    EXPECT_NO_THROW(config.setEnvironmentValue("", "empty_key_value"));
    
    // 测试空字符串值
    EXPECT_NO_THROW(config.setEnvironmentValue("empty_value_key", ""));
    EXPECT_EQ(config.get<std::string>("empty_value_key"), "");
    
    // 测试非常长的键和值
    std::string long_key(1000, 'a');
    std::string long_value(10000, 'b');
    EXPECT_NO_THROW(config.setEnvironmentValue(long_key, long_value));
    EXPECT_EQ(config.get<std::string>(long_key), long_value);
    
    // 测试包含特殊字符的键
    std::string special_key = "key.with-special_chars123";
    EXPECT_NO_THROW(config.setEnvironmentValue(special_key, "special_value"));
    EXPECT_EQ(config.get<std::string>(special_key), "special_value");
    
    // 测试数值边界值
    config.setEnvironmentValue("max_int", std::to_string(std::numeric_limits<int>::max()));
    config.setEnvironmentValue("min_int", std::to_string(std::numeric_limits<int>::min()));
    config.setEnvironmentValue("max_float", std::to_string(std::numeric_limits<float>::max()));
    config.setEnvironmentValue("min_float", std::to_string(std::numeric_limits<float>::lowest()));
    
    EXPECT_EQ(config.get<int>("max_int"), std::numeric_limits<int>::max());
    EXPECT_EQ(config.get<int>("min_int"), std::numeric_limits<int>::min());
    EXPECT_FLOAT_EQ(config.get<float>("max_float"), std::numeric_limits<float>::max());
    EXPECT_FLOAT_EQ(config.get<float>("min_float"), std::numeric_limits<float>::lowest());
}

// 测试多线程只读访问的安全性
// 注意：ConfigManager设计为启动时加载，运行时只读的使用模式
TEST_F(ConfigManagerExtendedTest, ConcurrentReadAccess) {
    ConfigManager config(config_file_.string());
    
    // 预先设置一些测试数据（模拟启动时配置）
    config.setEnvironmentValue("shared_key1", "shared_value1");
    config.setEnvironmentValue("shared_key2", "42");
    config.setEnvironmentValue("shared_key3", "true");
    
    const int num_threads = 10;
    const int reads_per_thread = 100;
    std::vector<std::thread> threads;
    std::atomic<int> success_count{0};
    
    // 多线程同时读取配置（这是实际使用场景）
    for (int i = 0; i < num_threads; ++i) {
        threads.emplace_back([&config, &success_count, reads_per_thread]() {
            for (int j = 0; j < reads_per_thread; ++j) {
                try {
                    // 读取预设的配置值
                    std::string val1 = config.get("shared_key1");
                    int val2 = config.get<int>("shared_key2", 0);
                    bool val3 = config.get<bool>("shared_key3", false);
                    
                    // 验证读取的值是正确的
                    if (val1 == "shared_value1" && val2 == 42 && val3 == true) {
                        success_count++;
                    }
                } catch (const std::exception& e) {
                    // 只读访问不应该有异常
                    std::cerr << "Unexpected exception in read-only access: " << e.what() << std::endl;
                }
            }
        });
    }
    
    // 等待所有线程完成
    for (auto& thread : threads) {
        thread.join();
    }
    
    int expected_successes = num_threads * reads_per_thread;
    EXPECT_EQ(success_count.load(), expected_successes);
    
    std::cout << "Concurrent read test: " << success_count.load() << "/" << expected_successes 
              << " reads successful" << std::endl;
    
    // 输出使用建议
    std::cout << "Note: ConfigManager is designed for 'load-once, read-many' pattern. "
              << "For thread safety, load configuration during startup and avoid "
              << "concurrent write operations during runtime." << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}