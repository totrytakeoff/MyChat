#pragma once

#ifndef TIME_UTILS_HPP
#define TIME_UTILS_HPP

/******************************************************************************
 *
 * @file       time_utils.hpp
 * @brief      时间工具
 *
 * @author     myself
 * @date       2025/07/14
 * 
 *****************************************************************************/



#include <chrono>
#include <ctime>
#include <iomanip>
#include <sstream>

namespace utils {
namespace TimeUtils {
// 获取当前时间戳（毫秒）
static uint64_t now_ms() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
                   std::chrono::system_clock::now().time_since_epoch())
            .count();
}

// 格式化时间字符串
static std::string format_time(uint64_t ms) {
    time_t t = ms / 1000;
    tm* lt = localtime(&t);
    char buffer[20];
    strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", lt);
    return std::string(buffer);
}

}  // namespace TimeUtils
}  // namespace utils


#endif // TIME_UTILS_HPP