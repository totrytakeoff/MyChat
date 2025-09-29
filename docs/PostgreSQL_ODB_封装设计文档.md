# PostgreSQL ODB 封装设计文档

## 1. 前言

本文档详细解释基于ODB的PostgreSQL数据库操作封装设计，旨在简化数据库操作、提高操作安全性，并提供RAII资源管理。

## 2. ODB基础概念

### 2.1 什么是ODB？

ODB (Object-Relational Database) 是一个C++的对象关系映射(ORM)系统，它允许你：

- **将C++对象直接映射到数据库表**
- **无需手写SQL语句**进行基本的CRUD操作
- **编译时类型检查**，避免运行时SQL错误
- **自动生成数据库映射代码**

### 2.2 ODB工作原理

```
C++类定义 → ODB编译器 → 生成映射代码 → 链接到程序
```

**示例：**
```cpp
#pragma db object
class User {
private:
    friend class odb::access;
    
    #pragma db id auto
    unsigned long id_;
    
    std::string name_;
    std::string email_;
    
public:
    // 构造函数、访问器等
};
```

ODB编译器会生成：
- 数据库表结构
- C++对象与数据库行之间的转换代码
- 查询接口代码

### 2.3 ODB的优势

1. **类型安全**：编译时检查，减少运行时错误
2. **代码简洁**：无需手写大量SQL和转换代码
3. **数据库无关**：支持多种数据库（PostgreSQL、MySQL、SQLite等）
4. **性能优化**：预编译语句、连接池等优化

## 3. 为什么需要封装？

### 3.1 ODB原生使用的问题

**原生ODB使用示例：**
```cpp
// 每次都需要手动管理连接和事务
odb::pgsql::database db("host=localhost dbname=mydb user=postgres");
odb::transaction t(db.begin());

User user;
user.set_name("张三");
db.persist(user);

t.commit();  // 忘记commit就丢失数据
```

**问题：**
1. **资源管理复杂**：需要手动管理数据库连接、事务
2. **重复代码**：每个操作都要写连接、事务管理代码
3. **错误处理繁琐**：需要在每个地方处理异常和资源清理
4. **连接效率低**：每次操作都创建新连接

### 3.2 封装的目标

1. **RAII资源管理**：自动管理连接、事务生命周期
2. **连接池复用**：高效管理数据库连接
3. **统一异常处理**：提供安全的操作接口
4. **简化API**：提供不同抽象层次的操作接口
5. **配置集中化**：统一管理数据库配置

## 4. 封装架构设计

### 4.1 整体架构

```
应用层
    ↓
PgSqlManager (管理器层)
    ↓
ConnectionPool (连接池层)
    ↓
PgSqlConnection (连接封装层)
    ↓
ODB Database (ODB层)
    ↓
PostgreSQL (数据库层)
```

### 4.2 各层职责

#### 4.2.1 PgSqlConnection (连接封装层)
**作用：**
- 封装单个ODB数据库连接
- 提供RAII事务管理
- 提供便捷的CRUD操作接口
- 处理连接级别的异常

**主要功能：**
- 连接创建和测试
- 事务的自动管理
- 对象持久化操作
- 查询操作
- 原生SQL执行
- 数据库模式管理

#### 4.2.2 ConnectionPool (连接池层)
**作用：**
- 管理多个数据库连接
- 连接复用，避免频繁创建/销毁连接
- 连接负载均衡
- 连接健康检查

**优势：**
- **性能提升**：复用连接，减少连接建立开销
- **资源控制**：限制并发连接数，避免数据库过载
- **稳定性**：连接池监控，自动重连

#### 4.2.3 PgSqlManager (管理器层)
**作用：**
- 管理连接池
- 提供统一的数据库操作入口
- 配置管理
- 全局异常处理

**设计模式：**
- **单例模式**：确保全局唯一的数据库管理器
- **工厂模式**：创建和管理连接
- **RAII模式**：自动资源管理

## 5. 配置参数详解

### 5.1 基础连接参数

```cpp
struct PgSqlConfig {
    std::string host = "127.0.0.1";     // 数据库服务器地址
    int port = 5432;                    // 数据库端口
    std::string database = "mychat";    // 数据库名
    std::string user = "postgres";      // 用户名
    std::string password = "";          // 密码
```

**参数说明：**
- **host**: PostgreSQL服务器IP地址或域名
  - `127.0.0.1` 或 `localhost`：本地数据库
  - 远程IP：连接远程数据库服务器
- **port**: PostgreSQL监听端口，默认5432
- **database**: 要连接的具体数据库名称
- **user/password**: 数据库认证信息

