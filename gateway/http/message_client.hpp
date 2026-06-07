#ifndef GATEWAY_HTTP_MESSAGE_CLIENT_HPP
#define GATEWAY_HTTP_MESSAGE_CLIENT_HPP

#include <memory>
#include <string>
#include <vector>

#include "../../services/message/message_service.hpp"

namespace im::gateway {

class MessageClient {
public:
    virtual ~MessageClient() = default;

    virtual im::service::message::SendResult send_text_message(
        const im::service::message::SendRequest& request) = 0;

    virtual std::vector<im::service::message::MessageData> get_conversation(
        const std::string& user_a,
        const std::string& user_b,
        int64_t before_time,
        int limit) = 0;

    virtual std::vector<im::service::message::MessageData> pull_offline(
        const std::string& receiver_uid,
        int64_t before_time,
        int limit) = 0;

    virtual bool mark_delivered(uint64_t msg_id, int64_t delivered_time) = 0;
    virtual bool mark_read(uint64_t msg_id, int64_t read_time) = 0;
};

class LocalMessageClient final : public MessageClient {
public:
    explicit LocalMessageClient(
        std::shared_ptr<im::service::message::MessageService> message_service);

    im::service::message::SendResult send_text_message(
        const im::service::message::SendRequest& request) override;

    std::vector<im::service::message::MessageData> get_conversation(
        const std::string& user_a,
        const std::string& user_b,
        int64_t before_time,
        int limit) override;

    std::vector<im::service::message::MessageData> pull_offline(
        const std::string& receiver_uid,
        int64_t before_time,
        int limit) override;

    bool mark_delivered(uint64_t msg_id, int64_t delivered_time) override;
    bool mark_read(uint64_t msg_id, int64_t read_time) override;

private:
    std::shared_ptr<im::service::message::MessageService> message_service_;
};

}  // namespace im::gateway

#endif  // GATEWAY_HTTP_MESSAGE_CLIENT_HPP
