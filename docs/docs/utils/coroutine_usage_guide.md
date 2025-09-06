# 协程管理器使用文档

## 1. 协程基础概念

### 1.1 什么是协程？
协程（Coroutine）是一种比线程更轻量级的并发执行单元。与线程不同，协程的调度是由程序员控制的，而不是操作系统。协程可以在执行过程中挂起，并在稍后恢复执行，而不会阻塞其他协程或线程。

### 1.2 协程的优势
- **轻量级**：协程的创建和切换开销远小于线程
- **无锁编程**：单线程内的协程不需要考虑数据竞争
- **高效并发**：可以在少量线程上运行大量协程
- **简化异步编程**：使用同步风格编写异步代码

### 1.3 C++20协程核心概念
- **协程函数**：使用`co_await`、`co_return`或`co_yield`的函数
- **协程句柄**：`std::coroutine_handle`，用于控制协程的执行
- **承诺类型**：`promise_type`，定义协程的行为
- **等待者**：实现`await_ready`、`await_suspend`、`await_resume`的对象

## 2. 协程管理器组件详解

### 2.1 Task协程任务类型
`Task<T>`是协程管理器的核心类型，它包装了一个协程并提供统一的接口。

```cpp
// 定义一个协程函数
Task<int> async_compute(int x, int y) {
    // 模拟异步计算
    auto future = std::async(std::launch::async, [x, y]() {
        std::this_thread::sleep_for(100ms); // 模拟耗时操作
        return x + y;
    });
    
    // 等待异步操作完成
    int result = co_await future;
    co_return result;
}
```

### 2.2 Awaitable包装器
协程管理器提供了多种Awaitable包装器，使异步操作可以在协程中使用：

```cpp
// 包装std::future
Task<std::string> fetch_data() {
    auto future = std::async(std::launch::async, []() {
        // 模拟网络请求
        std::this_thread::sleep_for(200ms);
        return std::string("data");
    });
    
    std::string data = co_await future;
    co_return data;
}

// 使用延迟操作
Task<void> delayed_operation() {
    std::cout << "开始延迟操作\n";
    co_await 1000ms; // 延迟1秒
    std::cout << "延迟操作完成\n";
}
```

### 2.3 协程调度器
协程管理器集成了线程池，可以将协程调度到不同的线程执行：

```cpp
// 调度协程执行
auto task = async_compute(10, 20);
CoroutineManager::getInstance().schedule(std::move(task));
```

## 3. 实际应用示例

### 3.1 异步消息处理
在即时通讯系统中，可以使用协程来处理消息：

```cpp
Task<bool> authenticate_user(const std::string& token) {
    // 模拟异步认证
    auto future = std::async(std::launch::async, [token]() {
        // 实际认证逻辑
        return token == "valid_token";
    });
    
    bool authenticated = co_await future;
    co_return authenticated;
}

Task<void> process_message(const Message& msg) {
    // 异步认证
    if (!co_await authenticate_user(msg.token())) {
        std::cout << "认证失败\n";
        co_return;
    }
    
    // 延迟处理
    co_await 500ms;
    
    // 处理消息
    std::cout << "处理消息: " << msg.content() << "\n";
}
```

### 3.2 并发操作
协程可以轻松实现并发操作：

```cpp
Task<int> fetch_user_id() {
    auto future = std::async(std::launch::async, []() {
        std::this_thread::sleep_for(100ms);
        return 12345;
    });
    co_return co_await future;
}

Task<std::string> fetch_user_name() {
    auto future = std::async(std::launch::async, []() {
        std::this_thread::sleep_for(150ms);
        return std::string("Alice");
    });
    co_return co_await future;
}

Task<void> fetch_user_info() {
    // 并发获取用户ID和姓名
    auto id_task = fetch_user_id();
    auto name_task = fetch_user_name();
    
    // 等待两个操作都完成
    int id = co_await id_task;
    std::string name = co_await name_task;
    
    std::cout << "用户信息: " << name << "(" << id << ")\n";
}
```

## 4. 错误处理

协程中的异常处理与普通函数类似：

```cpp
Task<int> risky_operation() {
    try {
        // 可能抛出异常的操作
        if (rand() % 2) {
            throw std::runtime_error("操作失败");
        }
        co_return 42;
    } catch (const std::exception& e) {
        std::cout << "捕获异常: " << e.what() << "\n";
        co_return -1;
    }
}

Task<void> handle_risky_operation() {
    try {
        int result = co_await risky_operation();
        if (result < 0) {
            std::cout << "操作失败\n";
        } else {
            std::cout << "操作成功: " << result << "\n";
        }
    } catch (const std::exception& e) {
        std::cout << "未处理异常: " << e.what() << "\n";
    }
}
```

## 5. 性能优化建议

1. **合理使用线程池**：避免为每个协程创建新线程
2. **减少协程切换**：避免不必要的`co_await`操作
3. **批量操作**：将多个小操作合并为一个协程
4. **异常处理**：合理使用异常处理避免性能损失

## 6. 注意事项

1. **生命周期管理**：确保协程对象在使用期间保持有效
2. **资源释放**：协程结束时正确释放资源
3. **线程安全**：在多线程环境中注意数据竞争
4. **异常安全**：正确处理协程中的异常情况