### 5.2 连接池参数

```cpp
    int pool_size = 10;                 // 连接池大小
    int connect_timeout = 30;           // 连接超时(秒)
    int query_timeout = 30;             // 查询超时(秒)
```

**参数说明：**
- **pool_size**: 连接池中维护的连接数量
  - 太小：高并发时连接不够用
  - 太大：占用过多数据库资源
  - 推荐：根据应用并发量调整，一般10-50
- **connect_timeout**: 建立连接的超时时间
  - 防止网络问题导致的长时间等待
- **query_timeout**: 单个查询的超时时间
  - 防止慢查询阻塞应用

### 5.3 SSL安全参数

```cpp
    std::string sslmode = "prefer";     // SSL模式
    std::string sslcert = "";           // SSL证书文件
    std::string sslkey = "";            // SSL私钥文件
    std::string sslrootcert = "";       // SSL根证书
```

**SSL模式说明：**
- **disable**: 不使用SSL
- **require**: 强制使用SSL
- **prefer**: 优先使用SSL，不支持时回退到非SSL
- **verify-ca**: 使用SSL并验证服务器证书
- **verify-full**: 使用SSL并完全验证服务器

### 5.4 其他参数

```cpp
    std::string options = "";           // 额外的连接选项
```

**options示例：**
```cpp
// 设置应用名称，便于数据库监控
options = "application_name=MyChat";

// 设置时区
options = "timezone=Asia/Shanghai";

// 多个选项用空格分隔
options = "application_name=MyChat timezone=Asia/Shanghai";
```

## 6. RAII封装的作用和优势

### 6.1 什么是RAII？

RAII (Resource Acquisition Is Initialization) 是C++的资源管理idiom：
- **构造时获取资源**
- **析构时自动释放资源**
- **异常安全**：即使发生异常也能正确清理

### 6.2 事务RAII封装

**传统方式：**
```cpp
odb::transaction t(db.begin());
try {
    // 数据库操作
    db.persist(user);
    t.commit();  // 手动提交
} catch (...) {
    t.rollback();  // 手动回滚
    throw;
}
```

**RAII封装后：**
```cpp
{
    auto tx = conn.begin_transaction();
    // 数据库操作
    conn.persist(user);
    tx.commit();  // 显式提交
}  // 析构时自动回滚（如果没有提交）
```

**优势：**
1. **异常安全**：即使抛异常也会自动回滚
2. **代码简洁**：减少手动资源管理代码
3. **防止遗忘**：不会忘记释放资源

### 6.3 连接RAII封装

**封装前：**
```cpp
auto conn = pool.get_connection();
try {
    // 使用连接
    conn->persist(user);
    pool.release_connection(conn);  // 手动归还
} catch (...) {
    pool.release_connection(conn);  // 异常时也要归还
    throw;
}
```

**封装后：**
```cpp
{
    auto conn = manager.get_connection();
    // 使用连接
    conn->persist(user);
}  // 析构时自动归还连接池
```

## 7. 操作方法封装解析

### 7.1 基础CRUD操作

#### 7.1.1 persist() - 持久化新对象
```cpp
template<typename T>
typename odb::object_traits<T>::id_type persist(T& obj);
```

**作用：**
- 将C++对象保存到数据库
- 自动分配主键（如果是auto类型）
- 返回对象的主键值

**使用场景：**
- 创建新用户
- 添加新消息记录
- 插入任何新数据

#### 7.1.2 update() - 更新对象
```cpp
template<typename T>
void update(const T& obj);
```

**作用：**
- 更新数据库中的现有对象
- 根据主键找到对应记录并更新

**使用场景：**
- 修改用户信息
- 更新消息状态
- 任何数据修改操作

#### 7.1.3 load()/find() - 加载对象
```cpp
template<typename T>
std::shared_ptr<T> load(const typename odb::object_traits<T>::id_type& id);

template<typename T>
std::shared_ptr<T> find(const typename odb::object_traits<T>::id_type& id);
```

**区别：**
- **load()**: 对象不存在时抛异常
- **find()**: 对象不存在时返回nullptr

**使用场景：**
- 根据用户ID获取用户信息
- 根据消息ID获取消息内容

#### 7.1.4 erase() - 删除对象
```cpp
template<typename T>
void erase(const T& obj);

template<typename T>
void erase(const typename odb::object_traits<T>::id_type& id);
```

**作用：**
- 从数据库删除对象
- 支持按对象或按ID删除

### 7.2 查询操作封装

#### 7.2.1 query() - 通用查询
```cpp
template<typename T>
odb::result<T> query(const odb::query<T>& q = odb::query<T>());
```

