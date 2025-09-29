#ifndef PGSQL_CONN_HPP
#define PGSQL_CONN_HPP

/******************************************************************************
 *
 * @file       pgsql_conn.hpp
 * @brief      PostgreSQL ODB连接封装，提供RAII和高级数据库操作接口
 *
 * @author     myself
 * @date       2025/09/28
 *
 * 设计目标：
 * 1. RAII自动资源管理 - 构造时获取资源，析构时自动释放
 * 2. 异常安全 - 任何异常都不会导致资源泄露
 * 3. 类型安全 - 编译时检查，避免运行时错误
 * 4. 易用性 - 简洁的API，隐藏复杂的ODB细节
 * 5. 高性能 - 减少不必要的开销和拷贝
 * 
 *****************************************************************************/

#include <odb/core.hxx>
#include <odb/database.hxx>
#include <odb/pgsql/database.hxx>
#include <odb/pgsql/exceptions.hxx>
#include <odb/result.hxx>
#include <odb/session.hxx>
#include <odb/transaction.hxx>

#include <memory>
#include <nlohmann/json.hpp>
#include <stdexcept>
#include <string>
#include "../../utils/log_manager.hpp"
// #include "spdlog/logger.h"


namespace im::db {


// 导入LogManager类型
using im::utils::LogManager;

/**
 * 全局日志记录器声明
 * 
 * 设计决策：
 * 1. 使用extern声明避免ODR（One Definition Rule）违规
 * 2. 在.cpp文件中唯一定义，确保所有编译单元共享同一个logger实例
 * 3. 使用spdlog::logger类型，线程安全且高性能
 * 
 * 注意：
 * - 头文件中只声明，不定义，避免重复定义链接错误
 * - 实际定义在pgsql_conn.cpp中
 */
extern std::shared_ptr<spdlog::logger> logger;

/**
 * @struct PgSqlConfig
 * @brief PostgreSQL数据库配置结构体
 * 
 * 功能：
 * 1. 存储所有PostgreSQL连接相关的配置参数
 * 2. 提供多种配置加载方式（文件、JSON、环境变量）
 * 3. 内置参数验证机制
 * 4. 生成标准的PostgreSQL连接字符串
 * 
 * 设计原则：
 * 1. 合理的默认值 - 开箱即用
 * 2. 类型安全 - 编译时检查
 * 3. 参数验证 - 运行时校验
 * 4. 易扩展 - 方便添加新参数
 */
struct PgSqlConfig {
    // ========== 基础连接参数 ==========
    std::string host = "127.0.0.1";        // 数据库服务器地址（默认本机）
    int port = 5432;                       // 数据库端口（PostgreSQL标准端口）
    std::string database = "mychat";       // 数据库名称（业务相关默认名）
    std::string user = "postgres";         // 数据库用户名（默认管理员用户）
    std::string password = "";             // 数据库密码（默认空，生产环境应设置）
    std::string options = "";              // 额外的连接选项（PostgreSQL特定参数）
    
    // ========== 连接池相关参数 ==========
    int pool_size = 10;                    // 连接池大小（影响并发性能，平衡资源使用）
    int connect_timeout = 30;              // 连接超时时间（秒，防止长时间等待）
    int query_timeout = 30;                // 查询超时时间（秒，防止长时间查询阻塞）

    // ========== SSL安全参数 ==========
    std::string sslmode = "prefer";        // SSL模式：disable(关闭), prefer(优先), require(必须), verify-ca(验证CA), verify-full(完全验证)
    std::string sslcert = "";              // SSL客户端证书文件路径（客户端认证用）
    std::string sslkey = "";               // SSL私钥文件路径（与客户端证书配对）
    std::string sslrootcert = "";          // SSL根证书文件路径（验证服务器证书）

    // ========== 静态工厂方法 ==========
    
