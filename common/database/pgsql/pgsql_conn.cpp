/******************************************************************************
 *
 * @file       pgsql_conn.cpp
 * @brief      PostgreSQL ODB连接封装的实现文件
 *
 * @author     myself
 * @date       2025/09/28
 *
 * 实现说明：
 * 1. 配置管理的完整实现 - 支持文件、JSON、环境变量加载
 * 2. RAII连接管理 - 自动资源获取和释放
 * 3. 事务自动化 - 异常安全的事务处理
 * 4. 错误处理 - 统一的异常转换和日志记录
 * 5. 会话缓存 - 可选的ODB会话管理
 *
 *****************************************************************************/

#include <exception>
#include <fstream>
#include <iostream>
#include <memory>
#include <nlohmann/json.hpp>

#include <odb/schema-catalog.hxx>
#include "pgsql_conn.hpp"

namespace im::db {

// JSON类型别名，简化代码
using json = nlohmann::json;

/**
 * 全局日志记录器定义
 * 
 * 实现说明：
 * 1. 这是头文件中extern声明的唯一定义
 * 2. 确保所有编译单元共享同一个logger实例
 * 3. 避免ODR（One Definition Rule）违规
 * 4. 使用"pgsql"作为logger名称，便于日志过滤
 */
auto logger = LogManager::GetLogger("pgsql");


// ==================== PgSqlConfig 实现 ====================

/**
 * @brief 从配置文件加载配置
 * 
 * 实现流程：
 * 1. 打开配置文件，检查文件是否存在
 * 2. 解析JSON格式，处理格式错误
 * 3. 委托给from_json方法进行配置解析
 * 4. 确保文件正确关闭
 * 
 * 错误处理：
 * - 文件不存在：抛出runtime_error异常
 * - JSON格式错误：抛出runtime_error异常，包含详细错误信息
 * - 记录错误日志，便于调试
 */
PgSqlConfig im::db::PgSqlConfig::from_file(const std::string& config_path) {
    std::ifstream file(config_path);

    // 确保文件已打开
    if (!file.is_open()) {
        logger->error("无法打开配置文件: {}", config_path);
        throw std::runtime_error("Failed to open config file: " + config_path);
    }

    json config_json;

    // 读取配置文件，处理JSON解析异常
    try {
        file >> config_json;
    } catch (const nlohmann::json::parse_error& e) {
        // 确保文件被关闭，即使解析失败
        file.close();
        throw std::runtime_error("配置文件JSON格式错误: " + std::string(e.what()));
    }

    file.close();
    // 委托给from_json方法，复用配置解析逻辑
    return from_json(config_json);
}

PgSqlConfig im::db::PgSqlConfig::from_json(const nlohmann::json& config) {
    PgSqlConfig config_;

    try {
        // 检查是否有postgresql配置段
        if (!config.contains("postgresql")) {
            throw std::runtime_error("配置文件中缺少postgresql配置段");
        }

        const auto& pg_config = config["postgresql"];

        // 逐个读取配置项，使用value()方法提供默认值
        config_.host = pg_config.value("host", config_.host);
        config_.port = pg_config.value("port", config_.port);
        config_.database = pg_config.value("database", config_.database);
        config_.user = pg_config.value("user", config_.user);
        config_.password = pg_config.value("password", config_.password);

        config_.pool_size = pg_config.value("pool_size", config_.pool_size);
        config_.connect_timeout = pg_config.value("connect_timeout", config_.connect_timeout);
        config_.query_timeout = pg_config.value("query_timeout", config_.query_timeout);

        config_.sslmode = pg_config.value("sslmode", config_.sslmode);
        config_.sslcert = pg_config.value("sslcert", config_.sslcert);
        config_.sslkey = pg_config.value("sslkey", config_.sslkey);
        config_.sslrootcert = pg_config.value("sslrootcert", config_.sslrootcert);

        config_.options = pg_config.value("options", config_.options);

        // 验证配置合理性
        config_.validate();
        return config_;
    } catch (const std::exception& e) {
        logger->error("配置文件内容错误: {}", e.what());
        throw std::runtime_error("配置文件内容错误: " + std::string(e.what()));
        return config_;
    }
}


PgSqlConfig PgSqlConfig::from_environment() {
    PgSqlConfig config;  // 默认配置

    // 定义环境变量映射
    // 约定：MYCHAT_DB_前缀 + 大写参数名
    if (const char* env_host = std::getenv("MYCHAT_DB_HOST")) {
        config.host = env_host;
    }

    if (const char* env_port = std::getenv("MYCHAT_DB_PORT")) {
        try {
            config.port = std::stoi(env_port);
        } catch (const std::exception& e) {
            throw std::runtime_error("环境变量MYCHAT_DB_PORT不是有效的数字: " +
                                     std::string(env_port));
        }
    }

    if (const char* env_database = std::getenv("MYCHAT_DB_DATABASE")) {
        config.database = env_database;
    }

    if (const char* env_user = std::getenv("MYCHAT_DB_USER")) {
        config.user = env_user;
    }

    if (const char* env_password = std::getenv("MYCHAT_DB_PASSWORD")) {
        config.password = env_password;
    }

    if (const char* env_pool_size = std::getenv("MYCHAT_DB_POOL_SIZE")) {
        try {
            config.pool_size = std::stoi(env_pool_size);
        } catch (const std::exception& e) {
            throw std::runtime_error("环境变量MYCHAT_DB_POOL_SIZE不是有效的数字: " +
                                     std::string(env_pool_size));
        }
    }

    // SSL相关环境变量
    if (const char* env_sslmode = std::getenv("MYCHAT_DB_SSLMODE")) {
        config.sslmode = env_sslmode;
    }

    // 验证配置
    config.validate();

    return config;
}

std::string PgSqlConfig::to_connection_string() const {
    std::string conn_str;

    // 必需参数
    conn_str += "host=" + host;
    conn_str += " port=" + std::to_string(port);
    conn_str += " dbname=" + database;
    conn_str += " user=" + user;

    // 密码（如果有）
    if (!password.empty()) {
        conn_str += " password=" + password;
    }

    // SSL模式
    if (!sslmode.empty() && sslmode != "prefer") {  // prefer是默认值，可以不写
        conn_str += " sslmode=" + sslmode;
    }

    // SSL证书相关
    if (!sslcert.empty()) {
        conn_str += " sslcert=" + sslcert;
    }

    if (!sslkey.empty()) {
        conn_str += " sslkey=" + sslkey;
    }

    if (!sslrootcert.empty()) {
        conn_str += " sslrootcert=" + sslrootcert;
    }

    // 连接超时
    if (connect_timeout != 30) {  // 30是默认值
        conn_str += " connect_timeout=" + std::to_string(connect_timeout);
    }

    // 额外选项
    if (!options.empty()) {
        conn_str += " " + options;
    }

    return conn_str;
}
void PgSqlConfig::validate() const {
    // 验证必填字段
    if (host.empty()) {
        throw std::runtime_error("host不能为空");
    }

    if (database.empty()) {
        throw std::runtime_error("database不能为空");
    }

    if (user.empty()) {
        throw std::runtime_error("user不能为空");
    }

    // 验证端口范围
    if (port <= 0 || port > 65535) {
        throw std::runtime_error("port必须在1-65535范围内，当前值: " + std::to_string(port));
    }

    // 验证连接池大小
    if (pool_size <= 0 || pool_size > 1000) {
        throw std::runtime_error("pool_size必须在1-1000范围内，当前值: " +
                                 std::to_string(pool_size));
    }

    // 验证超时时间
    if (connect_timeout <= 0 || connect_timeout > 3600) {
        throw std::runtime_error("connect_timeout必须在1-3600秒范围内，当前值: " +
                                 std::to_string(connect_timeout));
    }

    if (query_timeout <= 0 || query_timeout > 3600) {
        throw std::runtime_error("query_timeout必须在1-3600秒范围内，当前值: " +
                                 std::to_string(query_timeout));
    }

    // 验证SSL模式
    if (sslmode != "disable" && sslmode != "prefer" && sslmode != "require" &&
        sslmode != "verify-ca" && sslmode != "verify-full") {
        throw std::runtime_error("无效的sslmode: " + sslmode +
                                 "，有效值: disable, prefer, require, verify-ca, verify-full");
    }

    // 验证SSL证书文件（如果指定了路径）
    if (!sslcert.empty()) {
        std::ifstream cert_file(sslcert);
        if (!cert_file.is_open()) {
            throw std::runtime_error("SSL证书文件不存在: " + sslcert);
        }
    }

    if (!sslkey.empty()) {
        std::ifstream key_file(sslkey);
        if (!key_file.is_open()) {
            throw std::runtime_error("SSL私钥文件不存在: " + sslkey);
        }
    }

    if (!sslrootcert.empty()) {
        std::ifstream root_cert_file(sslrootcert);
        if (!root_cert_file.is_open()) {
            throw std::runtime_error("SSL根证书文件不存在: " + sslrootcert);
        }
    }
}

PgSqlConnection::PgSqlConnection() {
    try {
        config_ = PgSqlConfig::from_environment();

        setup_connection();

        test_connection();
    } catch (const std::exception& e) {
        logger->error("从环境变量初始化PostgreSQL数据库连接失败: {}", e.what());
        throw;
    }
}

PgSqlConnection::PgSqlConnection(const PgSqlConfig& config) : config_(config) {
    // 创建数据库连接
    try {
        setup_connection();

        test_connection();
    } catch (const std::exception& e) {
        logger->error("创建PostgreSQL数据库连接失败: {}", e.what());
        // 构造失败时，确保资源被清理
        cleanup();
        throw std::runtime_error("PostgreSQL连接创建失败: " + std::string(e.what()));
    }
}

PgSqlConnection::PgSqlConnection(const std::string& config_path) {
    try {
        config_ = PgSqlConfig::from_file(config_path);

        setup_connection();

        test_connection();
    } catch (const std::exception& e) {
        logger->error("从配置文件初始化PostgreSQL数据库连接失败: {}", e.what());
        throw;
    }
}

PgSqlConnection::PgSqlConnection(DatabasePtr db) : database_(std::move(db)) {
    if (!database_) {
        logger->error("数据库连接对象为空");
        throw std::runtime_error("数据库连接对象为空");
    }
    try {
        test_connection();
    } catch (const std::exception& e) {
        logger->error("从数据库连接对象初始化PostgreSQL数据库连接失败: {}", e.what());
        throw;
    }
}

// RAII
PgSqlConnection::~PgSqlConnection() { cleanup(); }


// 检查连接是否有效
bool PgSqlConnection::is_valid() const {
    if (!database_) {
        return false;
    }
    try {
        auto t = database_->begin();
        database_->execute("SELECT 1");
        t->commit();
        return true;
    } catch (std::exception& e) {
        logger->error("检查PostgreSQL数据库连接失败: {}", e.what());
        return false;
    }
}

PgSqlConnection::Transaction::Transaction(PgSqlConnection& conn) {
    if (!conn.database_) {
        throw std::runtime_error("数据库连接不可用，无法开始事务");
    }
    try {
        transaction_ = std::make_unique<odb::transaction>(conn.database_->begin());
    } catch (const std::exception& e) {
        logger->error("开始事务失败: {}", e.what());
        throw std::runtime_error("开始事务失败: " + std::string(e.what()));
    }
}

/**
 * @brief 事务析构函数 - RAII自动回滚实现
 * 
 * 这是RAII的核心：即使忘记调用commit()，也不会丢失数据一致性
 * 
 * 实现逻辑：
 * 1. 检查事务状态：必须是活跃且未提交
 * 2. 自动回滚事务，保证数据一致性
 * 3. 记录回滚警告，提醒开发者可能存在的问题
 * 4. 异常安全：析构函数不会抛出异常
 * 
 * 设计哲学：
 * - "宁可回滚，不可丢失数据一致性"
 * - 自动回滚通常表示程序逻辑有问题，需要注意
 * - 提供明确的日志信息，便于调试
 */
PgSqlConnection::Transaction::~Transaction() {
    if (active_ && !committed_) {  // 修复：正确的逻辑 - 活跃且未提交时才回滚
        try {
            if (transaction_) {
                transaction_->rollback();
                logger->warn("事务未提交，已自动回滚 - 可能存在程序逻辑问题");
            } else {
                logger->warn("事务对象失效，已自动回滚");
            }
        } catch (const std::exception& e) {
            // 析构函数不能抛异常，只能记录错误
            logger->error("自动回滚事务失败: {}", e.what());
        }
    }
}

void PgSqlConnection::Transaction::commit() {
    if (!active_) {
        logger->error("事务已结束，无法提交");
        throw std::runtime_error("事务已结束，无法提交");
    }
    if (!transaction_) {
        logger->error("事务对象无效，无法提交");
        throw std::runtime_error("事务对象无效，无法提交");
    }

    try {
        transaction_->commit();

        committed_ = true;
        active_ = false;

    } catch (const std::exception& e) {
        active_ = false;
        logger->error("提交事务失败: {}", e.what());
        throw std::runtime_error("提交事务失败: " + std::string(e.what()));
    }
}

void PgSqlConnection::Transaction::rollback() {
    if (!active_) {
        logger->error("事务已结束，无法回滚");
        throw std::runtime_error("事务已结束，无法回滚");
    }
    if (!transaction_) {
        logger->error("事务对象无效，无法回滚");
        throw std::runtime_error("事务对象无效，无法回滚");
    }

    try {
        transaction_->rollback();
        active_ = false;
    } catch (const std::exception& e) {
        active_ = false;
        logger->error("回滚事务失败: {}", e.what());
        throw std::runtime_error("回滚事务失败: " + std::string(e.what()));
    }
}

bool PgSqlConnection::Transaction::is_valid() const { return active_; }


PgSqlConnection::Transaction PgSqlConnection::begin_transaction() { return Transaction(*this); }


void PgSqlConnection::execute_sql(const std::string& sql) {
    check_db();
    try {
        database_->execute(sql);
    } catch (const odb::database_exception& e) {
        logger->error("执行SQL语句失败: {} , SQL:{}", e.what(), sql);
        throw std::runtime_error("执行SQL语句失败: " + std::string(e.what()));
    }
}


// 创建模式，如果drop_existing为true，则先删除已有模式
void PgSqlConnection::create_schema(bool drop_existing) {
    check_db();
    auto tx = begin_transaction();
    try {
        if (drop_existing) {
            odb::schema_catalog::drop_schema(*database_);
        }
        odb::schema_catalog::create_schema(*database_);
        tx.commit();

    } catch (const odb::database_exception& e) {
        logger->error("模式创建失败: {}", e.what());
        throw std::runtime_error("模式创建失败: " + std::string(e.what()));
    }
}

void PgSqlConnection::drop_schema() {
    check_db();

    auto tx = begin_transaction();
    try {
        odb::schema_catalog::drop_schema(*database_);
        tx.commit();
    } catch (const odb::database_exception& e) {
        logger->error("模式删除失败: {}", e.what());
        throw std::runtime_error("模式删除失败: " + std::string(e.what()));
    }
}

// ========== 会话缓存管理 ==========

/**
 * 启用会话缓存
 *
 * 优势：
 * 1. 避免重复加载相同对象
 * 2. 维护对象身份唯一性
 * 3. 支持延迟加载
 */

void PgSqlConnection::enable_session_cache() { session_ = std::make_unique<odb::session>(); }

void PgSqlConnection::disable_session_cache() { session_.reset(); }

bool PgSqlConnection::is_session_cache_enabled() const { return session_ != nullptr; }

/**
 * @brief 清理数据库资源
 * 
 * RAII资源清理的核心实现：
 * 1. 按正确顺序清理资源（先会话，后连接）
 * 2. 异常安全：清理过程中的异常不会传播
 * 3. 完整的日志记录，便于调试和监控
 * 
 * 清理顺序说明：
 * - 先清理session_：ODB会话依赖于数据库连接
 * - 后清理database_：数据库连接是底层资源
 * - 智能指针自动调用析构函数完成实际清理
 * 
 * 异常处理：
 * - 清理过程中可能发生异常（如网络断开）
 * - 不允许异常传播到析构函数外部
 * - 记录详细错误信息，便于故障排查
 */
void PgSqlConnection::cleanup() {
    try {
        // 先清理会话缓存
        if (session_) {
            session_.reset();
            logger->debug("会话缓存已清理");
        }

        // 再清理数据库连接
        if (database_) {
            database_.reset();
            logger->debug("数据库连接已关闭");
        }
        
    } catch (const std::exception& e) {
        // 清理过程中的异常不应该传播
        // 特别是在析构函数中调用时，不能抛出异常
        logger->error("清理数据库连接失败: {}", e.what());
    }
}

void PgSqlConnection::setup_connection() {
    // 创建数据库连接
    database_ = std::make_unique<odb::pgsql::database>(config_.to_connection_string());
}

void PgSqlConnection::test_connection() {
    check_db();

    try {
        auto tx = database_->begin();
        database_->execute("SELECT 1");
        tx->commit();
        logger->info("成功连接到PostgreSQL数据库 {}:{}", config_.host, config_.port);
    } catch (const std::exception& e) {
        logger->error("测试数据库连接失败: {}", e.what());
        throw std::runtime_error("测试数据库连接失败: " + std::string(e.what()));
    }
}


}  // namespace im::db