**作用：**
- 执行复杂查询
- 返回结果集合

**示例：**
```cpp
// 查询所有在线用户
auto users = conn.query<User>(odb::query<User>::online == true);
```

#### 7.2.2 query_one() - 单一结果查询
```cpp
template<typename T>
std::shared_ptr<T> query_one(const odb::query<T>& q);
```

**作用：**
- 期望单一结果的查询
- 多个结果时抛异常
- 无结果时返回nullptr

**使用场景：**
- 根据账号查找用户
- 根据唯一条件查询

### 7.3 高级操作封装

#### 7.3.1 execute_in_transaction() - 事务操作
```cpp
template<typename Func>
auto execute_in_transaction(Func&& func) -> decltype(func(*this));
```

**作用：**
- 在事务中执行一系列操作
- 自动提交或回滚
- 异常安全

**示例：**
```cpp
conn.execute_in_transaction([&](auto& conn) {
    // 创建用户
    User user;
    user.set_name("张三");
    auto user_id = conn.persist(user);
    
    // 创建用户配置
    UserConfig config;
    config.set_user_id(user_id);
    conn.persist(config);
    
    return user_id;
});
```

#### 7.3.2 safe_execute() - 安全执行
```cpp
template<typename Func, typename DefaultType>
auto safe_execute(Func&& func, DefaultType&& default_value);
```

**作用：**
- 执行操作并捕获异常
- 异常时返回默认值
- 记录错误日志

**使用场景：**
- 不确定操作是否会成功的场景
- 需要降级处理的操作

## 8. 使用层次和场景

### 8.1 三个使用层次

#### 8.1.1 低级API (直接使用连接)
```cpp
auto conn = manager.get_connection();
auto tx = conn->begin_transaction();
conn->persist(user);
tx.commit();
```

**适用场景：**
- 需要精确控制事务边界
- 复杂的多步操作
- 性能敏感的操作

#### 8.1.2 中级API (execute系列)
```cpp
manager.execute([&](auto& conn) {
    conn.persist(user);
});
```

**适用场景：**
- 大部分常规操作
- 平衡了便捷性和控制力
- 推荐的默认选择

#### 8.1.3 高级API (safe_execute系列)
```cpp
auto result = manager.safe_execute([&](auto& conn) {
    return conn.find<User>(user_id);
}, std::shared_ptr<User>{});
```

**适用场景：**
- 不确定是否成功的操作
- 需要降级处理的场景
- 对稳定性要求高的场景

### 8.2 实际使用场景

#### 8.2.1 用户注册场景
```cpp
// 需要事务保证数据一致性
bool register_user(const std::string& account, const std::string& password) {
    return manager.safe_execute_transaction([&](auto& conn) {
        // 检查账号是否存在
        auto existing = conn.query_one<User>(
            odb::query<User>::account == account);
        if (existing) {
            throw std::runtime_error("Account already exists");
        }
        
        // 创建用户
        User user;
        user.set_account(account);
        user.set_password_hash(hash_password(password));
        auto user_id = conn.persist(user);
        
        // 创建默认配置
        UserConfig config;
        config.set_user_id(user_id);
        conn.persist(config);
        
        return true;
    }, false);  // 失败时返回false
}
```

#### 8.2.2 消息查询场景
```cpp
// 简单查询，使用中级API
std::vector<Message> get_recent_messages(uint64_t user_id, int limit) {
    return manager.execute([&](auto& conn) {
        auto results = conn.query<Message>(
            odb::query<Message>::receiver_id == user_id +
            " ORDER BY create_time DESC LIMIT " + std::to_string(limit));
        
        std::vector<Message> messages;
        for (const auto& msg : results) {
            messages.push_back(msg);
        }
        return messages;
    });
}
```

## 9. 总结

### 9.1 封装的核心价值

1. **简化开发**：从复杂的资源管理中解放开发者
2. **提高安全性**：RAII确保资源正确释放，避免内存泄漏
3. **提升性能**：连接池复用，减少连接开销
4. **增强可维护性**：统一的错误处理和日志记录
5. **降低错误率**：类型安全，编译时检查

### 9.2 使用建议

1. **配置调优**：根据实际负载调整连接池大小
2. **选择合适的API层次**：根据场景选择不同的抽象级别
3. **事务边界**：合理设计事务边界，避免长事务
4. **错误处理**：在关键路径使用safe_execute系列方法
5. **监控日志**：关注连接池状态和数据库操作日志

这个封装设计的目标是让你能够专注于业务逻辑，而不是数据库操作的细节。通过RAII和连接池，确保高效且安全的数据库操作。