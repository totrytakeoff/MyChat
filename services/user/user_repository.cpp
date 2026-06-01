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

} // namespace user
} // namespace service
} // namespace im
