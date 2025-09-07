#ifndef LOG_MANAGER_HPP
#define LOG_MANAGER_HPP

#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <memory>
#include <string>
#include <unordered_map>
#include <mutex>
#include "./singleton.hpp"

namespace im {
namespace utils {

class LogManager : public Singleton<LogManager> {
public:
    // 设置日志输出到文件
    static void SetLogToFile(const std::string& logger_name, const std::string& filename);
    // 设置日志输出到控制台
    static void SetLogToConsole(const std::string& logger_name);
    // 设置日志开关
    static void SetLoggingEnabled(const std::string& logger_name, bool enabled);
    // 获取 logger
    static std::shared_ptr<spdlog::logger> GetLogger(const std::string& logger_name);
    // 查询日志是否开启
    static bool IsLoggingEnabled(const std::string& logger_name);

private:
    static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> s_loggers_;
    static std::unordered_map<std::string, bool> s_loggingEnabled_;
    static std::mutex s_mutex_;
};

} // namespace utils
} // namespace im

#endif // LOG_MANAGER_HPP 