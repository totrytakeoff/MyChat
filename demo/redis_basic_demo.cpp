#include <sw/redis++/redis.h>
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <thread>

using namespace sw::redis;

void demo_connection() {
    std::cout << "\n=== 1. Redis 连接演示 ===" << std::endl;
    
    try {
        // 方式1: 通过URI连接
        Redis redis1("tcp://127.0.0.1:6379");
        std::cout << "✅ URI连接成功" << std::endl;
        
        // 方式2: 通过ConnectionOptions连接
        ConnectionOptions opts;
        opts.host = "127.0.0.1";
        opts.port = 6379;
        opts.password = "";
        opts.db = 0;
        opts.connect_timeout = std::chrono::milliseconds(100);
        opts.socket_timeout = std::chrono::milliseconds(100);
        
        Redis redis2(opts);
        std::cout << "✅ Options连接成功" << std::endl;
        
        // 测试连接
        redis1.ping();
        std::cout << "✅ PING 成功" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 连接失败: " << e.what() << std::endl;
    }
}

void demo_string_operations() {
    std::cout << "\n=== 2. 字符串操作演示 ===" << std::endl;
    
    try {
        Redis redis("tcp://127.0.0.1:6379");
        
        // SET/GET基础操作
        redis.set("key1", "Hello Redis!");
        auto value = redis.get("key1");
        if (value) {
            std::cout << "GET key1: " << *value << std::endl;
        }
        
        // 设置带过期时间的键
        redis.setex("temp_key", 5, "This will expire in 5 seconds");
        std::cout << "✅ 设置5秒过期的键" << std::endl;
        
        // 批量设置
        std::unordered_map<std::string, std::string> kv_pairs = {
            {"user:1001", "john_doe"},
            {"user:1002", "jane_smith"},
            {"user:1003", "bob_wilson"}
        };
        redis.mset(kv_pairs.begin(), kv_pairs.end());
        std::cout << "✅ 批量设置用户" << std::endl;
        
        // 批量获取
        std::vector<std::string> keys = {"user:1001", "user:1002", "user:1003"};
        std::vector<OptionalString> values;
        redis.mget(keys.begin(), keys.end(), std::back_inserter(values));
        
        std::cout << "批量获取结果:" << std::endl;
        for (size_t i = 0; i < keys.size(); ++i) {
            if (values[i]) {
                std::cout << "  " << keys[i] << " = " << *values[i] << std::endl;
            }
        }
        
        // 原子操作
        auto counter = redis.incr("counter");
        std::cout << "计数器值: " << counter << std::endl;
        
        redis.incrby("counter", 10);
        counter = redis.get("counter");
        if (counter) {
            std::cout << "增加10后: " << *counter << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 字符串操作失败: " << e.what() << std::endl;
    }
}

void demo_hash_operations() {
    std::cout << "\n=== 3. 哈希操作演示 ===" << std::endl;
    
    try {
        Redis redis("tcp://127.0.0.1:6379");
        
        // 单个字段操作
        redis.hset("user:1001:profile", "name", "John Doe");
        redis.hset("user:1001:profile", "email", "john@example.com");
        redis.hset("user:1001:profile", "age", "30");
        redis.hset("user:1001:profile", "city", "New York");
        
        // 获取单个字段
        auto name = redis.hget("user:1001:profile", "name");
        if (name) {
            std::cout << "用户姓名: " << *name << std::endl;
        }
        
        // 批量设置字段
        std::unordered_map<std::string, std::string> user_data = {
            {"department", "Engineering"},
            {"position", "Senior Developer"},
            {"salary", "80000"},
            {"start_date", "2020-01-15"}
        };
        redis.hmset("user:1001:profile", user_data.begin(), user_data.end());
        std::cout << "✅ 批量设置用户档案" << std::endl;
        
        // 获取所有字段
        std::unordered_map<std::string, std::string> all_fields;
        redis.hgetall("user:1001:profile", std::inserter(all_fields, all_fields.end()));
        
        std::cout << "用户完整档案:" << std::endl;
        for (const auto& [field, value] : all_fields) {
            std::cout << "  " << field << ": " << value << std::endl;
        }
        
        // 检查字段是否存在
        bool has_email = redis.hexists("user:1001:profile", "email");
        std::cout << "是否有邮箱字段: " << (has_email ? "是" : "否") << std::endl;
        
        // 获取字段数量
        auto field_count = redis.hlen("user:1001:profile");
        std::cout << "字段总数: " << field_count << std::endl;
        
        // 删除字段
        redis.hdel("user:1001:profile", "salary");
        std::cout << "✅ 删除salary字段" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 哈希操作失败: " << e.what() << std::endl;
    }
}

void demo_set_operations() {
    std::cout << "\n=== 4. 集合操作演示 ===" << std::endl;
    
    try {
        Redis redis("tcp://127.0.0.1:6379");
        
        // 添加成员到集合
        redis.sadd("user:1001:devices", "android_phone");
        redis.sadd("user:1001:devices", "iphone");
        redis.sadd("user:1001:devices", "windows_pc");
        redis.sadd("user:1001:devices", "mac_laptop");
        
        // 批量添加
        std::vector<std::string> more_devices = {"ipad", "chrome_browser"};
        redis.sadd("user:1001:devices", more_devices.begin(), more_devices.end());
        
        // 获取所有成员
        std::unordered_set<std::string> devices;
        redis.smembers("user:1001:devices", std::inserter(devices, devices.end()));
        
        std::cout << "用户设备列表:" << std::endl;
        for (const auto& device : devices) {
            std::cout << "  " << device << std::endl;
        }
        
        // 检查成员是否存在
        bool has_iphone = redis.sismember("user:1001:devices", "iphone");
        std::cout << "是否有iPhone: " << (has_iphone ? "是" : "否") << std::endl;
        
        // 获取集合大小
        auto device_count = redis.scard("user:1001:devices");
        std::cout << "设备总数: " << device_count << std::endl;
        
        // 随机获取成员
        auto random_device = redis.srandmember("user:1001:devices");
        if (random_device) {
            std::cout << "随机设备: " << *random_device << std::endl;
        }
        
        // 创建另一个用户的设备集合
        redis.sadd("user:1002:devices", "android_phone");
        redis.sadd("user:1002:devices", "windows_pc");
        redis.sadd("user:1002:devices", "linux_desktop");
        
        // 集合交集
        std::unordered_set<std::string> common_devices;
        redis.sinter({"user:1001:devices", "user:1002:devices"}, 
                    std::inserter(common_devices, common_devices.end()));
        
        std::cout << "共同设备:" << std::endl;
        for (const auto& device : common_devices) {
            std::cout << "  " << device << std::endl;
        }
        
        // 移除成员
        redis.srem("user:1001:devices", "chrome_browser");
        std::cout << "✅ 移除chrome_browser" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 集合操作失败: " << e.what() << std::endl;
    }
}

void demo_list_operations() {
    std::cout << "\n=== 5. 列表操作演示 ===" << std::endl;
    
    try {
        Redis redis("tcp://127.0.0.1:6379");
        
        // 从左侧推入
        redis.lpush("user:1001:login_history", "2025-01-08 10:30:00");
        redis.lpush("user:1001:login_history", "2025-01-08 09:15:00");
        redis.lpush("user:1001:login_history", "2025-01-07 18:45:00");
        
        // 从右侧推入
        redis.rpush("user:1001:login_history", "2025-01-08 11:20:00");
        
        // 获取列表长度
        auto history_count = redis.llen("user:1001:login_history");
        std::cout << "登录历史记录数: " << history_count << std::endl;
        
        // 获取范围内的元素
        std::vector<std::string> recent_logins;
        redis.lrange("user:1001:login_history", 0, 2, std::back_inserter(recent_logins));
        
        std::cout << "最近3次登录:" << std::endl;
        for (const auto& login_time : recent_logins) {
            std::cout << "  " << login_time << std::endl;
        }
        
        // 获取指定索引的元素
        auto latest_login = redis.lindex("user:1001:login_history", 0);
        if (latest_login) {
            std::cout << "最新登录时间: " << *latest_login << std::endl;
        }
        
        // 修剪列表（保留最新的5条记录）
        redis.ltrim("user:1001:login_history", 0, 4);
        std::cout << "✅ 保留最新5条登录记录" << std::endl;
        
        // 阻塞弹出（模拟消息队列）
        redis.lpush("message_queue", "task1");
        redis.lpush("message_queue", "task2");
        redis.lpush("message_queue", "task3");
        
        // 非阻塞弹出
        auto task = redis.rpop("message_queue");
        if (task) {
            std::cout << "处理任务: " << *task << std::endl;
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 列表操作失败: " << e.what() << std::endl;
    }
}

void demo_sorted_set_operations() {
    std::cout << "\n=== 6. 有序集合操作演示 ===" << std::endl;
    
    try {
        Redis redis("tcp://127.0.0.1:6379");
        
        // 添加成员和分数
        redis.zadd("user_scores", "user1", 100);
        redis.zadd("user_scores", "user2", 85);
        redis.zadd("user_scores", "user3", 92);
        redis.zadd("user_scores", "user4", 78);
        redis.zadd("user_scores", "user5", 95);
        
        // 批量添加
        std::unordered_map<std::string, double> more_scores = {
            {"user6", 88.5},
            {"user7", 91.2},
            {"user8", 82.7}
        };
        redis.zadd("user_scores", more_scores.begin(), more_scores.end());
        
        // 获取排名范围内的成员（按分数升序）
        std::vector<std::string> top_users;
        redis.zrevrange("user_scores", 0, 2, std::back_inserter(top_users));
        
        std::cout << "前3名用户:" << std::endl;
        for (size_t i = 0; i < top_users.size(); ++i) {
            auto score = redis.zscore("user_scores", top_users[i]);
            if (score) {
                std::cout << "  " << (i+1) << ". " << top_users[i] 
                         << " (分数: " << *score << ")" << std::endl;
            }
        }
        
        // 获取成员的排名
        auto user1_rank = redis.zrevrank("user_scores", "user1");
        if (user1_rank) {
            std::cout << "user1排名: " << (*user1_rank + 1) << std::endl;
        }
        
        // 按分数范围查询
        std::vector<std::string> good_users;
        redis.zrangebyscore("user_scores", 90, 100, std::back_inserter(good_users));
        
        std::cout << "90分以上用户:" << std::endl;
        for (const auto& user : good_users) {
            auto score = redis.zscore("user_scores", user);
            if (score) {
                std::cout << "  " << user << " (分数: " << *score << ")" << std::endl;
            }
        }
        
        // 增加分数
        auto new_score = redis.zincrby("user_scores", "user2", 10);
        std::cout << "user2增加10分后: " << new_score << std::endl;
        
        // 获取集合大小
        auto total_users = redis.zcard("user_scores");
        std::cout << "总用户数: " << total_users << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 有序集合操作失败: " << e.what() << std::endl;
    }
}

void demo_expiration_operations() {
    std::cout << "\n=== 7. 过期时间操作演示 ===" << std::endl;
    
    try {
        Redis redis("tcp://127.0.0.1:6379");
        
        // 设置键值对
        redis.set("session:abc123", "user_data");
        
        // 设置过期时间（秒）
        redis.expire("session:abc123", 30);
        std::cout << "✅ 设置session 30秒后过期" << std::endl;
        
        // 查看剩余时间
        auto ttl = redis.ttl("session:abc123");
        std::cout << "session剩余时间: " << ttl << "秒" << std::endl;
        
        // 设置精确的过期时间戳
        auto future_time = std::chrono::system_clock::now() + std::chrono::minutes(5);
        auto timestamp = std::chrono::duration_cast<std::chrono::seconds>(
            future_time.time_since_epoch()).count();
        
        redis.set("future_key", "future_value");
        redis.expireat("future_key", timestamp);
        std::cout << "✅ 设置5分钟后过期" << std::endl;
        
        // 取消过期时间
        redis.set("permanent_key", "permanent_value");
        redis.expire("permanent_key", 10);
        redis.persist("permanent_key");
        std::cout << "✅ 取消permanent_key的过期时间" << std::endl;
        
        // 设置毫秒级过期时间
        redis.set("quick_key", "quick_value");
        redis.pexpire("quick_key", 2000);  // 2秒
        std::cout << "✅ 设置2000毫秒后过期" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 过期时间操作失败: " << e.what() << std::endl;
    }
}

void demo_pipeline_operations() {
    std::cout << "\n=== 8. 管道操作演示 ===" << std::endl;
    
    try {
        Redis redis("tcp://127.0.0.1:6379");
        
        // 普通操作（多次网络往返）
        auto start = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            redis.set("normal:" + std::to_string(i), "value" + std::to_string(i));
        }
        auto end = std::chrono::high_resolution_clock::now();
        auto normal_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        std::cout << "普通操作100次SET耗时: " << normal_duration.count() << "微秒" << std::endl;
        
        // 管道操作（批量执行，减少网络往返）
        start = std::chrono::high_resolution_clock::now();
        auto pipe = redis.pipeline();
        for (int i = 0; i < 100; ++i) {
            pipe.set("pipeline:" + std::to_string(i), "value" + std::to_string(i));
        }
        auto replies = pipe.exec();
        end = std::chrono::high_resolution_clock::now();
        auto pipeline_duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        
        std::cout << "管道操作100次SET耗时: " << pipeline_duration.count() << "微秒" << std::endl;
        std::cout << "性能提升: " << (double)normal_duration.count() / pipeline_duration.count() 
                  << "倍" << std::endl;
        
        // 复杂管道操作
        auto complex_pipe = redis.pipeline();
        complex_pipe.set("user:1001", "john");
        complex_pipe.hset("user:1001:profile", "name", "John Doe");
        complex_pipe.sadd("online_users", "1001");
        complex_pipe.zadd("user_scores", "1001", 100);
        complex_pipe.lpush("user:1001:notifications", "Welcome!");
        
        auto complex_replies = complex_pipe.exec();
        std::cout << "✅ 复杂管道操作完成，执行了 " << complex_replies.size() << " 个命令" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 管道操作失败: " << e.what() << std::endl;
    }
}

void demo_transaction_operations() {
    std::cout << "\n=== 9. 事务操作演示 ===" << std::endl;
    
    try {
        Redis redis("tcp://127.0.0.1:6379");
        
        // 初始化账户余额
        redis.set("account:1001", "1000");
        redis.set("account:1002", "500");
        
        // 转账事务（WATCH + MULTI + EXEC）
        auto tx = redis.transaction("account:1001");
        
        // 检查账户1001余额
        auto balance_str = redis.get("account:1001");
        if (balance_str) {
            int balance = std::stoi(*balance_str);
            if (balance >= 100) {
                // 开始事务
                tx.multi();
                tx.decrby("account:1001", 100);  // 扣除100
                tx.incrby("account:1002", 100);  // 增加100
                
                auto results = tx.exec();
                if (!results.empty()) {
                    std::cout << "✅ 转账成功！账户1001转给账户1002 100元" << std::endl;
                    
                    // 查看转账后余额
                    auto balance1 = redis.get("account:1001");
                    auto balance2 = redis.get("account:1002");
                    if (balance1 && balance2) {
                        std::cout << "账户1001余额: " << *balance1 << std::endl;
                        std::cout << "账户1002余额: " << *balance2 << std::endl;
                    }
                } else {
                    std::cout << "❌ 转账失败！可能是并发冲突" << std::endl;
                }
            } else {
                std::cout << "❌ 余额不足" << std::endl;
            }
        }
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 事务操作失败: " << e.what() << std::endl;
    }
}

void demo_pub_sub_operations() {
    std::cout << "\n=== 10. 发布订阅演示 ===" << std::endl;
    
    try {
        Redis redis("tcp://127.0.0.1:6379");
        Redis publisher("tcp://127.0.0.1:6379");
        
        std::cout << "注意：发布订阅需要在独立线程中运行订阅者" << std::endl;
        
        // 发布消息
        auto subscribers = publisher.publish("user:notifications", "New message arrived!");
        std::cout << "发送通知给 " << subscribers << " 个订阅者" << std::endl;
        
        subscribers = publisher.publish("system:alerts", "System maintenance in 10 minutes");
        std::cout << "发送系统警告给 " << subscribers << " 个订阅者" << std::endl;
        
        // 模式发布
        subscribers = publisher.publish("user:1001:messages", "You have a new friend request");
        std::cout << "发送用户消息给 " << subscribers << " 个订阅者" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "❌ 发布订阅操作失败: " << e.what() << std::endl;
    }
}

int main() {
    std::cout << "🔴 Redis-plus-plus 基础操作演示" << std::endl;
    std::cout << "请确保Redis服务已启动在 127.0.0.1:6379" << std::endl;
    
    try {
        demo_connection();
        demo_string_operations();
        demo_hash_operations();
        demo_set_operations();
        demo_list_operations();
        demo_sorted_set_operations();
        demo_expiration_operations();
        demo_pipeline_operations();
        demo_transaction_operations();
        demo_pub_sub_operations();
        
        std::cout << "\n🎉 所有演示完成！" << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "💥 演示过程中出现错误: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}