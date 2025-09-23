#ifndef LOG_MANAGER_HPP
#define LOG_MANAGER_HPP

#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
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

    static void SetLogLevel(spdlog::level::level_enum level, const std::string& logger_name = "");
    static void SetLogLevel(std::string level, const std::string& logger_name = "");

    static void SetLogLevel(spdlog::level::level_enum level,
                            std::vector<std::string>& logger_names);
    static void SetLogLevel(std::string level, std::vector<std::string>& logger_names);

private:
    static std::unordered_map<std::string, std::shared_ptr<spdlog::logger>> s_loggers_;
    static std::unordered_map<std::string, bool> s_loggingEnabled_;
    static std::mutex s_mutex_;
};

}  // namespace utils
}  // namespace im

#endif  // LOG_MANAGER_HPP