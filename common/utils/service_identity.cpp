#include "service_identity.hpp"
#include <random>
#include <unistd.h>
#include <cstdlib>
#include <iomanip>
#include <sstream>

namespace im {
namespace utils {

ServiceIdentityManager& ServiceIdentityManager::getInstance() {
    static ServiceIdentityManager instance;
    return instance;
}

bool ServiceIdentityManager::initialize(const ServiceIdentityConfig& config) {
    if (initialized_) {
        return true;  // 已经初始化过
    }
    
    // 从环境变量获取配置（优先级高于传入的config）
    identity_.service_name = getEnvVar(config.env_service_name, config.service_name);
    identity_.cluster_id = getEnvVar(config.env_cluster_id, config.cluster_id);
    identity_.region = getEnvVar(config.env_region, config.region);
    identity_.version = config.version;
    
    // 实例ID处理
    if (config.auto_generate_instance_id) {
        std::string env_instance_id = getEnvVar(config.env_instance_id);
        if (!env_instance_id.empty()) {
            identity_.instance_id = env_instance_id;
        } else {
            identity_.instance_id = generateInstanceId();
        }
    } else {
        identity_.instance_id = config.custom_instance_id;
    }
    
    // 检测平台信息
    identity_.platform = detectPlatform();
    
    // 设置启动时间
    identity_.startup_time = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    // 验证必要字段
    if (identity_.service_name.empty() || identity_.instance_id.empty()) {
        return false;
    }
    
    initialized_ = true;
    return true;
}

bool ServiceIdentityManager::initializeFromEnv(const std::string& service_name) {
    ServiceIdentityConfig config;
    config.service_name = service_name;
    return initialize(config);
}

const ServiceIdentity& ServiceIdentityManager::getServiceIdentity() const {
    return identity_;
}

std::string ServiceIdentityManager::getDeviceId() const {
    if (!initialized_) {
        return "unknown-device";
    }
    return identity_.get_device_id();
}

std::string ServiceIdentityManager::getPlatformInfo() const {
    if (!initialized_) {
        return "unknown-platform";
    }
    return identity_.get_platform_info();
}

bool ServiceIdentityManager::isInitialized() const {
    return initialized_;
}

void ServiceIdentityManager::updateServiceInfo(const std::unordered_map<std::string, std::string>& info) {
    for (const auto& [key, value] : info) {
        runtime_info_[key] = value;
    }
}

uint64_t ServiceIdentityManager::getUptimeSeconds() const {
    if (!initialized_) {
        return 0;
    }
    
    auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()).count();
    
    return (now - identity_.startup_time) / 1000;
}

std::string ServiceIdentityManager::generateInstanceId() {
    // 方案: 主机名 + 进程ID + 时间戳 + 随机数
    char hostname[256];
    gethostname(hostname, sizeof(hostname));
    
    pid_t pid = getpid();
    
    auto now = std::chrono::system_clock::now();
    auto timestamp = std::chrono::duration_cast<std::chrono::milliseconds>(
        now.time_since_epoch()).count();
    
    // 生成4位随机数
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(1000, 9999);
    int random_suffix = dis(gen);
    
    // 构建实例ID: {hostname}-{pid}-{timestamp}-{random}
    std::ostringstream oss;
    oss << hostname << "-" << pid << "-" << timestamp << "-" << random_suffix;
    
    return oss.str();
}

std::string ServiceIdentityManager::detectPlatform() {
    #ifdef __linux__
        return "linux-x64";
    #elif _WIN32
        return "windows-x64";
    #elif __APPLE__
        return "macos-x64";
    #else
        return "unknown";
    #endif
}

std::string ServiceIdentityManager::getEnvVar(const std::string& var_name, const std::string& default_value) {
    const char* value = std::getenv(var_name.c_str());
    return value ? std::string(value) : default_value;
}

// 全局便捷函数实现
namespace ServiceId {

std::string getDeviceId() {
    return ServiceIdentityManager::getInstance().getDeviceId();
}

std::string getPlatformInfo() {
    return ServiceIdentityManager::getInstance().getPlatformInfo();
}

std::string getFullIdentity() {
    if (!ServiceIdentityManager::getInstance().isInitialized()) {
        return "uninitialized-service";
    }
    return ServiceIdentityManager::getInstance().getServiceIdentity().get_full_identity();
}

bool isReady() {
    return ServiceIdentityManager::getInstance().isInitialized();
}

} // namespace ServiceId

} // namespace utils
} // namespace im