    /**
     * @brief 从JSON配置文件加载配置
     * @param config_path 配置文件路径
     * @return PgSqlConfig 配置对象
     * @throws std::runtime_error 当文件不存在或格式错误时抛出异常
     * 
     * 使用场景：
     * - 生产环境从配置文件加载
     * - 支持不同环境的配置文件
     * 
     * 示例配置文件格式：
     * {
     *   "postgresql": {
     *     "host": "127.0.0.1",
     *     "port": 5432,
     *     "database": "mychat",
     *     "user": "postgres",
     *     "password": "password123",
     *     "pool_size": 20,
     *     "sslmode": "require"
     *   }
     * }
     */
    static PgSqlConfig from_file(const std::string& config_path);

    /**
     * @brief 从JSON对象加载配置
     * @param config JSON配置对象
     * @return PgSqlConfig 配置对象
     * @throws std::runtime_error 当配置格式错误时抛出异常
     * 
     * 使用场景：
     * - 从网络API获取配置
     * - 程序内动态生成配置
     * - 配置的二次处理
     * 
     * 实现特点：
     * - 使用value()方法提供默认值
     * - 缺少的配置项会使用结构体默认值
     * - 自动进行配置验证
     */
    static PgSqlConfig from_json(const nlohmann::json& config);

    /**
     * @brief 从环境变量加载配置
     * @return PgSqlConfig 配置对象
     * @throws std::runtime_error 当环境变量格式错误时抛出异常
     * 
     * 环境变量命名约定：
     * - MYCHAT_DB_HOST: 数据库主机
     * - MYCHAT_DB_PORT: 数据库端口
     * - MYCHAT_DB_DATABASE: 数据库名
     * - MYCHAT_DB_USER: 用户名
     * - MYCHAT_DB_PASSWORD: 密码
     * - MYCHAT_DB_POOL_SIZE: 连接池大小
     * - MYCHAT_DB_SSLMODE: SSL模式
     * 
     * 使用场景：
     * - 容器化部署（Docker、Kubernetes）
     * - CI/CD环境
     * - 安全要求高的环境（密码不写入文件）
     */
    static PgSqlConfig from_environment();

    /**
     * @brief 验证配置参数的合理性
     * @throws std::runtime_error 当参数不合理时抛出异常
     * 
     * 验证内容：
     * 1. 必填字段不能为空（host、database、user）
     * 2. 数值字段在合理范围内（port、timeouts、pool_size）
     * 3. 枚举字段在有效值范围内（sslmode）
     * 4. 文件路径有效性（SSL证书文件）
     * 
     * 设计目的：
     * - 早期发现配置错误
     * - 提供清晰的错误信息
     * - 防止运行时错误
     */
    void validate() const;

    /**
     * @brief 生成PostgreSQL连接字符串
     * @return std::string 标准的PostgreSQL连接字符串
     * 
     * 格式示例：
     * "host=127.0.0.1 port=5432 dbname=mychat user=postgres password=xxx sslmode=prefer"
     * 
     * 实现特点：
     * 1. 自动处理可选参数（只有设置了才添加）
     * 2. 正确的参数格式化
     * 3. 默认值优化（prefer SSL模式等不必显式添加）
     * 4. 支持额外选项的追加
     */
    std::string to_connection_string() const;
};



/**
 * @class PgSqlConnection
 * @brief PostgreSQL数据库连接封装类
 * 
 * 核心功能：
 * 1. RAII资源管理 - 自动管理连接生命周期
 * 2. 事务自动化 - 提供RAII事务类
 * 3. 异常安全 - 保证强异常安全性
 * 4. 类型安全 - 模板自动推导，编译时检查
 * 5. 高级API - 简化常用数据库操作
 * 
 * 设计模式：
 * 1. RAII模式 - 资源获取即初始化
 * 2. 代理模式 - 封装ODB数据库对象
 * 3. 模板方法模式 - 统一的操作流程
 * 
 * 使用示例：
 * ```cpp
 * // 创建连接
 * PgSqlConfig config = PgSqlConfig::from_file("config.json");
 * PgSqlConnection conn(config);
 * 
 * // 简单操作
 * User user("john", "john@example.com");
 * auto id = conn.persist(user);
 * 
 * // 事务操作
 * conn.execute_in_transaction([&](auto& c) {
 *     c.persist(user);
 *     c.update(user);
 *     return true;
 * });
 * ```
 */
class PgSqlConnection {
public:
    // ========== 类型定义 ==========
    using DatabasePtr = std::shared_ptr<odb::pgsql::database>;    // 数据库智能指针类型
    using TransactionPtr = std::unique_ptr<odb::transaction>;     // 事务智能指针类型
    using SessionPtr = std::unique_ptr<odb::session>;             // 会话智能指针类型

