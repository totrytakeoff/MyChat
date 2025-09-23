/**
 * @file test_cli_parser.cpp
 * @brief CLIParser 类的单元测试
 * @details 测试命令行参数解析器的各种功能，包括选项定义、参数解析、帮助生成等
 */

#include <gtest/gtest.h>
#include "cli_parser.hpp"
#include <sstream>
#include <vector>
#include <string>

using namespace im::utils;

class CLIParserTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 重置全局状态
        parser_ = std::make_unique<CLIParser>("test_program", "1.0.0", "Test program for CLI parsing");
        
        // 重置测试变量
        callback_called_ = false;
        callback_value_.clear();
        callback_count_ = 0;
    }
    
    void TearDown() override {
        parser_.reset();
    }
    
    // 辅助函数：将字符串向量转换为 char* 数组
    std::vector<char*> stringVectorToCharArray(const std::vector<std::string>& strings) {
        std::vector<char*> result;
        for (const auto& str : strings) {
            result.push_back(const_cast<char*>(str.c_str()));
        }
        return result;
    }
    
    // 测试回调函数
    void TestCallback(const std::string& value) {
        callback_called_ = true;
        callback_value_ = value;
        callback_count_++;
    }
    
    std::unique_ptr<CLIParser> parser_;
    bool callback_called_ = false;
    std::string callback_value_;
    int callback_count_ = 0;
};

// 测试基本构造函数
TEST_F(CLIParserTest, BasicConstruction) {
    EXPECT_EQ(parser_->getProgramName(), "test_program");
    EXPECT_EQ(parser_->getVersion(), "1.0.0");
    EXPECT_EQ(parser_->getDescription(), "Test program for CLI parsing");
}

// 测试字符串选项添加
TEST_F(CLIParserTest, AddStringOption) {
    parser_->addStringOption("host", "h", "Server host", "localhost");
    
    std::vector<std::string> args = {"test_program", "--host", "127.0.0.1"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.getString("host"), "127.0.0.1");
}

// 测试短选项形式
TEST_F(CLIParserTest, ShortOptionForm) {
    parser_->addStringOption("host", "h", "Server host", "localhost");
    
    std::vector<std::string> args = {"test_program", "-h", "192.168.1.1"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.getString("host"), "192.168.1.1");
}

// 测试整数选项
TEST_F(CLIParserTest, IntegerOption) {
    parser_->addIntOption("port", "p", "Server port", 8080);
    
    std::vector<std::string> args = {"test_program", "--port", "9000"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.getInt("port"), 9000);
}

// 测试布尔选项
TEST_F(CLIParserTest, BooleanOption) {
    parser_->addBoolOption("verbose", "v", "Enable verbose mode");
    parser_->addBoolOption("debug", "d", "Enable debug mode");
    
    std::vector<std::string> args = {"test_program", "--verbose", "-d"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.getBool("verbose"));
    EXPECT_TRUE(result.getBool("debug"));
}

