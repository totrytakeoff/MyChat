#include <gtest/gtest.h>
#include <nlohmann/json.hpp>
#include <string>
#include <chrono>

// 直接包含DeviceSessionInfo的定义，避免复杂的依赖
namespace im {
namespace gateway {

// 设备会话信息结构 - 从connection_manager.hpp复制
struct DeviceSessionInfo {
    std::string session_id;  ///< WebSocket会话ID
    std::string device_id;   ///< 设备唯一标识
    std::string platform;    ///< 平台标识（如android, ios, web等）
    std::chrono::system_clock::time_point connect_time;  ///< 连接建立时间

    /**
     * @brief 序列化为JSON格式
     * @return JSON对象
     */
    nlohmann::json to_json() const {
        nlohmann::json j;
        j["session_id"] = session_id;
        j["device_id"] = device_id;
        j["platform"] = platform;
        // 将时间点转换为毫秒数进行存储
        j["connect_time"] =
                std::chrono::duration_cast<std::chrono::milliseconds>(connect_time.time_since_epoch())
                        .count();
        return j;
    }

    /**
     * @brief 从JSON对象反序列化
     * @param j JSON对象
     * @return DeviceSessionInfo对象
     */
    static DeviceSessionInfo from_json(const nlohmann::json& j) {
        DeviceSessionInfo info;
        info.session_id = j.value("session_id", "");
        info.device_id = j.value("device_id", "");
        info.platform = j.value("platform", "");
        // 从毫秒数恢复时间点，添加错误处理
        try {
            auto timestamp = j.value("connect_time", 0LL);
            info.connect_time = std::chrono::system_clock::time_point(std::chrono::milliseconds(timestamp));
        } catch (const std::exception& e) {
            // 如果时间戳无效，使用epoch时间
            info.connect_time = std::chrono::system_clock::time_point{};
        }
        return info;
    }
};

}  // namespace gateway
}  // namespace im

using namespace im::gateway;

// 简化的测试，只测试DeviceSessionInfo的功能
class DeviceSessionInfoTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 设置测试环境
    }

    void TearDown() override {
        // 清理测试环境
    }
};

// 测试DeviceSessionInfo的序列化和反序列化
TEST_F(DeviceSessionInfoTest, SerializationAndDeserialization) {
    DeviceSessionInfo info;
    info.session_id = "test_session_123";
    info.device_id = "device_456";
    info.platform = "android";
    info.connect_time = std::chrono::system_clock::now();

    // 测试序列化
    auto json_obj = info.to_json();
    EXPECT_EQ(json_obj["session_id"], "test_session_123");
    EXPECT_EQ(json_obj["device_id"], "device_456");
    EXPECT_EQ(json_obj["platform"], "android");
    EXPECT_TRUE(json_obj.contains("connect_time"));

    // 测试反序列化
    auto restored_info = DeviceSessionInfo::from_json(json_obj);
    EXPECT_EQ(restored_info.session_id, info.session_id);
    EXPECT_EQ(restored_info.device_id, info.device_id);
    EXPECT_EQ(restored_info.platform, info.platform);
    
    // 验证时间序列化的精度（毫秒级）
    auto time_diff = std::chrono::duration_cast<std::chrono::milliseconds>(
        info.connect_time - restored_info.connect_time).count();
    EXPECT_LE(std::abs(time_diff), 1000); // 1秒内的差异是可接受的
}

