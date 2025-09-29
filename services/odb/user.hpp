#ifndef USER_HPP
#define USER_HPP

/******************************************************************************
 *
 * @file       user.hpp
 * @brief      用户实体类定义
 *
 * @author     myself
 * @date       2025/09/25
 *
 *****************************************************************************/

#include <odb/core.hxx>
#include <odb/nullable.hxx>
#include <string>
#include <cstdint>

// 前向声明枚举类型
#pragma db enum
enum class Gender {
    UNKNOWN = 0,    // 未知
    MALE = 1,       // 男
    FEMALE = 2,     // 女
    OTHER = 3       // 其他
};

// 注册类型枚举
#pragma db enum
enum class RegisterType {
    REGISTER_UNKNOWN = 0, // 未知
    REGISTER_PHONE = 1,   // 手机号注册
    REGISTER_EMAIL = 2    // 邮箱注册
};

// 用户实体类
#pragma db object
class user {
public:
    // 构造函数
    user() : create_time_(0), last_login_(0), online_(false) {}
    
    user(const std::string& uid, 
         const std::string& account, 
         const std::string& nickname,
         const std::string& avatar,
         Gender gender = Gender::UNKNOWN,
         const std::string& signature = "",
         int64_t create_time = 0,
         int64_t last_login = 0,
         bool online = false)
        : uid_(uid), account_(account), nickname_(nickname), avatar_(avatar),
          gender_(gender), signature_(signature), create_time_(create_time),
          last_login_(last_login), online_(online) {}

    // Getter和Setter方法
    const std::string& uid() const { return uid_; }
    void uid(const std::string& uid) { uid_ = uid; }
    
    const std::string& account() const { return account_; }
    void account(const std::string& account) { account_ = account; }
    
    const std::string& nickname() const { return nickname_; }
    void nickname(const std::string& nickname) { nickname_ = nickname; }
    
    const std::string& avatar() const { return avatar_; }
    void avatar(const std::string& avatar) { avatar_ = avatar; }
    
    Gender gender() const { return gender_; }
    void gender(Gender gender) { gender_ = gender; }
    
    const std::string& signature() const { return signature_; }
    void signature(const std::string& signature) { signature_ = signature; }
    
    int64_t create_time() const { return create_time_; }
    void create_time(int64_t create_time) { create_time_ = create_time; }
    
    int64_t last_login() const { return last_login_; }
    void last_login(int64_t last_login) { last_login_ = last_login; }
    
    bool online() const { return online_; }
    void online(bool online) { online_ = online; }
    
    const std::string& phone_number() const { return phone_number_; }
    void phone_number(const std::string& phone_number) { phone_number_ = phone_number; }
    
    const std::string& email() const { return email_; }
    void email(const std::string& email) { email_ = email; }
    
    const std::string& address() const { return address_; }
    void address(const std::string& address) { address_ = address; }
    
    const std::string& birthday() const { return birthday_; }
    void birthday(const std::string& birthday) { birthday_ = birthday; }
    
    const std::string& company() const { return company_; }
    void company(const std::string& company) { company_ = company; }
    
    const std::string& job_title() const { return job_title_; }
    void job_title(const std::string& job_title) { job_title_ = job_title; }
    
    const std::string& wxid() const { return wxid_; }
    void wxid(const std::string& wxid) { wxid_ = wxid; }
    
    const std::string& qqid() const { return qqid_; }
    void qqid(const std::string& qqid) { qqid_ = qqid; }
    
    const std::string& real_name() const { return real_name_; }
    void real_name(const std::string& real_name) { real_name_ = real_name; }
    
    const std::string& extra() const { return extra_; }
    void extra(const std::string& extra) { extra_ = extra; }

private:
    // 数据库字段映射
    #pragma db id auto
    std::string uid_;
    
    #pragma db not_null unique
    std::string account_;
    
    std::string nickname_;
    std::string avatar_;
    
    #pragma db value_not_null
    Gender gender_;
    
    std::string signature_;
    int64_t create_time_;
    int64_t last_login_;
    bool online_;
    
    std::string phone_number_;
    std::string email_;
    std::string address_;
    std::string birthday_;
    std::string company_;
    std::string job_title_;
    std::string wxid_;
    std::string qqid_;
    std::string real_name_;
    std::string extra_;
};

#endif  // USER_HPP
