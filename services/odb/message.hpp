#ifndef IM_SERVICE_MESSAGE_MESSAGE_HPP
#define IM_SERVICE_MESSAGE_MESSAGE_HPP

#include <odb/core.hxx>
#include <string>
#include <cstdint>

namespace im {
namespace service {
namespace message {

enum class MessageStatus {
    SENT    = 0,
    DELIVERED = 1,
    READ    = 2
};

enum class MessageType {
    TEXT  = 0,
    IMAGE = 1,
    FILE  = 2,
    SYSTEM = 3
};

#pragma db object table("im_messages")
class Message {
public:
    Message() : msg_type_(MessageType::TEXT), status_(MessageStatus::SENT),
                create_time_(0), delivered_time_(0), read_time_(0) {}

    Message(const std::string& sender_uid,
            const std::string& receiver_uid,
            const std::string& content,
            MessageType msg_type = MessageType::TEXT,
            int64_t create_time = 0)
        : sender_uid_(sender_uid), receiver_uid_(receiver_uid),
          content_(content), msg_type_(msg_type),
          status_(MessageStatus::SENT),
          create_time_(create_time), delivered_time_(0), read_time_(0) {}

    uint64_t msg_id() const { return msg_id_; }
    void msg_id(uint64_t v) { msg_id_ = v; }

    const std::string& sender_uid() const { return sender_uid_; }
    void sender_uid(const std::string& v) { sender_uid_ = v; }

    const std::string& receiver_uid() const { return receiver_uid_; }
    void receiver_uid(const std::string& v) { receiver_uid_ = v; }

    const std::string& content() const { return content_; }
    void content(const std::string& v) { content_ = v; }

    MessageType msg_type() const { return msg_type_; }
    void msg_type(const MessageType v) { msg_type_ = v; }

    MessageStatus status() const { return status_; }
    void status(const MessageStatus v) { status_ = v; }

    int64_t create_time() const { return create_time_; }
    void create_time(const int64_t v) { create_time_ = v; }

    int64_t delivered_time() const { return delivered_time_; }
    void delivered_time(const int64_t v) { delivered_time_ = v; }

    int64_t read_time() const { return read_time_; }
    void read_time(const int64_t v) { read_time_ = v; }

private:
    friend class odb::access;

    #pragma db id auto
    uint64_t msg_id_;

    #pragma db not_null
    std::string sender_uid_;

    #pragma db not_null
    std::string receiver_uid_;

    #pragma db not_null
    std::string content_;

    #pragma db value_not_null
    MessageType msg_type_;

    #pragma db value_not_null
    MessageStatus status_;

    int64_t create_time_;
    int64_t delivered_time_;
    int64_t read_time_;
};

} // namespace message
} // namespace service
} // namespace im

#endif // IM_SERVICE_MESSAGE_MESSAGE_HPP