// 测试浮点数选项
TEST_F(CLIParserTest, FloatOption) {
    parser_->addFloatOption("timeout", "t", "Connection timeout", 30.0f);
    
    std::vector<std::string> args = {"test_program", "--timeout", "45.5"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_FLOAT_EQ(result.getFloat("timeout"), 45.5f);
}

// 测试默认值
TEST_F(CLIParserTest, DefaultValues) {
    parser_->addStringOption("host", "h", "Server host", "localhost");
    parser_->addIntOption("port", "p", "Server port", 8080);
    parser_->addBoolOption("verbose", "v", "Enable verbose mode");
    
    std::vector<std::string> args = {"test_program"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.getString("host"), "localhost");
    EXPECT_EQ(result.getInt("port"), 8080);
    EXPECT_FALSE(result.getBool("verbose"));
}

// 测试帮助选项
TEST_F(CLIParserTest, HelpOption) {
    parser_->addStringOption("host", "h", "Server host", "localhost");
    parser_->addIntOption("port", "p", "Server port", 8080);
    
    std::vector<std::string> args = {"test_program", "--help"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.getBool("help"));
}

// 测试版本选项
TEST_F(CLIParserTest, VersionOption) {
    std::vector<std::string> args = {"test_program", "--version"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(result.getBool("version"));
}

// 测试回调函数
TEST_F(CLIParserTest, CallbackFunction) {
    parser_->addStringOption("config", "c", "Configuration file", "", 
        [this](const std::string& value) {
            TestCallback(value);
        });
    
    std::vector<std::string> args = {"test_program", "--config", "config.json"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(callback_called_);
    EXPECT_EQ(callback_value_, "config.json");
    EXPECT_EQ(result.getString("config"), "config.json");
}

// 测试无效参数
TEST_F(CLIParserTest, InvalidArguments) {
    parser_->addIntOption("port", "p", "Server port", 8080);
    
    std::vector<std::string> args = {"test_program", "--port", "invalid"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// 测试未知选项
TEST_F(CLIParserTest, UnknownOption) {
    std::vector<std::string> args = {"test_program", "--unknown-option", "value"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// 测试缺失参数值
TEST_F(CLIParserTest, MissingArgumentValue) {
    parser_->addStringOption("host", "h", "Server host", "localhost");
    
    std::vector<std::string> args = {"test_program", "--host"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// 测试重复选项定义
TEST_F(CLIParserTest, DuplicateOptionDefinition) {
    parser_->addStringOption("host", "h", "Server host", "localhost");
    
    // 尝试添加重复的选项应该失败
    EXPECT_THROW(parser_->addStringOption("host", "h", "Another host", "127.0.0.1"), 
                 std::invalid_argument);
}

// 测试帮助信息生成
TEST_F(CLIParserTest, HelpGeneration) {
    parser_->addStringOption("host", "h", "Server host address", "localhost");
    parser_->addIntOption("port", "p", "Server port number", 8080);
    parser_->addBoolOption("verbose", "v", "Enable verbose logging");
    parser_->addFloatOption("timeout", "t", "Connection timeout in seconds", 30.0f);
    
    std::string help = parser_->generateHelp();
    
    // 验证帮助信息包含必要内容
    EXPECT_TRUE(help.find("test_program") != std::string::npos);
    EXPECT_TRUE(help.find("Test program for CLI parsing") != std::string::npos);
    EXPECT_TRUE(help.find("--host") != std::string::npos);
    EXPECT_TRUE(help.find("-h") != std::string::npos);
    EXPECT_TRUE(help.find("Server host address") != std::string::npos);
    EXPECT_TRUE(help.find("--port") != std::string::npos);
    EXPECT_TRUE(help.find("--verbose") != std::string::npos);
    EXPECT_TRUE(help.find("--help") != std::string::npos);
    EXPECT_TRUE(help.find("--version") != std::string::npos);
}

// 测试版本信息生成
TEST_F(CLIParserTest, VersionGeneration) {
    std::string version = parser_->generateVersion();
    
    EXPECT_TRUE(version.find("test_program") != std::string::npos);
    EXPECT_TRUE(version.find("1.0.0") != std::string::npos);
}

// 测试位置参数
TEST_F(CLIParserTest, PositionalArguments) {
    parser_->addStringOption("config", "c", "Config file", "");
    
    std::vector<std::string> args = {"test_program", "--config", "config.json", "file1.txt", "file2.txt"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    
    auto positional = result.getPositionalArgs();
    EXPECT_EQ(positional.size(), 2);
    EXPECT_EQ(positional[0], "file1.txt");
    EXPECT_EQ(positional[1], "file2.txt");
}

// 测试复杂场景
TEST_F(CLIParserTest, ComplexScenario) {
    int callback_count = 0;
    
    parser_->addStringOption("host", "h", "Server host", "localhost", 
        [&callback_count](const std::string& value) {
            callback_count++;
        });
    parser_->addIntOption("port", "p", "Server port", 8080);
    parser_->addBoolOption("daemon", "d", "Run as daemon");
    parser_->addBoolOption("verbose", "v", "Verbose output");
    parser_->addFloatOption("timeout", "t", "Timeout", 30.0f);
    parser_->addStringOption("log-level", "l", "Log level", "info");
    
    std::vector<std::string> args = {
        "test_program", 
        "--host", "production.server.com",
        "-p", "9000",
        "--daemon",
        "-v",
        "--timeout", "45.5",
        "--log-level", "debug",
        "input1.txt", "input2.txt"
    };
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(callback_count, 1);
    EXPECT_EQ(result.getString("host"), "production.server.com");
    EXPECT_EQ(result.getInt("port"), 9000);
    EXPECT_TRUE(result.getBool("daemon"));
    EXPECT_TRUE(result.getBool("verbose"));
    EXPECT_FLOAT_EQ(result.getFloat("timeout"), 45.5f);
    EXPECT_EQ(result.getString("log-level"), "debug");
    
    auto positional = result.getPositionalArgs();
    EXPECT_EQ(positional.size(), 2);
    EXPECT_EQ(positional[0], "input1.txt");
    EXPECT_EQ(positional[1], "input2.txt");
}

// 测试边界情况
TEST_F(CLIParserTest, EdgeCases) {
    parser_->addStringOption("empty", "e", "Empty string option", "");
    parser_->addIntOption("zero", "z", "Zero integer", 0);
    parser_->addFloatOption("negative", "n", "Negative float", -1.0f);
    
    std::vector<std::string> args = {
        "test_program",
        "--empty", "",
        "--zero", "0",
        "--negative", "-42.5"
    };
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.getString("empty"), "");
    EXPECT_EQ(result.getInt("zero"), 0);
    EXPECT_FLOAT_EQ(result.getFloat("negative"), -42.5f);
}

// 测试大量选项
TEST_F(CLIParserTest, ManyOptions) {
    const int num_options = 50;
    
    for (int i = 0; i < num_options; ++i) {
        std::string name = "option" + std::to_string(i);
        std::string short_name = std::to_string(i % 10);
        std::string desc = "Description for option " + std::to_string(i);
        parser_->addStringOption(name, short_name, desc, "default" + std::to_string(i));
    }
    
    std::vector<std::string> args = {"test_program", "--option25", "test_value"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.getString("option25"), "test_value");
    
    // 验证其他选项使用默认值
    EXPECT_EQ(result.getString("option0"), "default0");
    EXPECT_EQ(result.getString("option49"), "default49");
}

// 测试特殊字符
TEST_F(CLIParserTest, SpecialCharacters) {
    parser_->addStringOption("message", "m", "Message with special chars", "");
    
    std::vector<std::string> args = {
        "test_program", 
        "--message", "Hello, World! @#$%^&*()_+-={}[]|\\:;\"'<>?,./"
    };
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.getString("message"), "Hello, World! @#$%^&*()_+-={}[]|\\:;\"'<>?,./");
}

// 测试 Unicode 字符
TEST_F(CLIParserTest, UnicodeCharacters) {
    parser_->addStringOption("unicode", "u", "Unicode string", "");
    
    std::vector<std::string> args = {
        "test_program", 
        "--unicode", "你好世界 🌍 Здравствуй мир"
    };
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.getString("unicode"), "你好世界 🌍 Здравствуй мир");
}

// 测试空参数列表
TEST_F(CLIParserTest, EmptyArguments) {
    parser_->addStringOption("host", "h", "Server host", "localhost");
    
    std::vector<std::string> args = {"test_program"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.getString("host"), "localhost");
}

// 测试只有程序名
TEST_F(CLIParserTest, ProgramNameOnly) {
    std::vector<std::string> args = {"test_program"};
    auto char_args = stringVectorToCharArray(args);
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_FALSE(result.getBool("help"));
    EXPECT_FALSE(result.getBool("version"));
}

// 性能测试
TEST_F(CLIParserTest, PerformanceTest) {
    // 添加大量选项
    for (int i = 0; i < 100; ++i) {
        std::string name = "opt" + std::to_string(i);
        std::string short_name = std::to_string(i % 10);
        parser_->addStringOption(name, short_name, "Option " + std::to_string(i), "default");
    }
    
    std::vector<std::string> args = {"test_program"};
    for (int i = 0; i < 50; ++i) {
        args.push_back("--opt" + std::to_string(i));
        args.push_back("value" + std::to_string(i));
    }
    
    auto char_args = stringVectorToCharArray(args);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    EXPECT_TRUE(result.success);
    EXPECT_LT(duration.count(), 10000);  // 应该在10ms内完成
    
    std::cout << "Parsed " << args.size() << " arguments in " << duration.count() << " microseconds" << std::endl;
}

// 集成测试：模拟真实应用场景
TEST_F(CLIParserTest, RealWorldIntegration) {
    bool config_loaded = false;
    bool verbose_enabled = false;
    
    // 模拟一个典型的服务器应用程序
    parser_->addStringOption("config", "c", "Configuration file path", "config.json",
        [&config_loaded](const std::string& value) {
            config_loaded = true;
            std::cout << "Loading config from: " << value << std::endl;
        });
    
    parser_->addStringOption("host", "h", "Server host address", "0.0.0.0");
    parser_->addIntOption("port", "p", "Server port number", 8080);
    parser_->addBoolOption("daemon", "d", "Run as daemon process");
    parser_->addBoolOption("verbose", "v", "Enable verbose logging",
        [&verbose_enabled]() {
            verbose_enabled = true;
            std::cout << "Verbose logging enabled" << std::endl;
        });
    parser_->addStringOption("log-file", "l", "Log file path", "server.log");
    parser_->addIntOption("workers", "w", "Number of worker threads", 4);
    parser_->addFloatOption("timeout", "t", "Request timeout in seconds", 30.0f);
    
    std::vector<std::string> args = {
        "server_app",
        "--config", "/etc/server/production.conf",
        "--host", "192.168.1.100",
        "--port", "443",
        "--daemon",
        "--verbose",
        "--log-file", "/var/log/server.log",
        "--workers", "8",
        "--timeout", "60.0",
        "input1.txt", "input2.txt"
    };
    
    auto char_args = stringVectorToCharArray(args);
    auto result = parser_->parse(static_cast<int>(char_args.size()), char_args.data());
    
    // 验证解析结果
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(config_loaded);
    EXPECT_TRUE(verbose_enabled);
    
    EXPECT_EQ(result.getString("config"), "/etc/server/production.conf");
    EXPECT_EQ(result.getString("host"), "192.168.1.100");
    EXPECT_EQ(result.getInt("port"), 443);
    EXPECT_TRUE(result.getBool("daemon"));
    EXPECT_TRUE(result.getBool("verbose"));
    EXPECT_EQ(result.getString("log-file"), "/var/log/server.log");
    EXPECT_EQ(result.getInt("workers"), 8);
    EXPECT_FLOAT_EQ(result.getFloat("timeout"), 60.0f);
    
    auto positional = result.getPositionalArgs();
    EXPECT_EQ(positional.size(), 2);
    EXPECT_EQ(positional[0], "input1.txt");
    EXPECT_EQ(positional[1], "input2.txt");
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}