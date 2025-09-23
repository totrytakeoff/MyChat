/**
 * @file test_cli_parser_simple.cpp
 * @brief CLIParser 类的简化单元测试
 * @details 基于实际接口测试命令行参数解析器的基本功能
 */

#include <gtest/gtest.h>
#include "cli_parser.hpp"
#include <sstream>
#include <vector>
#include <string>

using namespace im::utils;

class CLIParserSimpleTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 重置测试变量
        callback_called_ = false;
        callback_value_.clear();
        callback_count_ = 0;
    }
    
    void TearDown() override {
        // 清理
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
    bool TestCallback(const std::string& value) {
        callback_called_ = true;
        callback_value_ = value;
        callback_count_++;
        return true;
    }
    
    bool callback_called_ = false;
    std::string callback_value_;
    int callback_count_ = 0;
};

// 测试基本构造函数
TEST_F(CLIParserSimpleTest, BasicConstruction) {
    CLIParser parser("test_program", "Test program for CLI parsing");
    
    // 构造函数应该成功，没有异常
    EXPECT_NO_THROW({
        CLIParser another_parser("another_program");
    });
}

// 测试添加基本参数
TEST_F(CLIParserSimpleTest, AddBasicArguments) {
    CLIParser parser("test_program", "Test program");
    
    // 添加字符串参数 (不能使用'h'因为已被help占用)
    bool success = parser.addArgument("host", 'H', ArgumentType::STRING, false, 
                                     "Server host", "localhost");
    EXPECT_TRUE(success);
    
    // 添加整数参数
    success = parser.addArgument("port", 'p', ArgumentType::INTEGER, false, 
                                "Server port", "8080");
    EXPECT_TRUE(success);
    
    // 添加标志参数
    success = parser.addArgument("verbose", 'v', ArgumentType::FLAG, false, 
                                "Enable verbose mode");
    EXPECT_TRUE(success);
}

// 测试重复参数检查
TEST_F(CLIParserSimpleTest, DuplicateArgumentCheck) {
    CLIParser parser("test_program", "Test program");
    
    // 添加参数 (不能使用'h'因为已被help占用)
    bool success = parser.addArgument("host", 'H', ArgumentType::STRING, false, 
                                     "Server host", "localhost");
    EXPECT_TRUE(success);
    
    // 尝试添加重复的长选项名
    success = parser.addArgument("host", 'o', ArgumentType::STRING, false, 
                                "Another host", "127.0.0.1");
    EXPECT_FALSE(success);
    
    // 尝试添加重复的短选项名
    success = parser.addArgument("hostname", 'H', ArgumentType::STRING, false, 
                                "Hostname", "localhost");
    EXPECT_FALSE(success);
}

// 测试版本参数添加
TEST_F(CLIParserSimpleTest, AddVersionArgument) {
    CLIParser parser("test_program", "Test program");
    
    // 添加版本参数
    EXPECT_NO_THROW({
        parser.addVersionArgument("1.0.0");
    });
}

// 测试回调函数
TEST_F(CLIParserSimpleTest, CallbackFunction) {
    CLIParser parser("test_program", "Test program");
    
    // 添加带回调的参数
    bool success = parser.addArgument("config", 'c', ArgumentType::STRING, false, 
                                     "Configuration file", "",
                                     "General",
                                     [this](const std::string& value) -> bool {
                                         return TestCallback(value);
                                     });
    EXPECT_TRUE(success);
}

// 测试基本解析功能
TEST_F(CLIParserSimpleTest, BasicParsing) {
    CLIParser parser("test_program", "Test program");
    
    // 添加一些参数 (避免与默认的help冲突)
    parser.addArgument("host", 'H', ArgumentType::STRING, false, "Server host", "localhost");
    parser.addArgument("port", 'p', ArgumentType::INTEGER, false, "Server port", "8080");
    parser.addArgument("verbose", 'V', ArgumentType::FLAG, false, "Enable verbose mode");
    
    // 测试空参数列表
    std::vector<std::string> args = {"test_program"};
    auto char_args = stringVectorToCharArray(args);
    
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    // 解析应该成功
    EXPECT_TRUE(result.success);
    if (!result.success) {
        std::cout << "Parse error: " << result.error_message << std::endl;
    }
}

// 测试帮助参数
TEST_F(CLIParserSimpleTest, HelpArgument) {
    CLIParser parser("test_program", "Test program");
    
    // 添加一些参数 (避免与help的'h'冲突)
    parser.addArgument("host", 'H', ArgumentType::STRING, false, "Server host", "localhost");
    
    // 测试长帮助选项
    std::vector<std::string> args = {"test_program", "--help"};
    auto char_args = stringVectorToCharArray(args);
    
    // 重定向输出以捕获帮助信息
    testing::internal::CaptureStdout();
    
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    std::string output = testing::internal::GetCapturedStdout();
    
    // 帮助应该被显示
    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(output.find("test_program") != std::string::npos);
}

