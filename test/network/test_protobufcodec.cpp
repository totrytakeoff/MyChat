#include <gtest/gtest.h>
#include "protobuf_codec.hpp"
#include "../../common/proto/base.pb.h"  // IMHeader定义
#include "test_message.pb.h" // 测试专用消息
#include "../../common/utils/log_manager.hpp"  // IMHeader定义
using namespace im::base;

using namespace im::utils;
using namespace im::network;


class ProtobufCodecTest : public ::testing::Test {
protected:
    void SetUp() override {
        // 初始化测试消息
        header_.set_version("1.0.0");
        header_.set_seq(12345);
        header_.set_cmd_id(1001);
        header_.set_from_uid("user_001");
        header_.set_to_uid("user_002");
        header_.set_timestamp(1719691200000);
        header_.set_token("test_token");
        header_.set_device_id("android_123");
        
        message_.set_id(42);
        message_.set_name("Test Message");
        message_.set_value(3.14159);
        message_.add_tags("tag1");
        message_.add_tags("tag2");
        message_.set_is_valid(true);
        
        LogManager::SetLogToFile("protobuf_codec", "logs/protobuf_codec_test.log");
        LogManager::SetLoggingEnabled("protobuf_codec", true);
    }
    
    void TearDown() override {
        // 清理资源
    }
    
    IMHeader header_;
    test::TestMessage message_;
};

// 测试空消息编码
TEST_F(ProtobufCodecTest, EncodeEmptyMessage) {
    test::TestMessage empty_message;
    std::string output;
    
    // 编码空消息
    EXPECT_TRUE(ProtobufCodec::encode(header_, empty_message, output));
    EXPECT_FALSE(output.empty());
    
    // 解码验证
    IMHeader decoded_header;
    test::TestMessage decoded_message;
    EXPECT_TRUE(ProtobufCodec::decode(output, decoded_header, decoded_message));
    
    // 验证头部
    EXPECT_EQ(header_.version(), decoded_header.version());
    EXPECT_EQ(header_.seq(), decoded_header.seq());
    EXPECT_EQ(header_.cmd_id(), decoded_header.cmd_id());
    
    // 验证消息体
    EXPECT_EQ(empty_message.id(), decoded_message.id());
    EXPECT_EQ(empty_message.name(), decoded_message.name());
    EXPECT_EQ(empty_message.value(), decoded_message.value());
    EXPECT_EQ(empty_message.tags_size(), decoded_message.tags_size());
    EXPECT_EQ(empty_message.is_valid(), decoded_message.is_valid());
}

// 测试完整消息编码
TEST_F(ProtobufCodecTest, EncodeDecodeFullMessage) {
    std::string output;
    
    // 编码
    EXPECT_TRUE(ProtobufCodec::encode(header_, message_, output));
    EXPECT_FALSE(output.empty());
    
    // 解码
    IMHeader decoded_header;
    test::TestMessage decoded_message;
    EXPECT_TRUE(ProtobufCodec::decode(output, decoded_header, decoded_message));
    
    // 验证头部
    EXPECT_EQ(header_.version(), decoded_header.version());
    EXPECT_EQ(header_.seq(), decoded_header.seq());
    EXPECT_EQ(header_.cmd_id(), decoded_header.cmd_id());
    EXPECT_EQ(header_.from_uid(), decoded_header.from_uid());
    EXPECT_EQ(header_.to_uid(), decoded_header.to_uid());
    EXPECT_EQ(header_.timestamp(), decoded_header.timestamp());
    EXPECT_EQ(header_.token(), decoded_header.token());
    EXPECT_EQ(header_.device_id(), decoded_header.device_id());
    
    // 验证消息体
    EXPECT_EQ(message_.id(), decoded_message.id());
    EXPECT_EQ(message_.name(), decoded_message.name());
    EXPECT_DOUBLE_EQ(message_.value(), decoded_message.value());
    ASSERT_EQ(message_.tags_size(), decoded_message.tags_size());
    EXPECT_EQ(message_.tags(0), decoded_message.tags(0));
    EXPECT_EQ(message_.tags(1), decoded_message.tags(1));
    EXPECT_EQ(message_.is_valid(), decoded_message.is_valid());
}

// 测试大消息编码
TEST_F(ProtobufCodecTest, EncodeLargeMessage) {
    // 创建大消息体
    test::TestMessage large_message;
    large_message.set_id(1000);
    large_message.set_name("Large Test Message");
    
    // 添加大量标签
    for (int i = 0; i < 1000; ++i) {
        large_message.add_tags("tag_" + std::to_string(i));
    }
    
    // 添加大量数据
    std::string big_data(1024 * 1024, 'x'); // 1MB 数据
    large_message.set_data(big_data);
    
    std::string output;
    
    // 编码
    EXPECT_TRUE(ProtobufCodec::encode(header_, large_message, output));
    EXPECT_GT(output.size(), 1024 * 1024); // 应大于1MB
    
    // 解码
    IMHeader decoded_header;
    test::TestMessage decoded_message;
    EXPECT_TRUE(ProtobufCodec::decode(output, decoded_header, decoded_message));
    
    // 验证消息体
    EXPECT_EQ(large_message.id(), decoded_message.id());
    EXPECT_EQ(large_message.name(), decoded_message.name());
    ASSERT_EQ(large_message.tags_size(), decoded_message.tags_size());
    EXPECT_EQ(large_message.tags(0), decoded_message.tags(0));
    EXPECT_EQ(large_message.tags(999), decoded_message.tags(999));
    EXPECT_EQ(large_message.data(), decoded_message.data());
}

// 测试无效输入解码
TEST_F(ProtobufCodecTest, DecodeInvalidInput) {
    // 空输入
    {
        IMHeader header;
        test::TestMessage message;
        EXPECT_FALSE(ProtobufCodec::decode("", header, message));
    }
    
    // 无效长度头
    {
        // 创建无效的varint长度头
        std::string invalid_data = "\xFF\xFF\xFF\xFF\xFF"; // 无效的varint
        IMHeader header;
        test::TestMessage message;
        EXPECT_FALSE(ProtobufCodec::decode(invalid_data, header, message));
    }
    
    // 长度头超出实际数据
    {
        std::string valid_output;
        ProtobufCodec::encode(header_, message_, valid_output);
        
        // 截断数据
        std::string truncated = valid_output.substr(0, valid_output.size() / 2);
        IMHeader header;
        test::TestMessage message;
        EXPECT_FALSE(ProtobufCodec::decode(truncated, header, message));
    }
    
    // 无效消息头
    {
        std::string valid_output;
        ProtobufCodec::encode(header_, message_, valid_output);
        
        // 篡改消息头数据
        std::string corrupted = valid_output;
        corrupted[10] = '\xFF'; // 随机篡改位置
        
        IMHeader header;
        test::TestMessage message;
        EXPECT_FALSE(ProtobufCodec::decode(corrupted, header, message));
    }
}

// 测试消息类型不匹配
TEST_F(ProtobufCodecTest, DecodeWrongMessageType) {
    std::string output;
    EXPECT_TRUE(ProtobufCodec::encode(header_, message_, output));
    
    // 尝试解码为错误的消息类型
    IMHeader decoded_header;
    test::AnotherTestMessage wrong_message;
    EXPECT_FALSE(ProtobufCodec::decode(output, decoded_header, wrong_message));
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}