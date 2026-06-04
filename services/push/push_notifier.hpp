#ifndef SERVICES_PUSH_PUSH_NOTIFIER_HPP
#define SERVICES_PUSH_PUSH_NOTIFIER_HPP

#include <cstdint>
#include <string>

namespace im::service::push {

// Boundary used by message entry points to request best-effort push delivery
// for an already persisted message.
class PushNotifier {
public:
    virtual ~PushNotifier() = default;

    virtual void notify_user(const std::string& receiver_uid,
                             uint64_t msg_id,
                             const std::string& content) = 0;
};

} // namespace im::service::push

#endif // SERVICES_PUSH_PUSH_NOTIFIER_HPP
