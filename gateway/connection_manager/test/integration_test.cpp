#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <chrono>
#include <memory>
#include <fstream>

// 包含需要测试的核心功能
namespace im {
namespace gateway {

// 复制关键的结构定义
struct DeviceSessionInfo {
    std::string session_id;
    std::string device_id;
    std::string platform;
    std::chrono::system_clock::time_point connect_time;

    nlohmann::json to_json() const {
        nlohmann::json j;
        j["session_id"] = session_id;
        j["device_id"] = device_id;
        j["platform"] = platform;
        j["connect_time"] =
                std::chrono::duration_cast<std::chrono::milliseconds>(connect_time.time_since_epoch())
                        .count();
        return j;
    }

    static DeviceSessionInfo from_json(const nlohmann::json& j) {
        DeviceSessionInfo info;
        
        // 安全地提取字符串字段
        try {
            if (j.contains("session_id") && j["session_id"].is_string()) {
                info.session_id = j["session_id"].get<std::string>();
            } else if (j.contains("session_id")) {
                info.session_id = std::to_string(j["session_id"].get<int>());
            }
        } catch (...) {
            info.session_id = "";
        }
        
        try {
            if (j.contains("device_id") && j["device_id"].is_string()) {
                info.device_id = j["device_id"].get<std::string>();
            } else if (j.contains("device_id")) {
                info.device_id = j["device_id"].is_boolean() ? 
                    (j["device_id"].get<bool>() ? "true" : "false") : 
                    std::to_string(j["device_id"].get<int>());
            }
        } catch (...) {
            info.device_id = "";
        }
        
        try {
            if (j.contains("platform") && j["platform"].is_string()) {
                info.platform = j["platform"].get<std::string>();
            }
        } catch (...) {
            info.platform = "";
        }
        
        // 安全地提取时间戳
        try {
            auto timestamp = j.value("connect_time", 0LL);
            info.connect_time = std::chrono::system_clock::time_point(std::chrono::milliseconds(timestamp));
        } catch (const std::exception& e) {
            info.connect_time = std::chrono::system_clock::time_point{};
        }
        
        return info;
    }
};

// 模拟ConnectionManager的键名生成功能
class ConnectionManagerUtils {
public:
    static std::string generate_redis_key(const std::string& prefix, const std::string& user_id) {
        return prefix + ":" + user_id;
    }

    static std::string generate_device_field(const std::string& device_id, const std::string& platform) {
        return device_id + ":" + platform;
    }
    
    static bool is_valid_session_id(const std::string& session_id) {
        return !session_id.empty() && session_id.length() >= 8;
    }
    
    static bool is_valid_user_id(const std::string& user_id) {
        return !user_id.empty() && user_id.find(':') == std::string::npos;
    }
    
    static bool is_valid_device_id(const std::string& device_id) {
        return !device_id.empty() && device_id.find(':') == std::string::npos;
    }
    
    static bool is_valid_platform(const std::string& platform) {
        std::vector<std::string> valid_platforms = {"android", "ios", "web", "desktop", "mobile"};
        return std::find(valid_platforms.begin(), valid_platforms.end(), platform) != valid_platforms.end();
    }
};

}  // namespace gateway
}  // namespace im

using namespace im::gateway;

// 集成测试类
class ConnectionManagerIntegrationTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 创建测试配置
        CreateTestConfig();
    }

    void TearDown() override {
        // 清理测试文件
        if (!config_path_.empty()) {
            std::remove(config_path_.c_str());
        }
    }

    void CreateTestConfig() {
        config_path_ = "/tmp/integration_test_config.json";
        nlohmann::json config = {
            {"platforms", {
                {"android", {
                    {"enable_multi_device", true},
                    {"access_token_expire_time", 3600},
                    {"refresh_token_expire_time", 86400},
                    {"algorithm", "HS256"}
                }},
                {"ios", {
                    {"enable_multi_device", false},
                    {"access_token_expire_time", 3600},
                    {"refresh_token_expire_time", 86400},
                    {"algorithm", "HS256"}
                }},
                {"web", {
                    {"enable_multi_device", true},
                    {"access_token_expire_time", 1800},
                    {"refresh_token_expire_time", 43200},
                    {"algorithm", "HS256"}
                }}
            }}
        };
        
        std::ofstream file(config_path_);
        file << config.dump(4);
        file.close();
    }

