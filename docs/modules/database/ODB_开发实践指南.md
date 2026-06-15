# ODB 开发实践指南

## 1. ODB 入门教程

### 1.1 什么是ORM？

ORM (Object-Relational Mapping) 对象关系映射是一种编程技术，用于：

- **将面向对象的数据模型映射到关系数据库**
- **让开发者用面向对象的方式操作数据库**
- **减少手写SQL的工作量**
- **提供类型安全的数据库操作**

**传统SQL方式 vs ORM方式：**

```cpp
// 传统SQL方式
std::string sql = "INSERT INTO users (name, email) VALUES (?, ?)";
auto stmt = connection.prepare(sql);
stmt.bind(1, user_name);
stmt.bind(2, user_email);
stmt.execute();

// ORM方式 (ODB)
User user;
user.set_name(user_name);
user.set_email(user_email);
db.persist(user);  // 自动生成SQL并执行
```

### 1.2 ODB的独特优势

#### 1.2.1 编译时代码生成
ODB使用编译器技术，在**编译时**分析C++类定义，生成数据库映射代码：

```
源代码: User.hpp → ODB编译器 → 生成: User-odb.hpp, User-odb.cpp
```

**优势：**
- **零运行时反射开销**：所有映射代码都是编译时生成的
- **类型安全**：编译时就能发现类型不匹配的问题
- **性能优异**：生成的代码经过优化，效率接近手写SQL

#### 1.2.2 C++原生支持
ODB是专门为C++设计的ORM，完美支持C++特性：

```cpp
#pragma db object
class User {
    #pragma db id auto
    unsigned long id_;
    
    std::string name_;
    std::vector<std::string> tags_;  // 支持STL容器
    std::shared_ptr<Profile> profile_;  // 支持智能指针
    
    // 支持C++访问控制
private:
    friend class odb::access;
};
```

#### 1.2.3 数据库无关性
同一套C++代码可以工作在不同数据库上：

```cpp
// PostgreSQL
odb::pgsql::database db("host=localhost dbname=mydb user=postgres");

// MySQL (只需要改变数据库对象)
odb::mysql::database db("host=localhost;user=root;database=mydb");

// SQLite
odb::sqlite::database db("mydb.db");
```

### 1.3 ODB开发流程

#### 步骤1：定义持久化类
```cpp
// User.hpp
#pragma db object
class User {
public:
    User() {}
    User(const std::string& name, const std::string& email)
        : name_(name), email_(email) {}
    
    // 访问器方法
    unsigned long id() const { return id_; }
    const std::string& name() const { return name_; }
    void set_name(const std::string& name) { name_ = name; }
    
private:
    friend class odb::access;
    
    #pragma db id auto
    unsigned long id_;
    
    #pragma db not_null
    std::string name_;
    
    std::string email_;
};
```

#### 步骤2：ODB编译
```bash
# 生成数据库映射代码
odb -d pgsql --generate-query --generate-schema User.hpp

# 生成的文件：
# User-odb.hpp   - 头文件，包含映射声明
# User-odb.cpp   - 实现文件，包含映射实现
# User-odb.sql   - SQL模式文件，创建表结构
```

#### 步骤3：编译链接
```bash
g++ -I$(ODB_PREFIX)/include \
    main.cpp User-odb.cpp \
    -lodb-pgsql -lodb -lpq
```

#### 步骤4：使用
```cpp
#include "User.hpp"
#include "User-odb.hpp"

int main() {
    // 创建数据库连接
    odb::pgsql::database db("host=localhost dbname=mydb user=postgres");
    
    // 创建表结构
    {
        odb::transaction t(db.begin());
        odb::schema_catalog::create_schema(db);
        t.commit();
    }
    
    // 插入用户
    {
        odb::transaction t(db.begin());
        
        User user("张三", "zhangsan@example.com");
        db.persist(user);
        
        t.commit();
        std::cout << "User ID: " << user.id() << std::endl;
    }
    
    return 0;
}
```

## 2. ODB 核心概念详解

### 2.1 持久化类声明

#### 2.1.1 基本声明
```cpp
#pragma db object  // 声明这是一个持久化类
class MyClass {
    // 类定义
};
```

#### 2.1.2 主键定义
```cpp
#pragma db object
class User {
    // 自动递增主键
    #pragma db id auto
    unsigned long id_;
    
    // 手动指定主键
    #pragma db id
    std::string account_;
    
    // 复合主键
    #pragma db id
    std::string group_id_;
    #pragma db id
    std::string user_id_;
};
```

#### 2.1.3 列属性设置
```cpp
#pragma db object
class User {
    // 不能为NULL
    #pragma db not_null
    std::string name_;
    
    // 设置列名和类型
    #pragma db column("user_email") type("VARCHAR(255)")
    std::string email_;
    
    // 设置默认值
    #pragma db default(0)
    int status_;
    
    // 唯一约束
    #pragma db unique
    std::string account_;
    
    // 索引
    #pragma db index
    std::string phone_;
};
```

