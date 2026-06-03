#include "friend_repository.hpp"

#include <utility>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>
#include <odb/query.hxx>

#include <friend.hpp>
#include <friend-odb.hxx>

namespace im {
namespace service {
namespace friend_ {

FriendRepository::FriendRepository(std::shared_ptr<odb::pgsql::database> db)
    : db_(std::move(db)) {}

bool FriendRepository::create(Friend f) {
    try {
        odb::transaction t(db_->begin());
        db_->persist(f);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

std::optional<Friend> FriendRepository::find_by_id(uint64_t friend_id) {
    try {
        odb::transaction t(db_->begin());
        std::unique_ptr<Friend> loaded(db_->load<Friend>(friend_id));
        t.commit();
        if (loaded) {
            return std::move(*loaded);
        }
        return std::nullopt;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::optional<Friend> FriendRepository::find_by_pair(
    const std::string& requester_uid,
    const std::string& target_uid)
{
    try {
        odb::transaction t(db_->begin());
        odb::query<Friend> q(
            odb::query<Friend>::requester_uid == requester_uid &&
            odb::query<Friend>::target_uid == target_uid);
        odb::result<Friend> r(db_->query<Friend>(q));
        auto it = r.begin();
        if (it != r.end()) {
            Friend f = *it;
            t.commit();
            return f;
        }
        t.commit();
        return std::nullopt;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::optional<Friend> FriendRepository::find_relationship(
    const std::string& user_a,
    const std::string& user_b)
{
    try {
        odb::transaction t(db_->begin());
        odb::query<Friend> q(
            (odb::query<Friend>::requester_uid == user_a &&
             odb::query<Friend>::target_uid == user_b) ||
            (odb::query<Friend>::requester_uid == user_b &&
             odb::query<Friend>::target_uid == user_a));
        odb::result<Friend> r(db_->query<Friend>(q));
        auto it = r.begin();
        if (it != r.end()) {
            Friend f = *it;
            t.commit();
            return f;
        }
        t.commit();
        return std::nullopt;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::vector<Friend> FriendRepository::find_friends(const std::string& uid) {
    std::vector<Friend> result;
    try {
        odb::transaction t(db_->begin());
        odb::query<Friend> q(
            (odb::query<Friend>::requester_uid == uid ||
             odb::query<Friend>::target_uid == uid) &&
            odb::query<Friend>::status == FriendStatus::ACCEPTED);
        odb::result<Friend> r(db_->query<Friend>(q));
        for (const auto& f : r) {
            result.push_back(f);
        }
        t.commit();
    } catch (const odb::exception&) {
    }
    return result;
}

std::vector<Friend> FriendRepository::find_pending(const std::string& uid) {
    std::vector<Friend> result;
    try {
        odb::transaction t(db_->begin());
        odb::query<Friend> q(
            odb::query<Friend>::target_uid == uid &&
            odb::query<Friend>::status == FriendStatus::PENDING);
        odb::result<Friend> r(db_->query<Friend>(q));
        for (const auto& f : r) {
            result.push_back(f);
        }
        t.commit();
    } catch (const odb::exception&) {
    }
    return result;
}

bool FriendRepository::update_status(uint64_t friend_id, FriendStatus status) {
    try {
        odb::transaction t(db_->begin());
        std::unique_ptr<Friend> loaded(db_->load<Friend>(friend_id));
        if (!loaded) return false;
        loaded->status(status);
        db_->update(*loaded);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

bool FriendRepository::relationship_exists(
    const std::string& user_a, const std::string& user_b)
{
    return find_relationship(user_a, user_b).has_value();
}

} // namespace friend_
} // namespace service
} // namespace im