    // ========== 构造函数 ==========
    
    /**
     * @brief 默认构造函数（从环境变量加载配置）
     * @throws std::runtime_error 连接创建失败或环境变量配置错误
     * 
     * 使用场景：
     * - 容器化环境，配置通过环境变量提供
     * - 简化的使用方式，无需显式配置
     * 
     * 工作流程：
     * 1. 从环境变量加载配置
     * 2. 验证配置有效性
     * 3. 建立数据库连接
     * 4. 测试连接可用性
     * 
     * 注意：需要设置相应的环境变量，否则会使用默认值
     */
    explicit PgSqlConnection();                                // default from env
    
    /**
     * @brief 从配置对象构造连接
     * @param config PostgreSQL配置对象
     * @throws std::runtime_error 连接创建失败或配置无效
     * 
     * 推荐使用方式，提供完全的配置控制
     * 
     * 工作流程：
     * 1. 复制并验证配置对象
     * 2. 建立数据库连接
     * 3. 测试连接可用性
     * 4. 异常时自动清理资源
     */
    explicit PgSqlConnection(const PgSqlConfig& config);       // from PgSqlConfig Object
    
    /**
     * @brief 从配置文件构造连接
     * @param config_path 配置文件路径
     * @throws std::runtime_error 连接创建失败或配置文件错误
     * 
     * 便捷方式，直接从文件加载配置并创建连接
     * 
     * 工作流程：
     * 1. 从文件加载配置
     * 2. 验证配置有效性
     * 3. 建立数据库连接
     * 4. 测试连接可用性
     */
    explicit PgSqlConnection(const std::string& config_path);  // from config file

    /**
     * @brief 从现有数据库对象构造连接
     * @param db 现有的ODB数据库对象
     * @throws std::runtime_error 数据库对象无效或连接测试失败
     * 
     * 高级用法，用于：
     * - 连接池场景
     * - 与现有ODB代码集成
     * - 特殊的连接配置需求
     * 
     * 注意：会移动数据库对象的所有权
     */
    explicit PgSqlConnection(DatabasePtr db);



    // ========== 拷贝/移动语义 ==========
    
    /**
     * 禁止拷贝构造和拷贝赋值
     * 
     * 原因：
     * 1. 数据库连接是昂贵的资源，不应该随意复制
     * 2. 避免意外的连接复制导致的资源浪费
     * 3. 明确连接的所有权语义
     * 4. 防止多个对象管理同一个连接导致的问题
     */
    PgSqlConnection(const PgSqlConnection&) = delete;
    PgSqlConnection& operator=(const PgSqlConnection&) = delete;
    
    /**
     * 允许移动构造和移动赋值
     * 
     * 优势：
     * 1. 支持从函数返回连接对象（返回值优化）
     * 2. 高效的资源转移，避免不必要的拷贝
     * 3. 支持在STL容器中存储连接对象
     * 4. 现代C++的最佳实践
     */
    PgSqlConnection(PgSqlConnection&&) = default;
    PgSqlConnection& operator=(PgSqlConnection&&) = default;

    /**
     * @brief 析构函数
     * 
     * RAII的核心实现：
     * 1. 自动清理数据库连接
     * 2. 清理会话缓存
     * 3. 即使发生异常也能正确清理
     * 4. noexcept保证（析构函数不抛异常）
     * 
     * 设计要点：
     * - 调用cleanup()方法进行资源清理
     * - 异常安全：清理过程中的异常会被捕获和记录
     * - 确保资源不泄露
     */
    ~PgSqlConnection();


    // ========== 连接状态管理 ==========
    
