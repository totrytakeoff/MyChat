#include "user_repository.hpp"

#include <utility>

#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/transaction.hxx>
#include <odb/query.hxx>

#include <user.hpp>
#include <user-odb.hxx>

namespace im {
namespace service {
namespace user {

UserRepository::UserRepository(std::shared_ptr<odb::pgsql::database> db)
    : db_(std::move(db)) {}

bool UserRepository::create(const User& user) {
    try {
        odb::transaction t(db_->begin());
        db_->persist(user);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

std::optional<User> UserRepository::find_by_uid(const std::string& uid) {
    try {
        odb::transaction t(db_->begin());
        std::unique_ptr<User> loaded(db_->load<User>(uid));
        t.commit();
        if (loaded) {
            return std::move(*loaded);
        }
        return std::nullopt;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::optional<User> UserRepository::find_by_account(const std::string& account) {
    try {
        odb::transaction t(db_->begin());
        odb::query<User> q(odb::query<User>::account == account);
        odb::result<User> r(db_->query<User>(q));
        auto it = r.begin();
        if (it != r.end()) {
            User user = *it;
            t.commit();
            return user;
        }
        t.commit();
        return std::nullopt;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

std::vector<User> UserRepository::search_by_account_or_nickname(
    const std::string& keyword,
    std::size_t limit) {
    std::vector<User> users;
    if (keyword.empty() || limit == 0) {
        return users;
    }

    try {
        odb::transaction t(db_->begin());
        const std::string pattern = "%" + keyword + "%";
        odb::query<User> q(
            odb::query<User>::account.like(pattern) ||
            odb::query<User>::nickname.like(pattern));
        odb::result<User> r(db_->query<User>(q));
        for (const auto& user : r) {
            users.push_back(user);
            if (users.size() >= limit) {
                break;
            }
        }
        t.commit();
    } catch (const odb::exception&) {
        return {};
    }
    return users;
}

bool UserRepository::account_exists(const std::string& account) {
    return find_by_account(account).has_value();
}

bool UserRepository::update_last_login(const std::string& uid, int64_t last_login) {
    try {
        odb::transaction t(db_->begin());
        std::unique_ptr<User> user(db_->load<User>(uid));
        if (!user) return false;
        user->last_login(last_login);
        db_->update(*user);
        t.commit();
        return true;
    } catch (const odb::exception&) {
        return false;
    }
}

std::optional<User> UserRepository::update_profile(const std::string& uid,
                                                   const std::string& nickname,
                                                   const std::string& avatar,
                                                   Gender gender,
                                                   const std::string& signature) {
    try {
        odb::transaction t(db_->begin());
        std::unique_ptr<User> user(db_->load<User>(uid));
        if (!user) {
            t.commit();
            return std::nullopt;
        }
        user->nickname(nickname);
        user->avatar(avatar);
        user->gender(gender);
        user->signature(signature);
        db_->update(*user);
        User updated = *user;
        t.commit();
        return updated;
    } catch (const odb::exception&) {
        return std::nullopt;
    }
}

} // namespace user
} // namespace service
} // namespace im
