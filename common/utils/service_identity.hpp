#ifndef SERVICE_IDENTITY_HPP
#define SERVICE_IDENTITY_HPP

#include <chrono>
#include <memory>
#include <string>
#include <unordered_map>

namespace im {
namespace utils {

/**
 * @brief 服务标识信息结构体
 */
struct ServiceIdentity {
    std::string service_name;  // 服务名称（如: gateway, message-service, user-service）
    std::string instance_id;   // 实例唯一标识
    std::string cluster_id;    // 集群标识
    std::string region;        // 区域/数据中心
    std::string platform;      // 平台信息
    std::string version;       // 服务版本
    uint64_t startup_time;     // 启动时间戳

    // 构造完整的设备ID
    std::string get_device_id() const {
        return service_name + "-" + cluster_id + "-" + instance_id;
    }

    // 构造完整的平台标识
    std::string get_platform_info() const { return platform + "-" + region + "-server"; }

    // 获取服务的完整标识
    std::string get_full_identity() const {
        return service_name + "/" + cluster_id + "/" + instance_id + "@" + region;
    }
};

/**
 * @brief 服务标识配置
 */
struct ServiceIdentityConfig {
    std::string service_name;               // 必填：服务名称
    std::string cluster_id = "default";     // 集群ID，默认"default"
    std::string region = "local";           // 区域，默认"local"
    std::string version = "1.0.0";          // 版本，默认"1.0.0"
    bool auto_generate_instance_id = true;  // 是否自动生成实例ID
    std::string custom_instance_id;  // 自定义实例ID（当auto_generate_instance_id=false时使用）

    // 环境变量配置
    std::string env_service_name = "SERVICE_NAME";
    std::string env_cluster_id = "CLUSTER_ID";
    std::string env_region = "REGION";
    std::string env_instance_id = "INSTANCE_ID";
};

/**
 * @brief 分布式服务标识管理器
 *
 * @details 为分布式系统中的每个服务实例提供唯一标识
 *          支持环境变量配置、自动生成、服务发现集成
 */
class ServiceIdentityManager {
public:
    /**
     * @brief 获取单例实例
     */
    static ServiceIdentityManager& getInstance();

    /**
     * @brief 初始化服务标识
     * @param config 服务标识配置
     * @return 是否初始化成功
     */
    bool initialize(const ServiceIdentityConfig& config);

    /**
     * @brief 从环境变量初始化
     * @param service_name 服务名称
     * @return 是否初始化成功
     */
    bool initializeFromEnv(const std::string& service_name);

    /**
     * @brief 获取当前服务标识
     */
    const ServiceIdentity& getServiceIdentity() const;

    /**
     * @brief 获取设备ID（用于protobuf响应）
     */
    std::string getDeviceId() const;

    /**
     * @brief 获取平台信息（用于protobuf响应）
     */
    std::string getPlatformInfo() const;

    /**
     * @brief 是否已初始化
     */
    bool isInitialized() const;

    /**
     * @brief 更新服务状态信息
     */
    void updateServiceInfo(const std::unordered_map<std::string, std::string>& info);

    /**
     * @brief 获取服务运行时间（秒）
     */
    uint64_t getUptimeSeconds() const;

private:
    ServiceIdentityManager() = default;
    ~ServiceIdentityManager() = default;
    ServiceIdentityManager(const ServiceIdentityManager&) = delete;
    ServiceIdentityManager& operator=(const ServiceIdentityManager&) = delete;

    /**
     * @brief 自动生成实例ID
     */
    std::string generateInstanceId();

    /**
     * @brief 检测平台信息
     */
    std::string detectPlatform();

    /**
     * @brief 从环境变量读取配置
     */
    std::string getEnvVar(const std::string& var_name, const std::string& default_value = "");

private:
    bool initialized_ = false;
    ServiceIdentity identity_;
    std::unordered_map<std::string, std::string> runtime_info_;
};

/**
 * @brief 全局便捷函数
 */
namespace ServiceId {
// 获取当前服务的设备ID
std::string getDeviceId();

// 获取当前服务的平台信息
std::string getPlatformInfo();

// 获取当前服务的完整标识
std::string getFullIdentity();

// 检查是否已初始化
bool isReady();
}  // namespace ServiceId

}  // namespace utils
}  // namespace im

#endif  // SERVICE_IDENTITY_HPP