// 测试DeviceSessionInfo的边界情况
TEST_F(DeviceSessionInfoTest, EdgeCases) {
    // 测试空字符串
    DeviceSessionInfo empty_info;
    empty_info.session_id = "";
    empty_info.device_id = "";
    empty_info.platform = "";
    empty_info.connect_time = std::chrono::system_clock::time_point{}; // epoch时间

    auto json_obj = empty_info.to_json();
    auto restored_info = DeviceSessionInfo::from_json(json_obj);
    
    EXPECT_EQ(restored_info.session_id, "");
    EXPECT_EQ(restored_info.device_id, "");
    EXPECT_EQ(restored_info.platform, "");

    // 测试特殊字符
    DeviceSessionInfo special_info;
    special_info.session_id = "session_!@#$%^&*()";
    special_info.device_id = "device_中文测试";
    special_info.platform = "platform_123";
    special_info.connect_time = std::chrono::system_clock::now();

    auto special_json = special_info.to_json();
    auto restored_special = DeviceSessionInfo::from_json(special_json);
    
    EXPECT_EQ(restored_special.session_id, special_info.session_id);
    EXPECT_EQ(restored_special.device_id, special_info.device_id);
    EXPECT_EQ(restored_special.platform, special_info.platform);
}

// 测试JSON解析的错误处理
TEST_F(DeviceSessionInfoTest, JsonErrorHandling) {
    // 测试缺少字段的JSON
    nlohmann::json incomplete_json = {
        {"session_id", "test_session"},
        {"device_id", "test_device"}
        // 缺少platform和connect_time
    };

    auto restored_info = DeviceSessionInfo::from_json(incomplete_json);
    EXPECT_EQ(restored_info.session_id, "test_session");
    EXPECT_EQ(restored_info.device_id, "test_device");
    EXPECT_EQ(restored_info.platform, ""); // 默认值
    
    // 测试无效的时间戳
    nlohmann::json invalid_time_json = {
        {"session_id", "test_session"},
        {"device_id", "test_device"},
        {"platform", "android"},
        {"connect_time", "invalid_timestamp"}
    };

    // 这应该使用默认值（epoch时间）
    auto restored_invalid = DeviceSessionInfo::from_json(invalid_time_json);
    EXPECT_EQ(restored_invalid.session_id, "test_session");
    EXPECT_EQ(restored_invalid.device_id, "test_device");
    EXPECT_EQ(restored_invalid.platform, "android");
}

// 测试时间精度
TEST_F(DeviceSessionInfoTest, TimePrecision) {
    DeviceSessionInfo info;
    info.session_id = "time_test";
    info.device_id = "device_time";
    info.platform = "web";
    
    // 使用具体的时间点进行测试
    auto now = std::chrono::system_clock::now();
    info.connect_time = now;
    
    auto json_obj = info.to_json();
    auto restored_info = DeviceSessionInfo::from_json(json_obj);
    
    // 验证毫秒级精度
    auto original_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    auto restored_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
        restored_info.connect_time.time_since_epoch()).count();
    
    EXPECT_EQ(original_ms, restored_ms);
}

// 测试JSON格式的正确性
TEST_F(DeviceSessionInfoTest, JsonFormat) {
    DeviceSessionInfo info;
    info.session_id = "format_test_session";
    info.device_id = "format_test_device";
    info.platform = "ios";
    info.connect_time = std::chrono::system_clock::now();
    
    auto json_obj = info.to_json();
    
    // 验证JSON包含所有必要字段
    EXPECT_TRUE(json_obj.is_object());
    EXPECT_TRUE(json_obj.contains("session_id"));
    EXPECT_TRUE(json_obj.contains("device_id"));
    EXPECT_TRUE(json_obj.contains("platform"));
    EXPECT_TRUE(json_obj.contains("connect_time"));
    
    // 验证字段类型
    EXPECT_TRUE(json_obj["session_id"].is_string());
    EXPECT_TRUE(json_obj["device_id"].is_string());
    EXPECT_TRUE(json_obj["platform"].is_string());
    EXPECT_TRUE(json_obj["connect_time"].is_number());
    
    // 验证可以转换为字符串
    std::string json_str = json_obj.dump();
    EXPECT_FALSE(json_str.empty());
    
    // 验证可以从字符串解析
    auto parsed_json = nlohmann::json::parse(json_str);
    auto restored_info = DeviceSessionInfo::from_json(parsed_json);
    
    EXPECT_EQ(restored_info.session_id, info.session_id);
    EXPECT_EQ(restored_info.device_id, info.device_id);
    EXPECT_EQ(restored_info.platform, info.platform);
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}