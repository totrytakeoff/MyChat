#include "../../common/network/websocket_server.hpp"
#include "../../common/utils/log_manager.hpp"
#include "connection_manager.hpp"

#include <nlohmann/json.hpp>
#include <unordered_set>

namespace im {
namespace gateway {

// DeviceSessionInfo序列化方法实现
nlohmann::json DeviceSessionInfo::to_json() const {
    nlohmann::json j;
    j["session_id"] = session_id;
    j["device_id"] = device_id;
    j["platform"] = platform;
    // 将时间点转换为毫秒数进行存储
    j["connect_time"] =
            std::chrono::duration_cast<std::chrono::milliseconds>(connect_time.time_since_epoch())
                    .count();
    return j;
}

DeviceSessionInfo DeviceSessionInfo::from_json(const nlohmann::json& j) {
    DeviceSessionInfo info;
    info.session_id = j.value("session_id", "");
    info.device_id = j.value("device_id", "");
    info.platform = j.value("platform", "");
    // 从毫秒数恢复时间点
    auto timestamp = j.value("connect_time", 0LL);
    info.connect_time = std::chrono::system_clock::time_point(std::chrono::milliseconds(timestamp));
    return info;
}

// ConnectionManager实现

/**
 * @brief ConnectionManager构造函数
 * @param platform_config_path 平台配置文件路径
 * @param websocket_server WebSocket服务器指针
 *
 * @details 初始化平台令牌策略管理器，用于获取平台特定的配置信息
 */
ConnectionManager::ConnectionManager(const std::string& platform_config_path,
                                     network::WebSocketServer* websocket_server)
        : websocket_server_(websocket_server) {
    platform_strategy_ = std::make_unique<PlatformTokenStrategy>(platform_config_path);
}

/**
 * @brief 生成Redis键名
 * @param prefix 键前缀
 * @param user_id 用户ID
 * @return 完整的Redis键名
 *
 * @details 生成格式为"prefix:user_id"的Redis键名，用于Redis存储的键名构建
 */
std::string ConnectionManager::generate_redis_key(const std::string& prefix,
                                                  const std::string& user_id) const {
    return prefix + ":" + user_id;
}

/**
 * @brief 生成设备字段名
 * @param device_id 设备ID
 * @param platform 平台标识
 * @return 设备字段名
 *
 * @details 生成格式为"device_id:platform"的设备字段名，用于Hash结构中的字段名
 */
std::string ConnectionManager::generate_device_field(const std::string& device_id,
                                                     const std::string& platform) const {
    return device_id + ":" + platform;
}

/**
 * @brief 添加连接（支持多设备）
 * @param user_id 用户ID
 * @param device_id 设备ID
 * @param platform 平台标识
 * @param session WebSocket会话
 * @return 是否添加成功
 *
 * @details 该方法会检查是否需要踢掉同平台的旧连接，然后将新的会话信息
 *          存储到Redis中，包括用户会话映射、设备到用户映射和会话到用户映射。
 *
 *          Redis存储结构：
 *          1. user:sessions:{user_id} Hash -
 * 存储用户会话映射，字段为device_id:platform，值为会话信息JSON
 *          2. user:platform:{user_id} Set - 存储用户设备集合，成员为device_id:platform
 *          3. session:user:{session_id} Hash - 存储会话到用户映射，字段包括user_id, device_id,
 * platform
 */
bool ConnectionManager::add_connection(const std::string& user_id,
                                       const std::string& device_id,
                                       const std::string& platform,
                                       SessionPtr session) {
    try {
        // 检查是否需要踢掉同平台的旧连接
        auto kicked_session_id = check_and_kick_same_platform(user_id, device_id, platform);
        if (!kicked_session_id.empty()) {
            im::utils::LogManager::GetLogger("connection_manager")
                    ->info("Kicked old session {} for user {} on platform {}", kicked_session_id,
                           user_id, platform);
        }

        // 创建设备会话信息
        DeviceSessionInfo session_info;
        session_info.session_id = session->get_session_id();
        session_info.device_id = device_id;
        session_info.platform = platform;
        session_info.connect_time = std::chrono::system_clock::now();

        // 序列化会话信息
        auto session_json = session_info.to_json();

        // 生成Redis键和字段
        auto sessions_key = generate_redis_key("user:sessions", user_id);
        auto devices_key = generate_redis_key("user:platform", user_id);
        auto session_user_key = generate_redis_key("session:user", session_info.session_id);
        auto device_field = generate_device_field(device_id, platform);

        // 使用Redis事务保存所有相关信息
        RedisManager::GetInstance().execute([&](auto& redis) {
            // 1. 保存用户会话映射
            redis.hset(sessions_key, device_field, session_json.dump());

            // 2. 保存设备到用户映射
            redis.sadd(devices_key, device_field);

            // 3. 保存会话到用户映射
            redis.hset(session_user_key, "user_id", user_id);
            redis.hset(session_user_key, "device_id", device_id);
            redis.hset(session_user_key, "platform", platform);

            // 4. 将用户加入全局在线集合
            redis.sadd("online:users", user_id);
        });

        return true;
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("connection_manager")
                ->error("Failed to add connection for user {}: {}", user_id, e.what());
        return false;
    }
}

/**
 * @brief 移除连接（通过用户ID和设备ID）
 * @param user_id 用户ID
 * @param device_id 设备ID
 *
 * @details 从Redis中删除指定用户和设备的会话信息，包括所有相关的映射关系。
 *          删除的键包括：
 *          1. user:sessions:{user_id} 中对应的字段
 *          2. user:platform:{user_id} 中对应的成员
 *          3. session:user:{session_id} 完整的键
 */
void ConnectionManager::remove_connection(const std::string& user_id,
                                          const std::string& device_id) {
    try {
        // 获取用户的所有会话信息
        auto sessions_key = generate_redis_key("user:sessions", user_id);
        auto devices_key = generate_redis_key("user:platform", user_id);

        RedisManager::GetInstance().execute([&](auto& redis) {
            std::unordered_map<std::string, std::string> sessions;
            redis.hgetall(sessions_key, std::inserter(sessions, sessions.begin()));

            for (const auto& [field, value] : sessions) {
                // 解析设备ID和平台信息 device_id:platform
                auto pos = field.find(':');
                if (pos != std::string::npos) {
                    auto field_device_id = field.substr(0, pos);
                    if (field_device_id == device_id) {
                        // 找到匹配的设备，删除相关记录
                        redis.hdel(sessions_key, field);
                        redis.srem(devices_key, field);

                        // 删除会话到用户的映射
                        auto session_info =
                                DeviceSessionInfo::from_json(nlohmann::json::parse(value));
                        auto session_user_key =
                                generate_redis_key("session:user", session_info.session_id);
                        redis.del(session_user_key);
                        break;
                    }
                }
            }

            // 如果用户已无任何会话，移出全局在线集合
            std::unordered_map<std::string, std::string> after_sessions;
            redis.hgetall(sessions_key, std::inserter(after_sessions, after_sessions.begin()));
            if (after_sessions.empty()) {
                redis.srem("online:users", user_id);
            }
        });
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("connection_manager")
                ->error("Failed to remove connection for user {} device {}: {}", user_id, device_id,
                        e.what());
    }
}

/**
 * @brief 移除连接（通过会话）
 * @param session WebSocket会话
 *
 * @details 通过会话ID获取用户信息，然后调用remove_connection(user_id, device_id)方法。
 *          首先从session:user:{session_id}键中获取用户信息，然后删除对应的连接信息。
 */
void ConnectionManager::remove_connection(SessionPtr session) {
    try {
        auto session_id = session->get_session_id();
        auto session_user_key = generate_redis_key("session:user", session_id);

        // 获取会话对应的用户信息
        std::string user_id, device_id, platform;
        RedisManager::GetInstance().execute([&](auto& redis) {
            std::unordered_map<std::string, std::string> user_info;
            redis.hgetall(session_user_key, std::inserter(user_info, user_info.begin()));
            user_id = user_info["user_id"];
            device_id = user_info["device_id"];
            platform = user_info["platform"];
        });

        // 删除连接
        if (!user_id.empty()) {
            remove_connection(user_id, device_id);
        }
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("connection_manager")
                ->error("Failed to remove connection by session: {}", e.what());
    }
}

/**
 * @brief 获取指定设备的会话
 * @param user_id 用户ID
 * @param device_id 设备ID
 * @param platform 平台标识
 * @return WebSocket会话指针，如果找不到则返回nullptr
 *
 * @details 通过Redis查询会话信息，然后通过WebSocketServer获取实际的SessionPtr。
 *          首先从user:sessions:{user_id}键中查询设备字段，获取会话信息，
 *          然后通过WebSocketServer获取实际的会话对象。
 */
SessionPtr ConnectionManager::get_session(const std::string& user_id,
                                          const std::string& device_id,
                                          const std::string& platform) {
    try {
        auto sessions_key = generate_redis_key("user:sessions", user_id);
        auto device_field = generate_device_field(device_id, platform);

        auto result = RedisManager::GetInstance().execute(
                [&](auto& redis) { return redis.hget(sessions_key, device_field); });

        if (result) {
            auto session_info = DeviceSessionInfo::from_json(nlohmann::json::parse(*result));
            // 通过WebSocketServer获取实际的SessionPtr
            if (websocket_server_) {
                return websocket_server_->get_session(session_info.session_id);
            }
        }
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("connection_manager")
                ->error("Failed to get session for user {} device {}: {}", user_id, device_id,
                        e.what());
    }

    return nullptr;
}

/**
 * @brief 获取用户的所有在线设备会话
 * @param user_id 用户ID
 * @return 设备会话信息列表
 *
 * @details 从Redis中获取用户的所有会话信息，返回DeviceSessionInfo对象列表。
 *          查询user:sessions:{user_id}键中的所有字段，解析每个字段对应的会话信息。
 */
std::vector<DeviceSessionInfo> ConnectionManager::get_user_sessions(const std::string& user_id) {
    std::vector<DeviceSessionInfo> sessions;

    try {
        auto sessions_key = generate_redis_key("user:sessions", user_id);

        RedisManager::GetInstance().execute([&](auto& redis) {
            std::unordered_map<std::string, std::string> session_map;
            redis.hgetall(sessions_key, std::inserter(session_map, session_map.begin()));

            for (const auto& [field, value] : session_map) {
                try {
                    auto session_info = DeviceSessionInfo::from_json(nlohmann::json::parse(value));
                    sessions.push_back(session_info);
                } catch (const std::exception& e) {
                    im::utils::LogManager::GetLogger("connection_manager")
                            ->error("Failed to parse session info for user {}: {}", user_id,
                                    e.what());
                }
            }
        });
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("connection_manager")
                ->error("Failed to get user sessions for user {}: {}", user_id, e.what());
    }

    return sessions;
}

/**
 * @brief 获取在线用户列表
 * @return 在线用户ID列表
 *
 * @note 该实现需要一个全局的在线用户集合，可以考虑使用Redis的集合来维护。
 *       当前实现查询online:users键中的所有成员，实际使用中可能需要根据具体需求调整。
 */
std::vector<std::string> ConnectionManager::get_online_users() {
    // 这个实现需要一个全局的在线用户集合
    // 可以考虑使用Redis的集合来维护在线用户列表
    std::vector<std::string> users;

    try {
        RedisManager::GetInstance().execute([&](auto& redis) {
            // 这里假设我们有一个全局的在线用户集合
            // 实际实现中可能需要遍历所有用户会话键
            std::unordered_set<std::string> user_set;
            redis.smembers("online:users", std::inserter(user_set, user_set.begin()));

            for (const auto& user : user_set) {
                users.push_back(user);
            }
        });
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("connection_manager")
                ->error("Failed to get online users: {}", e.what());
    }

    return users;
}

/**
 * @brief 获取在线用户数量
 * @return 在线用户数量
 *
 * @details 查询online:users键中的成员数量，返回在线用户总数。
 */
size_t ConnectionManager::get_online_count() const {
    try {
        auto count = RedisManager::GetInstance().execute(
                [&](auto& redis) { return redis.scard("online:users"); });
        return static_cast<size_t>(count);
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("connection_manager")
                ->error("Failed to get online count: {}", e.what());
        return 0;
    }
}

/**
 * @brief 检查是否需要踢掉同平台的旧连接
 * @param user_id 用户ID
 * @param device_id 设备ID
 * @param platform 平台标识
 * @return 被踢掉的会话ID，如果没有则为空
 *
 * @details 根据平台配置决定是否允许多设备登录，如果不允许则踢掉同平台的旧连接。
 *          查询user:sessions:{user_id}键中的所有会话，找到同一平台但不同设备的会话并踢掉。
 */
std::string ConnectionManager::check_and_kick_same_platform(const std::string& user_id,
                                                            const std::string& device_id,
                                                            const std::string& platform) {
    // 获取平台配置
    const auto& config = platform_strategy_->get_platform_token_config(platform);

    // 如果该平台允许多设备登录，则不踢号
    if (config.enable_multi_device) {
        return "";
    }

    try {
        // 查找同一平台的已登录设备
        auto key = generate_redis_key("user:sessions", user_id);
        auto field_pattern = ":" + platform;

        std::string old_session_id = "";

        RedisManager::GetInstance().execute([&](auto& redis) {
            std::unordered_map<std::string, std::string> sessions;
            redis.hgetall(key, std::inserter(sessions, sessions.begin()));

            for (const auto& [field, value] : sessions) {
                // 检查是否为同一平台的设备（但不是当前设备）
                if (field.find(field_pattern) != std::string::npos &&
                    field.find(device_id) == std::string::npos) {
                    // 解析旧会话信息
                    auto session_info = nlohmann::json::parse(value);
                    old_session_id = session_info.value("session_id", "");
                    break;
                }
            }
        });

        // 如果找到旧会话，断开它
        if (!old_session_id.empty()) {
            disconnect_session(old_session_id);
            return old_session_id;
        }
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("connection_manager")
                ->error("Failed to check and kick same platform session: {}", e.what());
    }

    return "";
}

/**
 * @brief 断开指定会话
 * @param session_id 会话ID
 *
 * @details 通过WebSocketServer获取会话并关闭连接，用于实现登录挤号功能。
 */
void ConnectionManager::disconnect_session(const std::string& session_id) {
    // 通过WebSocketServer断开指定会话
    if (websocket_server_) {
        auto session = websocket_server_->get_session(session_id);
        if (session) {
            session->close();
            im::utils::LogManager::GetLogger("connection_manager")
                    ->info("Disconnected session: {}", session_id);
        }
    } else {
        im::utils::LogManager::GetLogger("connection_manager")
                ->info("Disconnecting session: {}", session_id);
    }
}

/**
 * @brief 检查用户在指定平台是否已登录
 * @param user_id 用户ID
 * @param platform 平台标识
 * @return 是否已登录
 *
 * @details 查询Redis中的用户平台设备集合，检查是否有指定平台的设备。
 *          查询user:platform:{user_id}键中的所有成员，检查是否包含指定平台的设备。
 */
bool ConnectionManager::is_user_online_on_platform(const std::string& user_id,
                                                   const std::string& platform) {
    try {
        auto key = generate_redis_key("user:platform", user_id);
        auto result = RedisManager::GetInstance().execute([&](auto& redis) {
            std::unordered_set<std::string> device_set;
            redis.smembers(key, std::inserter(device_set, device_set.begin()));
            return device_set;
        });

        // 检查是否有该平台的设备
        for (const auto& device : result) {
            if (device.find(":" + platform) != std::string::npos) {
                return true;
            }
        }
        return false;
    } catch (const std::exception& e) {
        im::utils::LogManager::GetLogger("connection_manager")
                ->error("Failed to check user platform status: {}", e.what());
        return false;
    }
}

}  // namespace gateway
}  // namespace im