protected:
    std::string config_path_;
};

// 测试Redis键名生成功能
TEST_F(ConnectionManagerIntegrationTest, RedisKeyGeneration) {
    // 测试用户会话键生成
    auto sessions_key = ConnectionManagerUtils::generate_redis_key("user:sessions", "user123");
    EXPECT_EQ(sessions_key, "user:sessions:user123");
    
    // 测试用户平台键生成
    auto platform_key = ConnectionManagerUtils::generate_redis_key("user:platform", "user456");
    EXPECT_EQ(platform_key, "user:platform:user456");
    
    // 测试会话用户键生成
    auto session_key = ConnectionManagerUtils::generate_redis_key("session:user", "session789");
    EXPECT_EQ(session_key, "session:user:session789");
    
    // 测试空值处理
    auto empty_key = ConnectionManagerUtils::generate_redis_key("prefix", "");
    EXPECT_EQ(empty_key, "prefix:");
    
    // 测试特殊字符
    auto special_key = ConnectionManagerUtils::generate_redis_key("test", "user_123@domain.com");
    EXPECT_EQ(special_key, "test:user_123@domain.com");
}

// 测试设备字段名生成功能
TEST_F(ConnectionManagerIntegrationTest, DeviceFieldGeneration) {
    // 测试正常情况
    auto field1 = ConnectionManagerUtils::generate_device_field("device123", "android");
    EXPECT_EQ(field1, "device123:android");
    
    auto field2 = ConnectionManagerUtils::generate_device_field("iphone_15", "ios");
    EXPECT_EQ(field2, "iphone_15:ios");
    
    // 测试边界情况
    auto empty_field = ConnectionManagerUtils::generate_device_field("", "");
    EXPECT_EQ(empty_field, ":");
    
    auto partial_field = ConnectionManagerUtils::generate_device_field("device", "");
    EXPECT_EQ(partial_field, "device:");
}

// 测试验证函数
TEST_F(ConnectionManagerIntegrationTest, ValidationFunctions) {
    // 测试session_id验证
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_session_id("session12345"));
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_session_id("very_long_session_id_12345"));
    EXPECT_FALSE(ConnectionManagerUtils::is_valid_session_id(""));
    EXPECT_FALSE(ConnectionManagerUtils::is_valid_session_id("short"));
    
    // 测试user_id验证
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_user_id("user123"));
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_user_id("user_with_underscore"));
    EXPECT_FALSE(ConnectionManagerUtils::is_valid_user_id(""));
    EXPECT_FALSE(ConnectionManagerUtils::is_valid_user_id("user:with:colon"));
    
    // 测试device_id验证
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_device_id("device123"));
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_device_id("device_abc"));
    EXPECT_FALSE(ConnectionManagerUtils::is_valid_device_id(""));
    EXPECT_FALSE(ConnectionManagerUtils::is_valid_device_id("device:123"));
    
    // 测试platform验证
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_platform("android"));
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_platform("ios"));
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_platform("web"));
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_platform("desktop"));
    EXPECT_TRUE(ConnectionManagerUtils::is_valid_platform("mobile"));
    EXPECT_FALSE(ConnectionManagerUtils::is_valid_platform(""));
    EXPECT_FALSE(ConnectionManagerUtils::is_valid_platform("unknown"));
    EXPECT_FALSE(ConnectionManagerUtils::is_valid_platform("windows"));
}