    /**
     * @brief 检查连接是否有效
     * @return bool true表示连接有效，false表示连接不可用
     * 
     * 检查方式：
     * 1. 验证数据库对象是否存在
     * 2. 执行简单查询测试连接（SELECT 1）
     * 3. 捕获所有异常，返回false而不是抛出异常
     * 
     * 使用场景：
     * - 连接池健康检查
     * - 重连逻辑的判断
     * - 防御性编程
     * 
     * 实现特点：
     * - 非侵入式检查（不影响现有事务）
     * - 异常安全（不会抛出异常）
     * - 性能友好（简单查询）
     */
    bool is_valid() const;

    /**
     * @brief 获取原始数据库对象指针
     * @return odb::database* 原始ODB数据库对象指针
     * @throws std::runtime_error 数据库连接不可用
     * 
     * 使用场景：
     * - 需要直接调用ODB API
     * - 与现有ODB代码集成
     * - 高级数据库操作
     * 
     * 注意：
     * - 谨慎使用，绕过了封装的安全检查
     * - 使用前会自动调用check_db()验证连接
     * - 不要手动删除返回的指针
     */
    odb::database* operator->() { return database_.get(); }
    
    /**
     * @brief 获取原始数据库对象引用
     * @return odb::database& 原始ODB数据库对象引用
     * @throws std::runtime_error 数据库连接不可用
     * 
     * 功能同operator->()，但返回引用而非指针
     */
    odb::database& operator*() { return *database_; }

    // ========== 事务管理 ==========
    
    /**
     * @class Transaction
     * @brief RAII事务管理类
     * 
     * 核心特性：
     * 1. 构造时自动开始事务
     * 2. 析构时自动回滚（如果未提交）
     * 3. 异常安全：即使抛异常也不会丢失事务一致性
     * 4. 明确的提交/回滚接口
     * 
     * 使用模式：
     * ```cpp
     * {
     *     auto tx = conn.begin_transaction();
     *     // 数据库操作...
     *     tx.commit();  // 显式提交
     * }  // 如果忘记commit，析构时自动回滚
     * ```
     * 
     * 设计优势：
     * 1. 不会因为忘记commit而丢失数据一致性
     * 2. 异常时自动回滚，保证数据完整性
     * 3. 明确的事务边界，便于调试和理解
     * 4. RAII确保资源正确管理
     */
    class Transaction {
    public:
        /**
         * @brief 构造函数，开始新事务
         * @param conn 数据库连接引用
         * @throws std::runtime_error 事务开始失败
         * 
         * 工作流程：
         * 1. 检查数据库连接有效性
         * 2. 调用ODB开始事务
         * 3. 初始化事务状态标志
         * 4. 记录事务开始日志
         */
        explicit Transaction(PgSqlConnection& conn);
        
        /**
         * @brief 析构函数，自动回滚未提交的事务
         * 
         * RAII的核心实现：
         * 1. 检查事务状态（active_ && !committed_）
         * 2. 如果事务仍然活跃且未提交，自动回滚
         * 3. 异常安全：析构函数不会抛出异常
         * 4. 日志记录：记录自动回滚的情况
         * 
         * 这是RAII的精髓：即使程序员忘记显式处理资源，
         * 系统也能自动保证资源的正确清理。
         * 
         * 注意：自动回滚通常表示程序逻辑有问题，会记录警告日志
         */
        ~Transaction();

        // 禁止拷贝，允许移动（与连接类相同的语义）
        Transaction(const Transaction&) = delete;
        Transaction& operator=(const Transaction&) = delete;
        Transaction(Transaction&&) = default;
        Transaction& operator=(Transaction&&) = default;

        /**
         * @brief 提交事务
         * @throws std::runtime_error 提交失败或事务已结束
         * 
         * 工作流程：
         * 1. 检查事务状态（必须活跃且有效）
         * 2. 调用ODB提交事务
         * 3. 更新事务状态标志
         * 4. 记录提交成功日志
         * 
         * 调用后事务状态变为已提交，析构时不会回滚
         */
        void commit();
        
        /**
         * @brief 回滚事务
         * @throws std::runtime_error 回滚失败或事务已结束
         * 
         * 工作流程：
         * 1. 检查事务状态（必须活跃且有效）
         * 2. 调用ODB回滚事务
         * 3. 更新事务状态标志
         * 4. 记录回滚日志
         * 
         * 显式回滚，调用后事务状态变为已结束
         */
        void rollback();

