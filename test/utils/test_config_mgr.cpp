#include <gtest/gtest.h>
#include "../../common/utils/config_mgr.hpp"
#include <fstream>

// 用于测试的临时配置文件路径
const std::string TEST_CONFIG_PATH = "test_config.json";
using namespace im::utils;

// 测试夹具类
class ConfigManagerTest : public ::testing::Test {
protected:
    // 在每个测试用例前创建临时配置文件
    void SetUp() override {
        // 创建临时配置文件
        std::ofstream file(TEST_CONFIG_PATH);
        file << "{\"test\":{\"key\":\"value\"}}";
        file.close();
    }

    // 在每个测试用例后删除临时配置文件
    void TearDown() override {
        // std::remove(TEST_CONFIG_PATH.c_str());
    }
};

// 测试配置文件加载
TEST_F(ConfigManagerTest, LoadConfigFile) {
    ConfigManager config(TEST_CONFIG_PATH);
    EXPECT_EQ(config.get("test.key"), "value");
}

// 测试获取不存在的配置项
TEST_F(ConfigManagerTest, GetNonExistentKey) {
    ConfigManager config(TEST_CONFIG_PATH);
    EXPECT_EQ(config.get("non.existent.key"), "");
}

// 测试设置和保存配置
TEST_F(ConfigManagerTest, SetAndSaveConfig) {
    ConfigManager config(TEST_CONFIG_PATH);
    
    // 设置新值
    config.set("test.new_key", "new_value");
    config.save();
    
    // 重新加载文件验证保存
    ConfigManager config2(TEST_CONFIG_PATH);
    EXPECT_EQ(config2.get("test.new_key"), "new_value");
}

// 测试多层嵌套配置
TEST_F(ConfigManagerTest, NestedConfig) {
    ConfigManager config(TEST_CONFIG_PATH);
    
    // 设置嵌套值
    config.set("database.host", "localhost");
    config.set("database.port", "5432");
    config.save();
    
    // 验证嵌套值
    EXPECT_EQ(config.get("database.host"), "localhost");
    EXPECT_EQ(config.get("database.port"), "5432");
}

// 测试现有配置文件的加载
TEST_F(ConfigManagerTest, LoadExistingConfig) {
    ConfigManager config("../../../common/config.json");
    
    // 验证数据库配置
    EXPECT_EQ(config.get("database.host"), "localhost");
    EXPECT_EQ(config.get("database.port"), "5432");
    EXPECT_EQ(config.get("database.username"), "admin");
    EXPECT_EQ(config.get("database.password"), "secret");
    
    // 验证服务器配置
    EXPECT_EQ(config.get("server.ip"), "127.0.0.1");
    EXPECT_EQ(config.get("server.port"), "8080");
}