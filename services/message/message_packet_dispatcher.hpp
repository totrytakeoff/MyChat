#ifndef IM_SERVICE_MESSAGE_MESSAGE_PACKET_DISPATCHER_HPP
#define IM_SERVICE_MESSAGE_MESSAGE_PACKET_DISPATCHER_HPP

#include "message.pb.h"

namespace im::service::message {

class MessageService;

class MessagePacketDispatcher {
public:
    explicit MessagePacketDispatcher(MessageService* message_service);

    im::message::MessagePacketResponse handle(const im::message::MessagePacketRequest& packet);

private:
    im::message::MessagePacketResponse handle_send_message(const im::message::MessagePacketRequest& packet);
    im::message::MessagePacketResponse handle_send_message_protobuf(const im::message::MessagePacketRequest& packet);
    im::message::MessagePacketResponse handle_send_message_json(const im::message::MessagePacketRequest& packet);
    im::message::MessagePacketResponse handle_message_history(const im::message::MessagePacketRequest& packet);
    im::message::MessagePacketResponse handle_pull_offline(const im::message::MessagePacketRequest& packet);

    MessageService* message_service_;
};

}  // namespace im::service::message

#endif  // IM_SERVICE_MESSAGE_MESSAGE_PACKET_DISPATCHER_HPP
