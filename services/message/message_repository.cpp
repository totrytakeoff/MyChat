#include "message_repository.hpp"

#include <utility>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>
#include <odb/query.hxx>

#include <message.hpp>
#include <message-odb.hxx>

namespace im {
namespace service {
namespace message {

MessageRepository::MessageRepository(std::shared_ptr<odb::pgsql::database> db)
    : db_(std::move(db)) {}

bool MessageRepository::create(Message& msg) {
    try {
        odb::transaction t(db_->begin());
        db_->persist(msg);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

std::optional<Message> MessageRepository::find_by_id(uint64_t msg_id) {
    try {
        odb::transaction t(db_->begin());
        std::unique_ptr<Message> loaded(db_->load<Message>(msg_id));
        t.commit();
        if (loaded) {
            return std::move(*loaded);
        }
        return std::nullopt;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::vector<Message> MessageRepository::find_conversation(
    const std::string& user_a,
    const std::string& user_b,
    int64_t before_time,
    int limit)
{
    std::vector<Message> results;
    try {
        odb::transaction t(db_->begin());
        odb::query<Message> q =
            (odb::query<Message>::sender_uid == user_a &&
             odb::query<Message>::receiver_uid == user_b &&
             odb::query<Message>::create_time < before_time) ||
            (odb::query<Message>::sender_uid == user_b &&
             odb::query<Message>::receiver_uid == user_a &&
             odb::query<Message>::create_time < before_time);
        odb::result<Message> r(db_->query<Message>(q + "ORDER BY create_time"));
        for (auto it = r.begin(); it != r.end() && static_cast<int>(results.size()) < limit; ++it) {
            results.push_back(*it);
        }
        t.commit();
    } catch (const odb::exception&) {
    }
    return results;
}

std::vector<Message> MessageRepository::find_offline(
    const std::string& receiver_uid,
    int64_t before_time,
    int limit)
{
    std::vector<Message> results;
    try {
        odb::transaction t(db_->begin());
        odb::query<Message> q =
            odb::query<Message>::receiver_uid == receiver_uid &&
            odb::query<Message>::status == MessageStatus::SENT &&
            odb::query<Message>::create_time < before_time;
        odb::result<Message> r(db_->query<Message>(q + "ORDER BY create_time"));
        for (auto it = r.begin(); it != r.end() && static_cast<int>(results.size()) < limit; ++it) {
            results.push_back(*it);
        }
        t.commit();
    } catch (const odb::exception&) {
    }
    return results;
}

bool MessageRepository::mark_delivered(uint64_t msg_id, int64_t delivered_time) {
    try {
        odb::transaction t(db_->begin());
        std::unique_ptr<Message> msg(db_->load<Message>(msg_id));
        if (!msg) return false;
        msg->status(MessageStatus::DELIVERED);
        msg->delivered_time(delivered_time);
        db_->update(*msg);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

bool MessageRepository::mark_read(uint64_t msg_id, int64_t read_time) {
    try {
        odb::transaction t(db_->begin());
        std::unique_ptr<Message> msg(db_->load<Message>(msg_id));
        if (!msg) return false;
        msg->status(MessageStatus::READ);
        msg->read_time(read_time);
        db_->update(*msg);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

} // namespace message
} // namespace service
} // namespace im