// 测试版本参数
TEST_F(CLIParserSimpleTest, VersionArgument) {
    CLIParser parser("test_program", "Test program");
    parser.addVersionArgument("1.0.0");
    
    // 测试版本选项
    std::vector<std::string> args = {"test_program", "--version"};
    auto char_args = stringVectorToCharArray(args);
    
    testing::internal::CaptureStdout();
    
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    std::string output = testing::internal::GetCapturedStdout();
    
    // 版本信息应该被显示
    EXPECT_FALSE(output.empty());
    EXPECT_TRUE(output.find("1.0.0") != std::string::npos);
}

// 测试参数类型
TEST_F(CLIParserSimpleTest, ArgumentTypes) {
    CLIParser parser("test_program", "Test program");
    
    // 添加各种类型的参数
    EXPECT_TRUE(parser.addArgument("string_arg", 's', ArgumentType::STRING, false, 
                                  "String argument", "default"));
    EXPECT_TRUE(parser.addArgument("int_arg", 'i', ArgumentType::INTEGER, false, 
                                  "Integer argument", "42"));
    EXPECT_TRUE(parser.addArgument("float_arg", 'f', ArgumentType::FLOAT, false, 
                                  "Float argument", "3.14"));
    EXPECT_TRUE(parser.addArgument("bool_arg", 'b', ArgumentType::BOOLEAN, false, 
                                  "Boolean argument", "false"));
    EXPECT_TRUE(parser.addArgument("flag_arg", 'F', ArgumentType::FLAG, false, 
                                  "Flag argument"));
}

