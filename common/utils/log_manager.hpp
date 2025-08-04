#ifndef LOG_MANAGER_HPP
#define LOG_MANAGER_HPP

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <boost/format.hpp>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <fmt/core.h> // 引入fmt以支持格式化字符串

template <typename... Args>
using format_string_t = fmt::format_string<Args...>;

class LoggerInterface {
public:
    virtual ~LoggerInterface() = default;
    
    // 非模板方法保留虚函数接口
    virtual void info(const std::string& msg) {}
    virtual void error(const std::string& msg) {}
    virtual void warn(const std::string& msg) {}
    virtual void debug(const std::string& msg) {}
    virtual void trace(const std::string& msg) {}
    
    // 模板方法提供类型安全的格式化支持
    template <typename... Args>
    void info(format_string_t<Args...> fmt, Args&&... args) {
        info(fmt::format(fmt, std::forward<Args>(args)...));
    }
    
    // 其他模板方法类似...
};

class BaseLogger : public LoggerInterface {
public:
    // 继承基类的虚函数实现
    // 无需额外实现，使用LoggerInterface的默认实现
};

class SpdlogLogger : public LoggerInterface {
private:
    std::shared_ptr<spdlog::logger> logger_;
public:
    explicit SpdlogLogger(std::shared_ptr<spdlog::logger> logger) : logger_(std::move(logger)) {}
    
    // 重写虚函数接口
    void info(const std::string& msg) override { logger_->info(msg); }
    void error(const std::string& msg) override { logger_->error(msg); }
    void warn(const std::string& msg) override { logger_->warn(msg); }
    void debug(const std::string& msg) override { logger_->debug(msg); }
    void trace(const std::string& msg) override { logger_->trace(msg); }
};

class LogManager {
public:
    // 设置日志输出到文件
    static void SetLogToFile(const std::string& logger_name, const std::string& filename);
    // 设置日志输出到控制台
    static void SetLogToConsole(const std::string& logger_name);
    // 设置日志开关
    static void SetLoggingEnabled(const std::string& logger_name, bool enabled);
    // 获取 logger
    template <typename T = LoggerInterface>
    static std::shared_ptr<T> GetLogger(const std::string& logger_name);
    // 查询日志是否开启
    static bool IsLoggingEnabled(const std::string& logger_name);


private:
    static std::unordered_map<std::string, std::shared_ptr<LoggerInterface>> s_loggers_;
    static std::unordered_map<std::string, bool> s_loggingEnabled_;
    static std::mutex s_mutex_;
};

// 模板特化实现
template <>
std::shared_ptr<LoggerInterface> LogManager::GetLogger<>(const std::string& logger_name) {
    std::lock_guard<std::mutex> lock(s_mutex_);

    // 检查日志是否开启
    if (!s_loggingEnabled_[logger_name]) {
        return std::make_shared<BaseLogger>();
    }

    auto it = s_loggers_.find(logger_name);
    if (it != s_loggers_.end()) {
        return std::static_pointer_cast<LoggerInterface>(it->second);
    }

    // 默认创建控制台 logger
    auto spd_logger = spdlog::stdout_color_mt(logger_name);
    spd_logger->set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%l] %v");
    auto wrapped_logger = std::make_shared<SpdlogLogger>(spd_logger);
    s_loggers_[logger_name] = std::static_pointer_cast<LoggerInterface>(wrapped_logger);
    s_loggingEnabled_[logger_name] = true;
    return wrapped_logger;
}

#endif  // LOG_MANAGER_HPP