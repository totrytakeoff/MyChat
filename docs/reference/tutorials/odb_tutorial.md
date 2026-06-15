# ODB 使用教程

## 目录
1. [ODB 简介](#odb-简介)
2. [环境配置](#环境配置)
3. [实体类定义](#实体类定义)
4. [数据库操作](#数据库操作)
5. [高级特性](#高级特性)
6. [最佳实践](#最佳实践)
7. [常见问题](#常见问题)

## ODB 简介

### 什么是 ODB？

ODB 是一个开源、跨平台的 C++ 对象关系映射（ORM）系统。它允许您以自然、直观的方式在 C++ 中持久化对象，无需编写繁琐的 SQL 语句。ODB 支持 C++11 标准，并提供了类型安全的数据库访问接口。

### ODB 的主要特性

- **类型安全**：编译时类型检查，避免运行时错误
- **高性能**：直接生成优化的 SQL 代码
- **跨平台**：支持多种数据库（MySQL、SQLite、PostgreSQL、Oracle 等）
- **现代 C++**：充分利用 C++11/14/17 特性
- **工具链完整**：包含编译器插件、数据库驱动等

## 环境配置

### 1. 安装 ODB

#### Ubuntu/Debian
```bash
# 添加 ODB 仓库
sudo apt-add-repository pascal-lobre/odb
sudo apt-get update

# 安装 ODB 编译器和运行时
sudo apt-get install libodb libodb-boost libodb-sqlite

# 安装 ODB 编译器插件
sudo apt-get install odb-compiler
```

#### macOS (使用 Homebrew)
```bash
brew install odb
```

#### 从源码安装
```bash
# 下载 ODB 源码
wget https://github.com/odb/odb/releases/download/2.4.0/odb-2.4.0.tar.bz2
tar xjf odb-2.4.0.tar.bz2
cd odb-2.4.0

# 编译安装
./configure
make
sudo make install
```

### 2. 项目配置

#### CMake 配置示例
```cmake
# 查找 ODB 包
find_package(ODB REQUIRED)

# 添加 ODB 编译器规则
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/user-odb.hxx ${CMAKE_CURRENT_BINARY_DIR}/user-odb.cxx
    COMMAND ${ODB_COMPILER} --std c++11 --profile boost ${CMAKE_CURRENT_SOURCE_DIR}/user.hpp
    DEPENDS user.hpp
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

# 添加生成的文件到源码
set(USER_ODB_SOURCES 
    ${CMAKE_CURRENT_BINARY_DIR}/user-odb.hxx
    ${CMAKE_CURRENT_BINARY_DIR}/user-odb.cxx
)

# 创建库
add_library(user_odb ${USER_ODB_SOURCES})
target_link_libraries(user_odb odb odb-boost)
```

## 实体类定义

### 基本语法

#### 1. 类注解
```cpp
#pragma db object
class user {
    // 类成员定义
};
```

#### 2. 字段注解
```cpp
#pragma db id auto
std::string uid_;

#pragma db not_null unique
std::string account_;

#pragma db value_not_null
Gender gender_;
```

### 字段属性说明

| 属性 | 说明 | 示例 |
|------|------|------|
| `id` | 主键 | `#pragma db id auto` |
| `not_null` | 非空约束 | `#pragma db not_null` |
| `unique` | 唯一约束 | `#pragma db unique` |
| `default` | 默认值 | `#pragma db default(0)` |
| `index` | 创建索引 | `#pragma db index` |
| `column` | 自定义列名 | `#pragma db column("user_name")` |
| `type` | 自定义类型 | `#pragma db type("VARCHAR(255)")` |

### 类型自动映射

ODB会根据C++变量的类型自动推导出数据库中的对应类型。以下是常见类型的映射关系：

| C++ 类型 | SQLite 类型 | MySQL 类型 | PostgreSQL 类型 | 说明 |
|----------|-------------|------------|-----------------|------|
| `int` | `INTEGER` | `INT` | `INTEGER` | 32位整数 |
| `long` | `INTEGER` | `BIGINT` | `BIGINT` | 64位整数 |
| `long long` | `INTEGER` | `BIGINT` | `BIGINT` | 64位整数 |
| `unsigned int` | `INTEGER` | `INT UNSIGNED` | `INTEGER` | 无符号32位整数 |
| `unsigned long` | `INTEGER` | `BIGINT UNSIGNED` | `BIGINT` | 无符号64位整数 |
| `float` | `REAL` | `FLOAT` | `REAL` | 单精度浮点数 |
| `double` | `REAL` | `DOUBLE` | `DOUBLE PRECISION` | 双精度浮点数 |
| `bool` | `INTEGER` | `TINYINT(1)` | `BOOLEAN` | 布尔值（0或1） |
| `std::string` | `TEXT` | `VARCHAR(255)` | `VARCHAR(255)` | 字符串 |
| `std::string` (大文本) | `TEXT` | `TEXT` | `TEXT` | 大文本（无需指定type） |
| `std::time_t` | `INTEGER` | `BIGINT` | `BIGINT` | 时间戳 |
| `std::vector<char>` | `BLOB` | `BLOB` | `BYTEA` | 二进制数据 |
| `std::vector<std::string>` | `TEXT` | `TEXT` | `TEXT` | JSON数组 |
| `std::map<std::string, std::string>` | `TEXT` | `TEXT` | `TEXT` | JSON对象 |

#### 示例：自动类型映射
```cpp
#pragma db object
class example {
private:
    int id_;                    // 自动映射为 INTEGER/INT
    std::string name_;          // 自动映射为 TEXT/VARCHAR(255)
    double price_;              // 自动映射为 REAL/DOUBLE
    bool active_;               // 自动映射为 INTEGER/TINYINT(1)
    std::time_t created_at_;    // 自动映射为 INTEGER/BIGINT
    std::string description_;   // 自动映射为 TEXT/VARCHAR(255)
    
    // 显式指定类型
    #pragma db type("TEXT")
    std::string large_text_;    // 强制使用 TEXT 类型
    
    #pragma db type("VARCHAR(100)")
    std::string short_name_;    // 限制字符串长度为100
};
```

#### 枚举类型映射
枚举类型默认映射为整数类型：
```cpp
#pragma db enum
enum class Status {
    ACTIVE = 0,
    INACTIVE = 1,
    PENDING = 2
};

#pragma db object
class user {
private:
    #pragma db value_not_null
    Status status_;  // 自动映射为 INTEGER/INT
};
```

#### 自定义类型映射
如果需要特殊的类型映射，可以使用`type`属性：
```cpp
#pragma db object
class user {
private:
    // 使用JSON类型存储（需要数据库支持JSON）
    #pragma db type("JSON")
    std::string preferences_;  // 存储JSON字符串
    
    // 使用UUID类型
    #pragma db type("UUID")
    std::string uuid_;
};
```

### 完整实体类示例

参考 `services/odb/user.hpp` 文件，我们已经定义了一个完整的用户实体类：

```cpp
#pragma db object
class user {
public:
    // 构造函数
    user() : create_time_(0), last_login_(0), online_(false) {}
    
    // Getter 和 Setter 方法
    const std::string& uid() const { return uid_; }
    void uid(const std::string& uid) { uid_ = uid; }
    
    // ... 其他 getter/setter 方法
    
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
    
    // ... 其他字段
};
```

### 枚举类型处理

```cpp
#pragma db enum
enum class Gender {
    UNKNOWN = 0,
    MALE = 1,
    FEMALE = 2,
    OTHER = 3
};
```

## 数据库操作

### 1. 数据库连接

```cpp
#include <odb/database.hxx>
#include <odb/sqlite/database.hxx>

// 创建数据库连接
std::shared_ptr<odb::database> db(
    new odb::sqlite::database("test.db", 0, 0, 0, 
                              odb::sqlite::database::read_uncommitted | 
                              odb::sqlite::database::deferred_foreign_keys)
);
```

### 2. 事务管理

```cpp
#include <odb/transaction.hxx>

void transaction_example() {
    odb::transaction t(db->begin());
    
    try {
        // 执行数据库操作
        // ...
        
        t.commit();
    }
    catch (...) {
        t.rollback();
        throw;
    }
}
```

### 3. 基本 CRUD 操作

#### 创建对象
```cpp
void create_user() {
    odb::transaction t(db->begin());
    
    user u("123", "test@example.com", "Test User", "avatar.jpg");
    u.gender(Gender::MALE);
    u.create_time(std::time(nullptr));
    
    db->persist(u);
    
    t.commit();
}
```

#### 查询对象
```cpp
#include <odb/query.hxx>
#include <odb/result.hxx>

void query_user() {
    odb::transaction t(db->begin());
    
    // 查询所有用户
    odb::result<user> r(db->query<user>());
    
    for (const user& u : r) {
        std::cout << "User: " << u.nickname() << std::endl;
    }
    
    // 条件查询
    odb::result<user> r2(db->query<user>(odb::query<user>::account == "test@example.com"));
    
    t.commit();
}
```

#### 更新对象
```cpp
void update_user() {
    odb::transaction t(db->begin());
    
    odb::result<user> r(db->query<user>(odb::query<user>::account == "test@example.com"));
    
    if (r.begin() != r.end()) {
        user& u = *r.begin();
        u.nickname("Updated Name");
        db->update(u);
    }
    
    t.commit();
}
```

#### 删除对象
```cpp
void delete_user() {
    odb::transaction t(db->begin());
    
    odb::result<user> r(db->query<user>(odb::query<user>::account == "test@example.com"));
    
    if (r.begin() != r.end()) {
        db->erase(*r.begin());
    }
    
    t.commit();
}
```

### 4. 查询构建器

```cpp
void complex_query() {
    odb::transaction t(db->begin());
    
    // 复杂查询示例
    odb::result<user> r(db->query<user>(
        odb::query<user>::gender == Gender::MALE &&
        odb::query<user>::create_time > (std::time(nullptr) - 86400) && // 最近24小时
        odb::query<user>::online == true
    ).limit(10).order_by(odb::query<user>::create_time.desc()));
    
    t.commit();
}
```

## 高级特性

### 1. 关系映射

#### 一对一关系
```cpp
#pragma db object
class profile {
public:
    // ...
private:
    #pragma db id auto
    unsigned long id_;
    
    #pragma db not_null
    #pragma db unique
    #pragma db on_update cascade
    user user_; // 一对一关系
};
```

#### 一对多关系
```cpp
#pragma db object
class post {
public:
    // ...
private:
    #pragma db not_null
    #pragma db on_update cascade
    user author_; // 多对一关系
};

#pragma db object
class user {
public:
    // ...
private:
    // ...
    #pragma db type("std::vector<std::shared_ptr<post>>")
    std::vector<std::shared_ptr<post>> posts_; // 一对多关系
};
```

### 2. 继承映射

```cpp
#pragma db object abstract
class person {
public:
    // ...
private:
    std::string name_;
    int age_;
};

#pragma db object
class employee : public person {
public:
    // ...
private:
    std::string employee_id_;
    double salary_;
};
```

### 3. 自定义查询

```cpp
// 在类定义中添加查询方法
class user {
public:
    // ...
    static std::shared_ptr<user> find_by_account(
        odb::database& db, const std::string& account) {
        
        odb::transaction t(db.begin());
        
        odb::result<user> r(db.query<user>(
            odb::query<user>::account == account));
        
        std::shared_ptr<user> u;
        if (r.begin() != r.end()) {
            u = std::make_shared<user>(*r.begin());
        }
        
        t.commit();
        return u;
    }
};
```

### 4. 数据库迁移

#### 重要说明：ODB不会自动创建表结构

ODB本身**不会**根据实体类的注解自动创建表结构。您需要手动创建数据库表。这是ODB设计哲学的一部分：它专注于对象关系映射，而不是数据库管理。

#### 创建数据库表

```cpp
// 创建数据库表
void create_tables() {
    odb::transaction t(db->begin());
    
    // 创建用户表
    db->schema().create_table<user>();
    
    // 创建其他表
    // ...
    
    t.commit();
}

// 删除数据库表
void drop_tables() {
    odb::transaction t(db->begin());
    
    // 删除用户表
    db->schema().drop_table<user>();
    
    // 删除其他表
    // ...
    
    t.commit();
}
```

#### 表结构创建时机

通常在以下情况下需要创建表结构：

1. **应用首次启动时**
2. **数据库迁移时**
3. **测试环境初始化时**

#### 自动化表创建示例

```cpp
class database_manager {
public:
    static database_manager& instance() {
        static database_manager manager;
        return manager;
    }
    
    void initialize_database() {
        if (!tables_initialized()) {
            create_tables();
            mark_tables_initialized();
        }
    }
    
private:
    bool tables_initialized() {
        // 检查表是否已存在的逻辑
        // 可以查询数据库的表信息
        return false; // 实际实现中需要检查数据库
    }
    
    void mark_tables_initialized() {
        // 标记表已初始化的逻辑
        // 可以写入配置文件或数据库
    }
    
    void create_tables() {
        odb::transaction t(db_->begin());
        
        try {
            // 创建用户表
            db_->schema().create_table<user>();
            
            // 创建其他表
            // db_->schema().create_table<other_entity>();
            
            std::cout << "Database tables created successfully." << std::endl;
            t.commit();
        }
        catch (const odb::exception& e) {
            std::cerr << "Failed to create database tables: " << e.what() << std::endl;
            t.rollback();
            throw;
        }
    }
    
    std::shared_ptr<odb::database> db_;
};

// 在应用启动时调用
int main() {
    // 初始化数据库连接
    auto& db_manager = database_manager::instance();
    db_manager.initialize_database();
    
    // 其他应用逻辑...
    return 0;
}
```

#### 使用SQL脚本创建表

```cpp
// 使用SQL脚本创建表（推荐用于生产环境）
void create_tables_from_sql() {
    std::string sql = R"(
    CREATE TABLE IF NOT EXISTS user (
        uid TEXT PRIMARY KEY,
        account TEXT NOT NULL UNIQUE,
        nickname TEXT,
        avatar TEXT,
        gender INTEGER NOT NULL DEFAULT 0,
        signature TEXT,
        create_time INTEGER NOT NULL DEFAULT 0,
        last_login INTEGER NOT NULL DEFAULT 0,
        online INTEGER NOT NULL DEFAULT 0,
        phone_number TEXT,
        email TEXT,
        address TEXT,
        birthday TEXT,
        company TEXT,
        job_title TEXT,
        wxid TEXT,
        qqid TEXT,
        real_name TEXT,
        extra TEXT
    );
    )";
    
    odb::transaction t(db->begin());
    
    // 执行SQL语句
    odb::result<std::string> r(db->query<std::string>(
        "SELECT name FROM sqlite_master WHERE type='table' AND name='user'"));
    
    if (r.begin() == r.end()) {
        // 表不存在，创建表
        db->execute(sql);
        std::cout << "Table 'user' created successfully." << std::endl;
    }
    
    t.commit();
}
```

#### 数据库版本管理

```cpp
class database_migrator {
public:
    static database_migrator& instance() {
        static database_migrator migrator;
        return migrator;
    }
    
    void migrate() {
        int current_version = get_current_version();
        
        switch (current_version) {
            case 0:
                migrate_to_version_1();
                // fall through
            case 1:
                migrate_to_version_2();
                // fall through
            case 2:
                // 最新版本
                break;
            default:
                throw std::runtime_error("Unknown database version");
        }
        
        set_current_version(get_latest_version());
    }
    
private:
    int get_current_version() {
        // 从数据库配置表中获取当前版本
        return 0; // 默认版本
    }
    
    void set_current_version(int version) {
        // 更新数据库版本
    }
    
    void migrate_to_version_1() {
        odb::transaction t(db_->begin());
        
        // 创建用户表
        db_->schema().create_table<user>();
        
        // 创建数据库版本表
        std::string create_version_table = R"(
        CREATE TABLE database_version (
            version INTEGER PRIMARY KEY
        );
        )";
        db_->execute(create_version_table);
        
        // 插入初始版本
        db_->execute("INSERT INTO database_version (version) VALUES (1)");
        
        t.commit();
    }
    
    void migrate_to_version_2() {
        odb::transaction t(db_->begin());
        
        // 添加新字段
        std::string alter_table = R"(
        ALTER TABLE user ADD COLUMN phone_number TEXT;
        ALTER TABLE user ADD COLUMN email TEXT;
        )";
        db_->execute(alter_table);
        
        t.commit();
    }
    
    int get_latest_version() {
        return 2; // 返回最新版本号
    }
    
    std::shared_ptr<odb::database> db_;
};
```

## 最佳实践

### 1. 错误处理

```cpp
void safe_database_operation() {
    try {
        odb::transaction t(db->begin());
        
        // 数据库操作
        // ...
        
        t.commit();
    }
    catch (const odb::exception& e) {
        std::cerr << "Database error: " << e.what() << std::endl;
        // 处理数据库错误
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        // 处理其他错误
    }
}
```

### 2. 连接池管理

```cpp
class database_pool {
public:
    static database_pool& instance() {
        static database_pool pool;
        return pool;
    }
    
    std::shared_ptr<odb::database> get_connection() {
        std::lock_guard<std::mutex> lock(mutex_);
        
        if (connections_.empty()) {
            return create_connection();
        }
        
        auto conn = connections_.top();
        connections_.pop();
        return conn;
    }
    
    void return_connection(std::shared_ptr<odb::database> conn) {
        std::lock_guard<std::mutex> lock(mutex_);
        connections_.push(conn);
    }
    
private:
    std::stack<std::shared_ptr<odb::database>> connections_;
    std::mutex mutex_;
    
    std::shared_ptr<odb::database> create_connection() {
        return std::make_shared<odb::sqlite::database>(
            "test.db", 0, 0, 0,
            odb::sqlite::database::read_uncommitted |
            odb::sqlite::database::deferred_foreign_keys);
    }
};
```

### 3. 性能优化

#### 使用预编译查询
```cpp
// 预编译查询
odb::prepared_query<user> pq = db->prepare_query<user>(
    odb::query<user>::gender == Gender::MALE);

// 使用预编译查询
void use_prepared_query() {
    odb::transaction t(db->begin());
    
    odb::result<user> r(pq.execute());
    
    for (const user& u : r) {
        // 处理结果
    }
    
    t.commit();
}
```

#### 批量操作
```cpp
void batch_insert() {
    odb::transaction t(db->begin());
    
    std::vector<user> users;
    for (int i = 0; i < 1000; ++i) {
        users.emplace_back("uid_" + std::to_string(i), 
                          "user_" + std::to_string(i) + "@example.com",
                          "User " + std::to_string(i));
    }
    
    for (const auto& u : users) {
        db->persist(u);
    }
    
    t.commit();
}
```

### 4. 日志记录

```cpp
class database_logger : public odb::database::logger {
public:
    virtual void log(const char* file, unsigned long line,
                    const char* function, const char* info) override {
        std::cout << "[" << file << ":" << line << "] " 
                  << function << ": " << info << std::endl;
    }
    
    virtual void log(const char* file, unsigned long line,
                    const char* function, const char* info,
                    const odb::database::connection& conn) override {
        std::cout << "[" << file << ":" << line << "] " 
                  << function << ": " << info << std::endl;
    }
};

// 使用日志记录器
std::shared_ptr<odb::database> db(
    new odb::sqlite::database("test.db", 0, 0, 0,
                              odb::sqlite::database::read_unmitted |
                              odb::sqlite::database::deferred_foreign_keys));
db->logger(new database_logger());
```

## 常见问题

### 1. 编译错误

#### 错误：`#pragma db` 注解未识别
**原因**：ODB 编译器未正确配置
**解决方案**：
```bash
# 检查 odb-compiler 是否安装
which odb-compiler

# 如果未安装，安装 ODB 编译器插件
sudo apt-get install odb-compiler
```

#### 错误：`#pragma db enum` 枚举类型未定义
**原因**：枚举类型定义顺序问题
**解决方案**：
```cpp
// 确保枚举类型在使用前已定义
#pragma db enum
enum class Gender {
    UNKNOWN = 0,
    MALE = 1,
    FEMALE = 2,
    OTHER = 3
};

#pragma db object
class user {
private:
    #pragma db value_not_null
    Gender gender_;
};
```

### 2. 运行时错误

#### 错误：数据库连接失败
**原因**：数据库文件路径错误或权限不足
**解决方案**：
```cpp
// 检查数据库文件路径
std::string db_path = "/path/to/database.db";
std::cout << "Database path: " << db_path << std::endl;

// 确保目录存在
boost::filesystem::create_directories(boost::filesystem::path(db_path).parent_path());

// 检查文件权限
std::ofstream test_file(db_path);
if (test_file.is_open()) {
    test_file.close();
    std::remove(db_path.c_str());
} else {
    std::cerr << "Cannot create database file: permission denied" << std::endl;
}
```

#### 错误：事务超时
**原因**：长时间运行的事务未提交或回滚
**解决方案**：
```cpp
void transaction_with_timeout() {
    odb::transaction t(db->begin());
    
    try {
        // 设置事务超时（如果数据库支持）
        // ...
        
        // 执行数据库操作
        // ...
        
        t.commit();
    }
    catch (const odb::timeout& e) {
        std::cerr << "Transaction timeout: " << e.what() << std::endl;
        t.rollback();
    }
    catch (...) {
        t.rollback();
        throw;
    }
}
```

### 3. 性能问题

#### 问题：查询速度慢
**解决方案**：
```cpp
// 1. 添加索引
#pragma db index
std::string nickname_;

// 2. 使用分页查询
odb::result<user> r(db->query<user>().limit(100).offset(0));

// 3. 只查询需要的字段
odb::result<user> r(db->query<user>(
    odb::query<user>::nickname.column("nickname_") // 只查询昵称字段
));

// 4. 使用连接池
auto conn = database_pool::instance().get_connection();
// 使用连接
database_pool::instance().return_connection(conn);
```

#### 问题：内存占用高
**解决方案**：
```cpp
// 1. 使用流式查询
odb::result<user> r(db->query<user>());
for (const user& u : r) {
    // 逐条处理，避免一次性加载所有数据
    process_user(u);
}

// 2. 使用智能指针管理对象
std::vector<std::shared_ptr<user>> users;
for (const auto& u : r) {
    users.push_back(std::make_shared<user>(u));
}
```

### 4. 调试技巧

#### 启用 SQL 日志
```cpp
class sql_logger : public odb::database::logger {
public:
    virtual void log(const char* file, unsigned long line,
                    const char* function, const char* info) override {
        std::cout << "[SQL] " << info << std::endl;
    }
    
    virtual void log(const char* file, unsigned long line,
                    const char* function, const char* info,
                    const odb::database::connection& conn) override {
        std::cout << "[SQL] " << info << std::endl;
    }
};

// 启用 SQL 日志
db->logger(new sql_logger());
```

#### 使用调试模式
```cpp
// 创建数据库连接时启用调试模式
std::shared_ptr<odb::database> db(
    new odb::sqlite::database("test.db", 0, 0, 0,
                              odb::sqlite::database::read_uncommitted |
                              odb::sqlite::database::deferred_foreign_keys |
                              odb::sqlite::database::debug));
```

## 总结

ODB 是一个功能强大的 C++ ORM 框架，它提供了类型安全的数据库访问接口，支持多种数据库，并且具有优秀的性能。通过本教程，您应该已经掌握了 ODB 的基本使用方法，包括：

1. 环境配置和项目设置
2. 实体类定义和字段映射
3. 基本的 CRUD 操作
4. 高级特性如关系映射和继承
5. 最佳实践和性能优化
6. 常见问题的解决方案

在实际项目中，建议根据具体需求选择合适的数据库，并遵循最佳实践来确保代码的可维护性和性能。同时，充分利用 ODB 的类型安全特性来减少运行时错误，提高代码质量。

## 参考资料

- [ODB 官方文档](https://www.codesynthesis.com/products/odb/doc/)
- [ODB GitHub 仓库](https://github.com/odb/odb)
- [C++ ORM 框架对比](https://en.wikipedia.org/wiki/List_of_object-relational_mapping_software#C++)
- [SQLite 官方文档](https://www.sqlite.org/docs.html)
- [Boost C++ Libraries](https://www.boost.org/)

---

*本教程最后更新时间：2025年9月28日*