// 测试无效参数处理
TEST_F(CLIParserSimpleTest, InvalidArguments) {
    CLIParser parser("test_program", "Test program");
    
    parser.addArgument("port", 'p', ArgumentType::INTEGER, false, "Server port", "8080");
    
    // 测试未知选项
    std::vector<std::string> args = {"test_program", "--unknown"};
    auto char_args = stringVectorToCharArray(args);
    
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    // 解析应该失败
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// 测试必需参数
TEST_F(CLIParserSimpleTest, RequiredArguments) {
    CLIParser parser("test_program", "Test program");
    
    // 添加必需参数
    parser.addArgument("config", 'c', ArgumentType::STRING, true, "Configuration file");
    
    // 测试缺少必需参数
    std::vector<std::string> args = {"test_program"};
    auto char_args = stringVectorToCharArray(args);
    
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    // 解析应该失败
    EXPECT_FALSE(result.success);
    EXPECT_FALSE(result.error_message.empty());
}

// 测试剩余参数
TEST_F(CLIParserSimpleTest, RemainingArguments) {
    CLIParser parser("test_program", "Test program");
    
    parser.addArgument("verbose", 'v', ArgumentType::FLAG, false, "Verbose mode");
    
    // 测试带剩余参数的解析
    std::vector<std::string> args = {"test_program", "--verbose", "file1.txt", "file2.txt"};
    auto char_args = stringVectorToCharArray(args);
    
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_EQ(result.remaining_args.size(), 2);
    EXPECT_EQ(result.remaining_args[0], "file1.txt");
    EXPECT_EQ(result.remaining_args[1], "file2.txt");
}

// 测试参数分组
TEST_F(CLIParserSimpleTest, ArgumentGroups) {
    CLIParser parser("test_program", "Test program");
    
    // 添加不同分组的参数
    parser.addArgument("host", 'h', ArgumentType::STRING, false, 
                      "Server host", "localhost", "Network");
    parser.addArgument("port", 'p', ArgumentType::INTEGER, false, 
                      "Server port", "8080", "Network");
    parser.addArgument("verbose", 'v', ArgumentType::FLAG, false, 
                      "Verbose mode", "", "Debug");
    parser.addArgument("log-file", 'l', ArgumentType::STRING, false, 
                      "Log file", "app.log", "Debug");
    
    // 参数应该被成功添加
    // 实际的分组功能会在帮助信息中体现
}

// 测试边界情况
TEST_F(CLIParserSimpleTest, EdgeCases) {
    CLIParser parser("test_program", "Test program");
    
    // 测试空长选项名
    EXPECT_FALSE(parser.addArgument("", 'e', ArgumentType::STRING, false, "Empty name"));
    
    // 测试无短选项名
    EXPECT_TRUE(parser.addArgument("no-short", 0, ArgumentType::STRING, false, "No short option"));
    
    // 测试特殊字符在选项名中
    EXPECT_TRUE(parser.addArgument("special-option", 0, ArgumentType::STRING, false, "Special option"));
}

// 测试回调执行
TEST_F(CLIParserSimpleTest, CallbackExecution) {
    CLIParser parser("test_program", "Test program");
    
    bool callback_executed = false;
    std::string callback_value;
    
    // 添加带回调的参数
    parser.addArgument("test", 't', ArgumentType::STRING, false, 
                      "Test argument", "",
                      "General",
                      [&callback_executed, &callback_value](const std::string& value) -> bool {
                          callback_executed = true;
                          callback_value = value;
                          return true;
                      });
    
    // 测试回调执行
    std::vector<std::string> args = {"test_program", "--test", "test_value"};
    auto char_args = stringVectorToCharArray(args);
    
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(callback_executed);
    EXPECT_EQ(callback_value, "test_value");
}

// 测试回调失败处理
TEST_F(CLIParserSimpleTest, CallbackFailure) {
    CLIParser parser("test_program", "Test program");
    
    // 添加会失败的回调
    parser.addArgument("fail", 'f', ArgumentType::STRING, false, 
                      "Failing argument", "",
                      "General",
                      [](const std::string&) -> bool {
                          return false;  // 回调失败
                      });
    
    std::vector<std::string> args = {"test_program", "--fail", "value"};
    auto char_args = stringVectorToCharArray(args);
    
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    // 解析应该失败，因为回调返回了false
    EXPECT_FALSE(result.success);
}

// 测试复杂场景
TEST_F(CLIParserSimpleTest, ComplexScenario) {
    CLIParser parser("server", "A simple server application");
    parser.addVersionArgument("2.1.0");
    
    bool config_loaded = false;
    bool verbose_enabled = false;
    
    // 添加各种参数
    parser.addArgument("config", 'c', ArgumentType::STRING, false, 
                      "Configuration file", "server.conf", "Configuration",
                      [&config_loaded](const std::string& value) -> bool {
                          config_loaded = true;
                          std::cout << "Loading config: " << value << std::endl;
                          return true;
                      });
    
    parser.addArgument("host", 'H', ArgumentType::STRING, false, 
                      "Server host", "0.0.0.0", "Network");
    parser.addArgument("port", 'p', ArgumentType::INTEGER, false, 
                      "Server port", "8080", "Network");
    parser.addArgument("workers", 'w', ArgumentType::INTEGER, false, 
                      "Worker threads", "4", "Performance");
    parser.addArgument("verbose", 'V', ArgumentType::FLAG, false, 
                      "Verbose logging", "", "Debug",
                      [&verbose_enabled](const std::string&) -> bool {
                          verbose_enabled = true;
                          return true;
                      });
    
    // 测试复杂参数组合
    std::vector<std::string> args = {
        "server", 
        "--config", "production.conf",
        "--host", "192.168.1.100",
        "--port", "443",
        "--workers", "8",
        "--verbose",
        "input1.txt", "input2.txt"
    };
    
    auto char_args = stringVectorToCharArray(args);
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    EXPECT_TRUE(result.success);
    EXPECT_TRUE(config_loaded);
    EXPECT_TRUE(verbose_enabled);
    EXPECT_EQ(result.remaining_args.size(), 2);
    EXPECT_EQ(result.remaining_args[0], "input1.txt");
    EXPECT_EQ(result.remaining_args[1], "input2.txt");
}

// 性能测试
TEST_F(CLIParserSimpleTest, PerformanceTest) {
    CLIParser parser("perf_test", "Performance test application");
    
    // 添加大量参数
    for (int i = 0; i < 100; ++i) {
        std::string name = "option" + std::to_string(i);
        char short_name = (i % 26) + 'a';  // 循环使用字母
        if (i >= 26) short_name = 0;  // 超过26个后不使用短选项
        
        parser.addArgument(name, short_name, ArgumentType::STRING, false, 
                          "Option " + std::to_string(i), "default" + std::to_string(i));
    }
    
    // 测试解析性能
    std::vector<std::string> args = {"perf_test", "--option50", "test_value"};
    auto char_args = stringVectorToCharArray(args);
    
    auto start_time = std::chrono::high_resolution_clock::now();
    
    ParseResult result = parser.parse(static_cast<int>(char_args.size()), char_args.data());
    
    auto end_time = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
    
    EXPECT_TRUE(result.success);
    EXPECT_LT(duration.count(), 10000);  // 应该在10ms内完成
    
    std::cout << "Performance test: parsed 100 options in " << duration.count() << " microseconds" << std::endl;
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}