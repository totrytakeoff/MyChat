#ifndef IM_SERVICE_GROUP_GROUP_MESSAGE_REPOSITORY_HPP
#define IM_SERVICE_GROUP_GROUP_MESSAGE_REPOSITORY_HPP

#include <memory>
#include <vector>

#include <odb/pgsql/database.hxx>

#include <group_message.hpp>
#include <group_message-odb.hxx>

namespace im {
namespace service {
namespace group {

class GroupMessageRepository {
public:
    explicit GroupMessageRepository(std::shared_ptr<odb::pgsql::database> db);

    bool create(GroupMessage& msg);
    std::vector<GroupMessage> find_by_group(uint64_t group_id, int64_t before_time,
                                            int limit = 50);
    std::vector<GroupMessage> find_offline(uint64_t group_id, int64_t after_time);

private:
    std::shared_ptr<odb::pgsql::database> db_;
};

} // namespace group
} // namespace service
} // namespace im

#endif // IM_SERVICE_GROUP_GROUP_MESSAGE_REPOSITORY_HPP