#### 2.1.4 关系映射
```cpp
// 一对一关系
#pragma db object
class User {
    #pragma db id auto
    unsigned long id_;
    
    // 延迟加载的一对一关系
    #pragma db load(lazy)
    std::shared_ptr<Profile> profile_;
};

// 一对多关系
#pragma db object
class Post {
    #pragma db id auto
    unsigned long id_;
    
    // 多对一关系（多个帖子属于一个用户）
    std::shared_ptr<User> author_;
    
    // 一对多关系（一个帖子有多个评论）
    std::vector<std::shared_ptr<Comment>> comments_;
};
```

### 2.2 事务管理

#### 2.2.1 事务基础
```cpp
odb::transaction t(db.begin());  // 开始事务

// 数据库操作
db.persist(user);
db.update(user);

t.commit();  // 提交事务，如果不调用commit，析构时会自动回滚
```

#### 2.2.2 事务异常处理
```cpp
try {
    odb::transaction t(db.begin());
    
    // 可能抛异常的操作
    db.persist(user);
    risky_operation();
    
    t.commit();  // 只有所有操作成功才提交
} catch (const std::exception& e) {
    // 事务会自动回滚
    std::cerr << "Transaction failed: " << e.what() << std::endl;
}
```

### 2.3 查询语言

#### 2.3.1 基本查询
```cpp
typedef odb::query<User> query;
typedef odb::result<User> result;

// 查询所有用户
result r = db.query<User>();

// 条件查询
result r = db.query<User>(query::name == "张三");

// 复合条件
result r = db.query<User>(
    query::age > 18 && query::status == 1);

// 模糊查询
result r = db.query<User>(
    query::name.like("%张%"));
```

#### 2.3.2 查询结果处理
```cpp
// 遍历结果
for (const User& user : db.query<User>(query::active == true)) {
    std::cout << "User: " << user.name() << std::endl;
}

// 获取单个结果
auto users = db.query<User>(query::account == "admin");
if (!users.empty()) {
    User admin = *users.begin();
    // 使用admin对象
}

// 统计查询
auto count = db.query_value<std::size_t>(
    "SELECT COUNT(*) FROM users WHERE active = true");
```

### 2.4 会话(Session)机制

#### 2.4.1 什么是会话？
会话是ODB的**对象缓存机制**，用于：
- **避免重复加载相同对象**
- **维护对象身份的唯一性**
- **延迟加载关联对象**

#### 2.4.2 会话使用
```cpp
// 启用会话
odb::session s;

odb::transaction t(db.begin());

// 第一次加载
auto user1 = db.load<User>(1);

// 第二次加载相同ID，直接从会话缓存返回
auto user2 = db.load<User>(1);

// user1 和 user2 指向同一个对象
assert(user1.get() == user2.get());

t.commit();
```

## 3. 为什么需要我们的封装？

### 3.1 ODB原生使用的挑战

#### 3.1.1 资源管理复杂
```cpp
// 原生ODB代码 - 需要小心管理资源
void save_user(const User& user) {
    odb::pgsql::database db("connection_string");
    
    odb::transaction t(db.begin());
    try {
        db.persist(user);
        t.commit();
    } catch (...) {
        // 需要手动处理事务回滚
        // 数据库连接也需要手动管理
        throw;
    }
}

// 问题：
// 1. 每次调用都创建新的数据库连接（性能差）
// 2. 需要手动管理事务生命周期
// 3. 异常处理复杂
// 4. 代码重复度高
```

#### 3.1.2 连接效率问题
```cpp
// 每个操作都创建新连接
void create_user() {
    odb::pgsql::database db("connection_string");  // 创建连接1
    // 操作...
}

void update_user() {
    odb::pgsql::database db("connection_string");  // 创建连接2
    // 操作...
}

void delete_user() {
    odb::pgsql::database db("connection_string");  // 创建连接3
    // 操作...
}

// 问题：
// - 连接建立有开销（TCP握手、认证等）
// - 数据库连接数会很快耗尽
// - 性能差，资源浪费
```

#### 3.1.3 错误处理分散
```cpp
// 每个函数都要重复错误处理逻辑
bool save_user(const User& user) {
    try {
        odb::pgsql::database db("connection_string");
        odb::transaction t(db.begin());
        db.persist(user);
        t.commit();
        return true;
    } catch (const odb::database_exception& e) {
        log_error("Database error: " + std::string(e.what()));
        return false;
    } catch (const std::exception& e) {
        log_error("General error: " + std::string(e.what()));
        return false;
    }
}

// 类似的错误处理在每个函数中都要重复
```

### 3.2 封装解决的问题

#### 3.2.1 连接池解决性能问题
```cpp
// 封装后：连接复用
void create_user() {
    auto conn = manager.get_connection();  // 从池中获取
    // 使用完自动归还
}

void update_user() {
    auto conn = manager.get_connection();  // 复用连接
    // 高效！
}
```