        /**
         * @brief 检查事务是否仍然活跃
         * @return bool true表示事务活跃，false表示已结束
         * 
         * 活跃状态：事务已开始且未提交也未回滚
         */
        bool is_valid() const;

    private:
        TransactionPtr transaction_;     // ODB事务对象智能指针
        bool committed_ = false;         // 是否已提交标志
        bool active_ = true;            // 事务是否活跃标志
    };

    /**
     * @brief 创建新事务
     * @return Transaction RAII事务对象
     * @throws std::runtime_error 事务创建失败
     * 
     * 使用示例：
     * ```cpp
     * auto tx = conn.begin_transaction();
     * conn.persist(user);
     * tx.commit();
     * ```
     * 
     * 返回值优化：现代编译器会优化返回值，避免不必要的拷贝
     */
    Transaction begin_transaction();

    /**
     * @brief 在事务中执行操作（模板方法）
     * @tparam Func 操作函数类型
     * @param func 要执行的操作，签名：auto func(PgSqlConnection& conn) -> ReturnType
     * @return auto 操作函数的返回值
     * @throws 传播操作函数抛出的异常（事务会自动回滚）
     * 
     * 这是最推荐的事务使用方式：
     * 1. 自动开始事务
     * 2. 执行用户操作
     * 3. 成功时自动提交
     * 4. 异常时自动回滚（通过RAII）
     * 
     * 使用示例：
     * ```cpp
     * auto result = conn.execute_in_transaction([&](auto& c) {
     *     c.persist(user);
     *     c.update(profile);
     *     return user.id();
     * });
     * ```
     * 
     * 优势：
     * 1. 代码简洁，无需手动管理事务
     * 2. 异常安全，自动回滚
     * 3. 支持返回值
     * 4. 完美转发，性能优异
     * 
     * 实现特点：
     * - 使用完美转发（Func&& func）
     * - 自动类型推导返回值
     * - 异常传播（不吞噬异常）
     * - RAII保证事务正确处理
     */
    template <typename Func>
    auto execute_in_transaction(Func&& func) -> decltype(func(*this)) {
        auto tx = begin_transaction();  // RAII事务对象
        try {
            // 执行用户操作
            auto ret = func(*this);
            // 操作成功，提交事务
            tx.commit();
            return ret;
        } catch (const odb::database_exception& e) {
            // 记录数据库异常，事务会在tx析构时自动回滚
            logger->error("数据库事务执行异常: {}", e.what());
            throw;  // 重新抛出异常，让调用者处理
        }
        // 注意：其他类型的异常也会导致事务回滚（通过RAII）
    }

    // ========== 数据库操作（CRUD） ==========
    
    /**
     * @brief 持久化对象到数据库（CREATE）
     * @tparam T 对象类型，必须是ODB持久化类
     * @param obj 要持久化的对象引用
     * @return typename odb::object_traits<T>::id_type 对象的主键值
     * @throws std::runtime_error 持久化失败或对象已存在
     * 
     * 功能：
     * 1. 将C++对象保存到数据库
     * 2. 自动分配主键（如果是auto类型）
     * 3. 返回分配的主键值
     * 4. 统一的异常处理和日志记录
     * 
     * 使用示例：
     * ```cpp
     * User user("john", "john@example.com");
     * auto user_id = conn.persist(user);
     * std::cout << "创建用户，ID: " << user_id << std::endl;
     * ```
     * 
     * 注意：
     * 1. 对象不能已经在数据库中存在
     * 2. 如果主键是auto类型，对象的ID会被更新
     * 3. 操作需要在事务中进行
     * 4. 会自动检查数据库连接有效性
     */
    template <typename T>
    typename odb::object_traits<T>::id_type persist(T& obj) {
        check_db();  // 检查数据库连接有效性
        try {
            return database_->persist(obj);
        } catch (const odb::object_already_persistent& e) {
            // 对象已存在异常，提供清晰的错误信息
            logger->error("对象已经存在于数据库中: {}", e.what());
            throw std::runtime_error("对象已经存在于数据库中: " + std::string(e.what()));
        } catch (const odb::database_exception& e) {
            // 其他数据库异常
            logger->error("数据库操作失败: {}", e.what());
            throw std::runtime_error("数据库操作失败: " + std::string(e.what()));
        }
    }

