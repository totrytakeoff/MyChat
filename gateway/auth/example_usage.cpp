/******************************************************************************
 *
 * @file       example_usage.cpp
 * @brief      多平台认证系统使用示例
 *
 * @author     myself
 * @date       2025/08/12
 *
 *****************************************************************************/

#include <iostream>
#include <string>
#include "multi_platform_auth.hpp"
#include "../../common/database/redis/redis_mgr.hpp"
#include "../../common/utils/log_manager.hpp"

using namespace im::gateway;
using namespace im::db;

void print_separator(const std::string& title) {
    std::cout << "\n" << std::string(50, '=') << std::endl;
    std::cout << title << std::endl;
    std::cout << std::string(50, '=') << std::endl;
}

void print_user_info(const UserTokenInfo& user_info) {
    std::cout << "User ID: " << user_info.user_id << std::endl;
    std::cout << "Username: " << user_info.username << std::endl;
    std::cout << "Device ID: " << user_info.device_id << std::endl;
    std::cout << "Platform: " << user_info.platform << std::endl;
    
    auto create_time_t = std::chrono::system_clock::to_time_t(user_info.create_time);
    auto expire_time_t = std::chrono::system_clock::to_time_t(user_info.expire_time);
    
    std::cout << "Created: " << std::ctime(&create_time_t);
    std::cout << "Expires: " << std::ctime(&expire_time_t);
}

int main() {
    try {
        print_separator("多平台认证系统示例");
        
        // 初始化日志系统
        im::utils::LogManager::SetLogToConsole("auth_mgr");
        
        // 初始化Redis
        std::cout << "初始化Redis连接..." << std::endl;
        bool redis_init = RedisManager::GetInstance().initialize("../common/database/config.json");
        if (!redis_init) {
            std::cerr << "Failed to initialize Redis!" << std::endl;
            return -1;
        }
        std::cout << "Redis连接成功!" << std::endl;
        
        // 创建认证管理器
        std::cout << "创建认证管理器..." << std::endl;
        MultiPlatformAuthManager auth_manager(
            "example_secret_key_12345678901234567890", 
            "platform_token_config.json"
        );
        std::cout << "认证管理器创建成功!" << std::endl;
        
        // 示例用户信息
        std::string user_id = "user_12345";
        std::string username = "john_doe";
        std::string device_id = "device_abcdef123456";
        std::string platform = "web";
        
        print_separator("1. 生成Token对");
        std::cout << "为用户生成tokens..." << std::endl;
        std::cout << "User ID: " << user_id << std::endl;
        std::cout << "Username: " << username << std::endl;
        std::cout << "Device ID: " << device_id << std::endl;
        std::cout << "Platform: " << platform << std::endl;
        
        TokenResult token_result = auth_manager.generate_tokens(user_id, username, device_id, platform);
        
        if (!token_result.success) {
            std::cerr << "Token生成失败: " << token_result.error_message << std::endl;
            return -1;
        }
        
        std::cout << "\n✅ Token生成成功!" << std::endl;
        std::cout << "Access Token: " << token_result.new_access_token.substr(0, 50) << "..." << std::endl;
        std::cout << "Refresh Token: " << token_result.new_refresh_token.substr(0, 30) << "..." << std::endl;
        
        print_separator("2. 验证Access Token");
        UserTokenInfo user_info;
        bool access_verify = auth_manager.verify_access_token(token_result.new_access_token, user_info);
        
        if (access_verify) {
            std::cout << "✅ Access Token验证成功!" << std::endl;
            print_user_info(user_info);
        } else {
            std::cout << "❌ Access Token验证失败!" << std::endl;
        }
        
        print_separator("3. 验证Refresh Token");
        std::string test_device_id = device_id;
        bool refresh_verify = auth_manager.verify_refresh_token(
            token_result.new_refresh_token, test_device_id, user_info);
        
        if (refresh_verify) {
            std::cout << "✅ Refresh Token验证成功!" << std::endl;
            print_user_info(user_info);
        } else {
            std::cout << "❌ Refresh Token验证失败!" << std::endl;
        }
        
        print_separator("4. 刷新Access Token");
        std::string refresh_device_id = device_id;
        TokenResult refresh_result = auth_manager.refresh_access_token(
            token_result.new_refresh_token, refresh_device_id);
        
        if (refresh_result.success) {
            std::cout << "✅ Access Token刷新成功!" << std::endl;
            std::cout << "新的Access Token: " << refresh_result.new_access_token.substr(0, 50) << "..." << std::endl;
            if (!refresh_result.new_refresh_token.empty()) {
                std::cout << "新的Refresh Token: " << refresh_result.new_refresh_token.substr(0, 30) << "..." << std::endl;
            }
        } else {
            std::cout << "❌ Access Token刷新失败: " << refresh_result.error_message << std::endl;
        }
        
        print_separator("5. Token撤销测试");
        
        // 撤销access token
        std::cout << "撤销Access Token..." << std::endl;
        bool revoke_success = auth_manager.revoke_token(token_result.new_access_token);
        std::cout << (revoke_success ? "✅" : "❌") << " Access Token撤销" 
                  << (revoke_success ? "成功" : "失败") << std::endl;
        
        // 检查撤销状态
        bool is_revoked = auth_manager.is_token_revoked(token_result.new_access_token);
        std::cout << "Token撤销状态: " << (is_revoked ? "已撤销" : "未撤销") << std::endl;
        
        // 尝试验证被撤销的token
        bool verify_revoked = auth_manager.verify_access_token(token_result.new_access_token, user_info);
        std::cout << "验证被撤销的Token: " << (verify_revoked ? "成功(异常!)" : "失败(正常)") << std::endl;
        
        print_separator("6. 多平台测试");
        std::vector<std::string> platforms = {"web", "android", "ios", "desktop", "miniapp"};
        
        for (const auto& test_platform : platforms) {
            std::cout << "\n测试平台: " << test_platform << std::endl;
            TokenResult platform_result = auth_manager.generate_tokens(
                user_id + "_" + test_platform, username, device_id, test_platform);
            
            if (platform_result.success) {
                std::cout << "  ✅ " << test_platform << " tokens生成成功" << std::endl;
                
                // 验证平台特定的token
                UserTokenInfo platform_info;
                bool platform_verify = auth_manager.verify_access_token(
                    platform_result.new_access_token, platform_info);
                
                if (platform_verify && platform_info.platform == test_platform) {
                    std::cout << "  ✅ " << test_platform << " token验证成功" << std::endl;
                } else {
                    std::cout << "  ❌ " << test_platform << " token验证失败" << std::endl;
                }
            } else {
                std::cout << "  ❌ " << test_platform << " tokens生成失败: " 
                          << platform_result.error_message << std::endl;
            }
        }
        
        print_separator("7. 清理");
        std::cout << "删除Refresh Token..." << std::endl;
        bool delete_success = auth_manager.del_refresh_token(token_result.new_refresh_token);
        std::cout << (delete_success ? "✅" : "❌") << " Refresh Token删除" 
                  << (delete_success ? "成功" : "失败") << std::endl;
        
        // 关闭Redis连接
        RedisManager::GetInstance().shutdown();
        std::cout << "Redis连接已关闭" << std::endl;
        
        print_separator("示例完成");
        std::cout << "🎉 多平台认证系统示例运行完成!" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "程序异常: " << e.what() << std::endl;
        return -1;
    } catch (...) {
        std::cerr << "未知异常" << std::endl;
        return -1;
    }
    
    return 0;
}