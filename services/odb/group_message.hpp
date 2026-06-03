#ifndef IM_ODB_GROUP_MESSAGE_HPP
#define IM_ODB_GROUP_MESSAGE_HPP

#include <odb/core.hxx>
#include <cstdint>
#include <string>

namespace im {
namespace service {
namespace group {

#pragma db object table("im_group_messages")
class GroupMessage {
public:
    GroupMessage() = default;
    GroupMessage(uint64_t group_id, const std::string& sender_uid,
                 const std::string& content, int64_t created_at)
        : group_id_(group_id)
        , sender_uid_(sender_uid)
        , content_(content)
        , created_at_(created_at)
    {}

    uint64_t id() const { return id_; }
    uint64_t group_id() const { return group_id_; }
    const std::string& sender_uid() const { return sender_uid_; }
    const std::string& content() const { return content_; }
    int64_t created_at() const { return created_at_; }

private:
    friend class odb::access;

    #pragma db id auto
    uint64_t id_ = 0;

    #pragma db not_null
    uint64_t group_id_ = 0;

    #pragma db not_null
    std::string sender_uid_;

    #pragma db not_null
    std::string content_;

    #pragma db not_null
    int64_t created_at_ = 0;
};

} // namespace group
} // namespace service
} // namespace im

#endif // IM_ODB_GROUP_MESSAGE_HPP