    template <typename T>
    void update(const T& obj) {
        check_db();
        try {
            database_->update(obj);
        } catch (const odb::object_not_persistent& e) {
            logger->error("对象不存在于数据库中，ID: {}", obj.id());
            throw std::runtime_error("对象不存在于数据库中: " + std::string(e.what()));
        } catch (const odb::database_exception& e) {
            logger->error("数据库更新异常: {}", e.what());
            throw std::runtime_error("数据库更新失败: " + std::string(e.what()));
        }
    }

    template <typename T>
    void erase(const T& obj) {
        check_db();
        try {
            database_->erase(obj);
        } catch (const odb::object_not_persistent& e) {
            logger->error("对象不存在于数据库中，ID: {}", obj.id());
            throw std::runtime_error("对象不存在于数据库中: " + std::string(e.what()));
        } catch (const odb::database_exception& e) {
            logger->error("数据库删除异常: {}", e.what());
            throw std::runtime_error("数据库删除失败: " + std::string(e.what()));
        }
    }


    /**
     * 根据ID删除对象
     */
    template <typename T>
    void erase(const typename odb::object_traits<T>::id_type& id) {
        check_db();
        try {
            database_->erase<T>(id);
        } catch (const odb::object_not_persistent& e) {
            logger->error("对象不存在于数据库中，ID: {}", id);
            throw std::runtime_error("对象不存在于数据库中，ID: " + std::to_string(id));
        } catch (const odb::database_exception& e) {
            logger->error("数据库删除异常: {}", e.what());
            throw std::runtime_error("数据库删除失败: " + std::string(e.what()));
        }
    }

    template <typename T>
    std::shared_ptr<T> load(const typename odb::object_traits<T>::id_type& id) {
        check_db();
        try {
            return database_->load<T>(id);
        } catch (const odb::object_not_persistent& e) {
            logger->error("对象不存在于数据库中，ID: {}", id);
            throw std::runtime_error("对象不存在，ID: " + std::to_string(id));
        } catch (const odb::database_exception& e) {
            logger->error("数据库加载异常: {}", e.what());
            throw std::runtime_error("数据库加载失败: " + std::string(e.what()));
        }
    }

    template <typename T>
    std::shared_ptr<T> find(const typename odb::object_traits<T>::id_type& id) {
        check_db();
        try {
            return database_->find<T>(id);
        } catch (const odb::database_exception& e) {
            logger->error("数据库查找异常: {}", e.what());
            throw std::runtime_error("数据库查找失败: " + std::string(e.what()));
        }
    }

    template <typename T>
    odb::result<T> query(const odb::query<T>& q = odb::query<T>()) {
        check_db();

        try {
            return database_->query<T>(q);
        } catch (const odb::database_exception& e) {
            logger->error("数据库查询异常: {}", e.what());
            throw std::runtime_error("数据库查询失败: " + std::string(e.what()));
        }
    }

    template <typename T>
    std::shared_ptr<T> query_one(const odb::query<T>& q) {
        auto result = query<T>(q);
        auto it = result.begin();
        if (it != result.end()) {
            auto obj = std::make_shared<T>(*it);
            ++it;
            if (it != result.end()) {
                logger->error("Query returned more than one result");
                throw std::runtime_error("Query returned more than one result");
            }
            return obj;
        }
        return nullptr;
    }

    void execute_sql(const std::string& sql);

    void create_schema(bool drop_existing = false);

    void drop_schema();

    void enable_session_cache();

    void disable_session_cache();

    bool is_session_cache_enabled() const;

private:
    DatabasePtr database_;
    SessionPtr session_;
    PgSqlConfig config_;

    void setup_connection();
    void test_connection();
    void cleanup();
    void check_db() {
        if (!database_) {
            logger->error("数据库连接不可用");
            throw std::runtime_error("Database connection is not available");
        }
    }
};


}  // namespace im::db

#endif  // PGSQL_CONN_HPP