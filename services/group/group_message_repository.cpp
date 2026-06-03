#include "group_message_repository.hpp"

#include <odb/transaction.hxx>

namespace im {
namespace service {
namespace group {

GroupMessageRepository::GroupMessageRepository(
    std::shared_ptr<odb::pgsql::database> db)
    : db_(std::move(db))
{}

bool GroupMessageRepository::create(GroupMessage& msg) {
    try {
        odb::transaction t(db_->begin());
        db_->persist(msg);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

std::vector<GroupMessage> GroupMessageRepository::find_by_group(
    uint64_t group_id, int64_t before_time, int limit)
{
    std::vector<GroupMessage> result;
    try {
        odb::transaction t(db_->begin());
        odb::query<GroupMessage> q(
            odb::query<GroupMessage>::group_id == group_id &&
            odb::query<GroupMessage>::created_at < before_time);
        odb::result<GroupMessage> r(db_->query<GroupMessage>(
            q + "ORDER BY created_at DESC LIMIT " + std::to_string(limit)));
        for (const auto& m : r) {
            result.push_back(m);
        }
        t.commit();
    } catch (const odb::exception&) {
    }
    return result;
}

std::vector<GroupMessage> GroupMessageRepository::find_offline(
    uint64_t group_id, int64_t after_time)
{
    std::vector<GroupMessage> result;
    try {
        odb::transaction t(db_->begin());
        odb::query<GroupMessage> q(
            odb::query<GroupMessage>::group_id == group_id &&
            odb::query<GroupMessage>::created_at > after_time);
        odb::result<GroupMessage> r(db_->query<GroupMessage>(
            q + "ORDER BY created_at ASC"));
        for (const auto& m : r) {
            result.push_back(m);
        }
        t.commit();
    } catch (const odb::exception&) {
    }
    return result;
}

} // namespace group
} // namespace service
} // namespace im