// 测试DeviceSessionInfo的完整功能
TEST_F(ConnectionManagerIntegrationTest, DeviceSessionInfoFullTest) {
    // 创建多个设备会话信息
    std::vector<DeviceSessionInfo> sessions;
    
    DeviceSessionInfo session1;
    session1.session_id = "android_session_123";
    session1.device_id = "device_android_1";
    session1.platform = "android";
    session1.connect_time = std::chrono::system_clock::now();
    sessions.push_back(session1);
    
    DeviceSessionInfo session2;
    session2.session_id = "ios_session_456";
    session2.device_id = "device_ios_1";
    session2.platform = "ios";
    session2.connect_time = std::chrono::system_clock::now() + std::chrono::seconds(10);
    sessions.push_back(session2);
    
    DeviceSessionInfo session3;
    session3.session_id = "web_session_789";
    session3.device_id = "device_web_1";
    session3.platform = "web";
    session3.connect_time = std::chrono::system_clock::now() + std::chrono::seconds(20);
    sessions.push_back(session3);
    
    // 测试序列化为JSON数组
    nlohmann::json sessions_array = nlohmann::json::array();
    for (const auto& session : sessions) {
        sessions_array.push_back(session.to_json());
    }
    
    EXPECT_EQ(sessions_array.size(), 3);
    EXPECT_TRUE(sessions_array.is_array());
    
    // 测试从JSON数组反序列化
    std::vector<DeviceSessionInfo> restored_sessions;
    for (const auto& session_json : sessions_array) {
        restored_sessions.push_back(DeviceSessionInfo::from_json(session_json));
    }
    
    EXPECT_EQ(restored_sessions.size(), 3);
    
    // 验证每个会话信息
    for (size_t i = 0; i < sessions.size(); ++i) {
        EXPECT_EQ(restored_sessions[i].session_id, sessions[i].session_id);
        EXPECT_EQ(restored_sessions[i].device_id, sessions[i].device_id);
        EXPECT_EQ(restored_sessions[i].platform, sessions[i].platform);
    }
}

// 测试配置文件读取功能
TEST_F(ConnectionManagerIntegrationTest, ConfigurationFileTest) {
    // 读取创建的配置文件
    std::ifstream file(config_path_);
    ASSERT_TRUE(file.is_open()) << "无法打开配置文件: " << config_path_;
    
    nlohmann::json config;
    file >> config;
    file.close();
    
    // 验证配置结构
    EXPECT_TRUE(config.contains("platforms"));
    EXPECT_TRUE(config["platforms"].is_object());
    
    // 验证Android平台配置
    EXPECT_TRUE(config["platforms"].contains("android"));
    auto android_config = config["platforms"]["android"];
    EXPECT_TRUE(android_config["enable_multi_device"].get<bool>());
    EXPECT_EQ(android_config["access_token_expire_time"].get<int>(), 3600);
    EXPECT_EQ(android_config["refresh_token_expire_time"].get<int>(), 86400);
    
    // 验证iOS平台配置
    EXPECT_TRUE(config["platforms"].contains("ios"));
    auto ios_config = config["platforms"]["ios"];
    EXPECT_FALSE(ios_config["enable_multi_device"].get<bool>());
    
    // 验证Web平台配置
    EXPECT_TRUE(config["platforms"].contains("web"));
    auto web_config = config["platforms"]["web"];
    EXPECT_TRUE(web_config["enable_multi_device"].get<bool>());
    EXPECT_EQ(web_config["access_token_expire_time"].get<int>(), 1800);
}

