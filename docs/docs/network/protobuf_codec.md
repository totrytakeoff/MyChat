# ProtobufCodec 文档

## 概述

ProtobufCodec 是一个用于序列化和反序列化 IM 消息的编解码器类。它能够将 IMHeader 和 Protobuf 消息编码为二进制数据，以及将二进制数据解码为 IMHeader 和 Protobuf 消息。

## 数据格式

编码后的数据格式如下：

```
[header_size(varint)][type_name_size(varint)][type_name_string][header_data][message_data][CRC32]
```

各部分说明：
- `header_size`: varint 编码的消息头大小
- `type_name_size`: varint 编码的消息类型名称大小
- `type_name_string`: 消息类型的名称字符串
- `header_data`: 序列化后的 IMHeader 数据
- `message_data`: 序列化后的消息体数据
- `CRC32`: 用于数据完整性校验的 CRC32 码

## 特性

1. **Varint 编码**: 使用 varint 编码存储头部大小和类型名称大小，节省空间
2. **类型验证**: 在消息中包含类型信息以支持类型验证
3. **数据完整性**: 使用 CRC32 校验确保数据完整性
4. **兼容性**: 支持空消息和大型消息的处理
5. **错误日志**: 提供详细的错误日志记录

## API 说明

### encode 方法

```cpp
static bool encode(const im::base::IMHeader& header, 
                  const google::protobuf::Message& message, 
                  std::string& output);
```

将 IMHeader 和 Protobuf 消息序列化为二进制数据。

**参数**:
- `header`: 消息头，包含版本、序列号、命令ID等元数据
- `message`: 消息体，具体的 Protobuf 消息
- `output`: 输出序列化后的二进制数据

**返回值**:
- `true`: 编码成功
- `false`: 编码失败

### decode 方法

```cpp
static bool decode(const std::string& input, 
                  im::base::IMHeader& header, 
                  google::protobuf::Message& message);
```

将二进制数据反序列化为 IMHeader 和 Protobuf 消息。

**参数**:
- `input`: 输入的二进制数据
- `header`: 输出解析后的消息头
- `message`: 输出解析后的消息体

**返回值**:
- `true`: 解码成功
- `false`: 解码失败

## 错误处理

ProtobufCodec 提供了全面的错误处理机制：

1. **CRC 校验**: 自动验证数据完整性
2. **类型匹配**: 验证消息类型是否匹配
3. **初始化检查**: 检查解码后的对象是否完全初始化
4. **日志记录**: 详细的错误日志便于调试

## 使用示例

```cpp
// 编码示例
im::base::IMHeader header;
// 设置 header 字段...

MyMessage message;
// 设置 message 字段...

std::string encoded_data;
if (ProtobufCodec::encode(header, message, encoded_data)) {
    // 编码成功，使用 encoded_data
} else {
    // 编码失败，处理错误
}

// 解码示例
im::base::IMHeader decoded_header;
MyMessage decoded_message;
if (ProtobufCodec::decode(encoded_data, decoded_header, decoded_message)) {
    // 解码成功，使用 decoded_header 和 decoded_message
} else {
    // 解码失败，处理错误
}
```

## 设计考虑

1. **分层设计**: ProtobufCodec 专注于应用层消息的编解码，与网络层解耦
2. **性能优化**: 使用 Google Protobuf 的 CodedInputStream/CodedOutputStream 进行高效读写
3. **安全性**: 通过 CRC32 校验和类型验证增强数据安全性
4. **兼容性**: 保持与 TCPSession 消息头设计的兼容性