#### 3.2.2 RAII解决资源管理
```cpp
// 封装后：自动资源管理
void save_user_batch(const std::vector<User>& users) {
    manager.execute_transaction([&](auto& conn) {
        for (const auto& user : users) {
            conn.persist(user);
        }
        // 事务自动提交或回滚
        // 连接自动归还池
    });
}
```

#### 3.2.3 统一错误处理
```cpp
// 封装后：统一的错误处理策略
bool save_user_safe(const User& user) {
    return manager.safe_execute_transaction([&](auto& conn) {
        conn.persist(user);
        return true;
    }, false);  // 失败时返回false，错误已记录日志
}
```

## 4. 封装设计的核心思想

### 4.1 分层设计

```
┌─────────────────────────┐
│     应用业务逻辑层        │  ← 用户代码
├─────────────────────────┤
│    PgSqlManager         │  ← 管理器层：统一入口
├─────────────────────────┤
│    ConnectionPool       │  ← 连接池层：资源管理
├─────────────────────────┤
│    PgSqlConnection      │  ← 连接封装层：RAII
├─────────────────────────┤
│    ODB Database         │  ← ODB层：ORM功能
├─────────────────────────┤
│    PostgreSQL           │  ← 数据库层
└─────────────────────────┘
```

**分层的好处：**
- **职责分离**：每层只关注自己的职责
- **可替换性**：底层实现可以替换而不影响上层
- **可测试性**：每层都可以独立测试
- **可维护性**：修改某层不会影响其他层

### 4.2 RAII设计模式

RAII是C++中的重要设计模式：**Resource Acquisition Is Initialization**

```cpp
class Transaction {
public:
    Transaction(Database& db) : tx_(db.begin()) {}
    
    ~Transaction() {
        if (!committed_) {
            tx_.rollback();  // 析构时自动回滚
        }
    }
    
    void commit() {
        tx_.commit();
        committed_ = true;
    }
    
private:
    odb::transaction tx_;
    bool committed_ = false;
};

// 使用：
{
    Transaction tx(db);  // 构造时开始事务
    
    // 数据库操作
    db.persist(user);
    
    tx.commit();  // 显式提交
}  // 析构时自动处理（如果没提交则回滚）
```

**RAII的优势：**
1. **异常安全**：即使抛异常也能正确清理资源
2. **自动化**：不需要手动记住释放资源
3. **简洁**：代码更清晰，少了很多错误处理逻辑

### 4.3 单例设计模式

```cpp
class PgSqlManager : public Singleton<PgSqlManager> {
    // 确保全局只有一个数据库管理器实例
};

// 使用：
auto& manager = PgSqlManager::GetInstance();
// 或者：
auto& manager = pgsql_manager();  // 便捷函数
```

**为什么使用单例？**
- **全局访问点**：应用中任何地方都可以访问数据库
- **资源控制**：避免创建多个连接池，浪费资源
- **配置一致性**：确保整个应用使用相同的数据库配置

## 5. 实际开发建议

### 5.1 选择合适的抽象层次

```cpp
// 1. 低级API - 精确控制
void complex_operation() {
    auto conn = manager.get_connection();
    auto tx = conn->begin_transaction();
    
    // 复杂的多步操作
    step1(conn);
    step2(conn);
    step3(conn);
    
    tx.commit();
}

// 2. 中级API - 平衡便捷性和控制力
void normal_operation() {
    manager.execute([](auto& conn) {
        conn.persist(user);
        conn.update(config);
    });
}

// 3. 高级API - 最大便捷性
void safe_operation() {
    auto success = manager.safe_execute_transaction([](auto& conn) {
        conn.persist(user);
        return true;
    }, false);
}
```

### 5.2 性能优化建议

#### 5.2.1 合理设置连接池大小
```cpp
// 配置建议
PgSqlConfig config;
config.pool_size = std::thread::hardware_concurrency() * 2;  // CPU核心数的2倍
config.connect_timeout = 30;  // 30秒连接超时
config.query_timeout = 60;    // 60秒查询超时
```

#### 5.2.2 批量操作优化
```cpp
// 好的做法：批量操作在同一事务中
manager.execute_transaction([&](auto& conn) {
    for (const auto& user : users) {
        conn.persist(user);  // 所有插入在一个事务中
    }
});

// 不好的做法：每个操作一个事务
for (const auto& user : users) {
    manager.execute_transaction([&](auto& conn) {
        conn.persist(user);  // 每次都开新事务，性能差
    });
}
```

#### 5.2.3 适当使用会话缓存
```cpp
// 在需要多次访问相关对象时启用会话
manager.execute([](auto& conn) {
    conn.enable_session_cache();
    
    auto user = conn.load<User>(user_id);
    auto profile = user->profile();  // 延迟加载，缓存在会话中
    auto posts = user->posts();      // 从缓存获取
    
    // 会话在连接归还时自动清理
});
```

这份指南应该能帮助你更好地理解ODB和我们封装设计的思路。有什么具体的概念需要进一步解释吗？