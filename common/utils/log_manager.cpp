#include "log_manager.hpp"

std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> LogManager::s_loggers_;
std::unordered_map<std::string, bool> LogManager::s_loggingEnabled_;
std::mutex LogManager::s_mutex_;

void LogManager::SetLogToFile(const std::string& logger_name, const std::string& filename) {
    std::lock_guard<std::mutex> lock(s_mutex_);
    spdlog::drop(logger_name);  // 先删除旧的 logger，防止重复创建
    auto logger = spdlog::basic_logger_mt(logger_name, filename);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    s_loggers_[logger_name] = logger;
    s_loggingEnabled_[logger_name] = true;
}

void LogManager::SetLogToConsole(const std::string& logger_name) {
    std::lock_guard<std::mutex> lock(s_mutex_);
    spdlog::drop(logger_name);  // 先删除旧的 logger，防止重复创建
    auto logger = spdlog::stdout_color_mt(logger_name);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    s_loggers_[logger_name] = logger;
    s_loggingEnabled_[logger_name] = true;
}

void LogManager::SetLoggingEnabled(const std::string& logger_name, bool enabled) {
    std::lock_guard<std::mutex> lock(s_mutex_);
    s_loggingEnabled_[logger_name] = enabled;
}

std::shared_ptr<spdlog::logger> LogManager::GetLogger(const std::string& logger_name) {
    std::lock_guard<std::mutex> lock(s_mutex_);
    auto it = s_loggers_.find(logger_name);
    if (it != s_loggers_.end()) {
        return it->second;
    }
    // 默认创建控制台 logger
    auto logger = spdlog::stdout_color_mt(logger_name);
    logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    s_loggers_[logger_name] = logger;
    s_loggingEnabled_[logger_name] = true;
    return logger;
}

bool LogManager::IsLoggingEnabled(const std::string& logger_name) {
    std::lock_guard<std::mutex> lock(s_mutex_);
    auto it = s_loggingEnabled_.find(logger_name);
    if (it != s_loggingEnabled_.end()) {
        return it->second;
    }
    return true;
} 