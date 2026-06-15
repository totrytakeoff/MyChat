#include "group_repository.hpp"

#include <utility>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>
#include <odb/query.hxx>

#include <group.hpp>
#include <group-odb.hxx>

namespace im {
namespace service {
namespace group {

GroupRepository::GroupRepository(std::shared_ptr<odb::pgsql::database> db)
    : db_(std::move(db)) {}

// === Group operations ===

bool GroupRepository::create_group(Group& g) {
    try {
        odb::transaction t(db_->begin());
        db_->persist(g);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

bool GroupRepository::create_group_with_owner(Group& g, GroupMember& owner) {
    try {
        odb::transaction t(db_->begin());
        db_->persist(g);
        owner.group_id(g.group_id());
        db_->persist(owner);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

std::optional<Group> GroupRepository::find_group_by_id(uint64_t group_id) {
    try {
        odb::transaction t(db_->begin());
        std::unique_ptr<Group> loaded(db_->load<Group>(group_id));
        t.commit();
        if (loaded) {
            return std::move(*loaded);
        }
        return std::nullopt;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::vector<Group> GroupRepository::find_groups_by_user(
    const std::string& user_uid)
{
    std::vector<uint64_t> group_ids;
    try {
        odb::transaction t(db_->begin());
        odb::query<GroupMember> mq(
            odb::query<GroupMember>::user_uid == user_uid);
        odb::result<GroupMember> mr(db_->query<GroupMember>(mq));
        for (const auto& m : mr) {
            group_ids.push_back(m.group_id());
        }
        t.commit();
    } catch (const odb::exception&) {
        return {};
    }

    std::vector<Group> result;
    for (auto gid : group_ids) {
        auto g = find_group_by_id(gid);
        if (g.has_value()) {
            result.push_back(std::move(g.value()));
        }
    }
    return result;
}

std::vector<Group> GroupRepository::search_groups(const std::string& keyword,
                                                  std::size_t limit) {
    std::vector<Group> groups;
    if (keyword.empty() || limit == 0) {
        return groups;
    }

    try {
        auto by_id = find_group_by_id(static_cast<uint64_t>(std::stoull(keyword)));
        if (by_id.has_value()) {
            groups.push_back(*by_id);
            return groups;
        }
    } catch (...) {
    }

    try {
        odb::transaction t(db_->begin());
        const std::string pattern = "%" + keyword + "%";
        odb::query<Group> q(odb::query<Group>::name.like(pattern));
        odb::result<Group> r(db_->query<Group>(q));
        for (const auto& group : r) {
            groups.push_back(group);
            if (groups.size() >= limit) {
                break;
            }
        }
        t.commit();
    } catch (const odb::exception&) {
        return {};
    }
    return groups;
}

// === Member operations ===

bool GroupRepository::add_member(GroupMember m) {
    try {
        odb::transaction t(db_->begin());
        db_->persist(m);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

bool GroupRepository::remove_member(uint64_t group_id,
                                    const std::string& user_uid)
{
    try {
        odb::transaction t(db_->begin());
        odb::query<GroupMember> q(
            odb::query<GroupMember>::group_id == group_id &&
            odb::query<GroupMember>::user_uid == user_uid);
        db_->erase_query<GroupMember>(q);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

bool GroupRepository::update_member_role(uint64_t group_id,
                                         const std::string& user_uid,
                                         GroupRole role)
{
    try {
        odb::transaction t(db_->begin());
        odb::query<GroupMember> q(
            odb::query<GroupMember>::group_id == group_id &&
            odb::query<GroupMember>::user_uid == user_uid);
        odb::result<GroupMember> r(db_->query<GroupMember>(q));
        auto it = r.begin();
        if (it == r.end()) {
            t.commit();
            return false;
        }
        GroupMember m = *it;
        m.role(role);
        db_->update(m);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

std::vector<GroupMember> GroupRepository::find_members(uint64_t group_id) {
    std::vector<GroupMember> result;
    try {
        odb::transaction t(db_->begin());
        odb::query<GroupMember> q(
            odb::query<GroupMember>::group_id == group_id);
        odb::result<GroupMember> r(db_->query<GroupMember>(q));
        for (const auto& m : r) {
            result.push_back(m);
        }
        t.commit();
    } catch (const odb::exception&) {
    }
    return result;
}

std::optional<GroupMember> GroupRepository::find_member(
    uint64_t group_id, const std::string& user_uid)
{
    try {
        odb::transaction t(db_->begin());
        odb::query<GroupMember> q(
            odb::query<GroupMember>::group_id == group_id &&
            odb::query<GroupMember>::user_uid == user_uid);
        odb::result<GroupMember> r(db_->query<GroupMember>(q));
        auto it = r.begin();
        if (it != r.end()) {
            GroupMember m = *it;
            t.commit();
            return m;
        }
        t.commit();
        return std::nullopt;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

} // namespace group
} // namespace service
} // namespace im