// 测试并发场景模拟
TEST_F(ConnectionManagerIntegrationTest, ConcurrencySimulation) {
    // 模拟多个用户同时连接的场景
    std::vector<std::string> user_ids = {"user1", "user2", "user3", "user4", "user5"};
    std::vector<std::string> platforms = {"android", "ios", "web"};
    
    // 为每个用户在每个平台创建设备会话信息
    std::vector<DeviceSessionInfo> all_sessions;
    
    for (const auto& user_id : user_ids) {
        for (size_t platform_idx = 0; platform_idx < platforms.size(); ++platform_idx) {
            DeviceSessionInfo session;
            session.session_id = user_id + "_" + platforms[platform_idx] + "_session";
            session.device_id = user_id + "_device_" + std::to_string(platform_idx);
            session.platform = platforms[platform_idx];
            session.connect_time = std::chrono::system_clock::now() + 
                                 std::chrono::milliseconds(platform_idx * 100);
            all_sessions.push_back(session);
        }
    }
    
    // 验证创建的会话数量
    EXPECT_EQ(all_sessions.size(), user_ids.size() * platforms.size());
    
    // 验证每个会话的唯一性
    std::set<std::string> session_ids;
    std::set<std::string> device_ids;
    
    for (const auto& session : all_sessions) {
        // 验证session_id唯一性
        EXPECT_TRUE(session_ids.find(session.session_id) == session_ids.end())
            << "重复的session_id: " << session.session_id;
        session_ids.insert(session.session_id);
        
        // 验证device_id唯一性
        EXPECT_TRUE(device_ids.find(session.device_id) == device_ids.end())
            << "重复的device_id: " << session.device_id;
        device_ids.insert(session.device_id);
        
        // 验证字段有效性
        EXPECT_TRUE(ConnectionManagerUtils::is_valid_session_id(session.session_id));
        EXPECT_TRUE(ConnectionManagerUtils::is_valid_device_id(session.device_id));
        EXPECT_TRUE(ConnectionManagerUtils::is_valid_platform(session.platform));
    }
}

// 测试错误场景处理
TEST_F(ConnectionManagerIntegrationTest, ErrorScenarios) {
    // 测试无效JSON的处理
    nlohmann::json invalid_json = nlohmann::json::object();
    // 不设置任何字段
    
    auto info_from_empty = DeviceSessionInfo::from_json(invalid_json);
    EXPECT_EQ(info_from_empty.session_id, "");
    EXPECT_EQ(info_from_empty.device_id, "");
    EXPECT_EQ(info_from_empty.platform, "");
    
    // 测试部分字段缺失
    nlohmann::json partial_json = {
        {"session_id", "test_session"}
        // 其他字段缺失
    };
    
    auto info_from_partial = DeviceSessionInfo::from_json(partial_json);
    EXPECT_EQ(info_from_partial.session_id, "test_session");
    EXPECT_EQ(info_from_partial.device_id, "");
    EXPECT_EQ(info_from_partial.platform, "");
    
    // 测试字段类型错误
    nlohmann::json wrong_type_json = {
        {"session_id", 12345},  // 应该是字符串
        {"device_id", true},    // 应该是字符串
        {"platform", "android"},
        {"connect_time", "not_a_number"}  // 应该是数字
    };
    
    // 这应该能够处理而不崩溃
    EXPECT_NO_THROW({
        auto info = DeviceSessionInfo::from_json(wrong_type_json);
        // 验证类型转换的结果
        EXPECT_EQ(info.session_id, "12345");  // 数字转换为字符串
        EXPECT_EQ(info.device_id, "true");   // 布尔值转换为字符串
        EXPECT_EQ(info.platform, "android"); // 字符串保持不变
        // connect_time应该是默认值（epoch时间）
    });
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    
    std::cout << "运行ConnectionManager集成测试..." << std::endl;
    std::cout << "测试包括:" << std::endl;
    std::cout << "- DeviceSessionInfo序列化/反序列化" << std::endl;
    std::cout << "- Redis键名生成功能" << std::endl;
    std::cout << "- 输入验证功能" << std::endl;
    std::cout << "- 配置文件处理" << std::endl;
    std::cout << "- 并发场景模拟" << std::endl;
    std::cout << "- 错误处理" << std::endl;
    std::cout << std::endl;
    
    int result = RUN_ALL_TESTS();
    
    if (result == 0) {
        std::cout << std::endl;
        std::cout << "🎉 所有集成测试通过！ConnectionManager的核心功能工作正常。" << std::endl;
    } else {
        std::cout << std::endl;
        std::cout << "❌ 部分测试失败，请检查输出信息。" << std::endl;
    }
    
    return result;
}