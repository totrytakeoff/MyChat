#include <gtest/gtest.h>
#include "../../common/utils/memory_pool.hpp"
#include <vector>
#include <algorithm>


using namespace im::utils;


// 用于测试的简单结构体
typedef struct DummyObj {
    int value;
    DummyObj() : value(0) {}
    DummyObj(int v) : value(v) {}
} DummyObj;

// 1. 基本用法演示和测试
TEST(MemoryPoolTest, BasicAllocateDeallocate) {
    // 创建一个池，最多可容纳10个DummyObj
    MemoryPool<DummyObj> pool(10);
    std::vector<DummyObj*> ptrs;
    // 分配10个对象
    for (int i = 0; i < 10; ++i) {
        DummyObj* obj = pool.Allocate();
        obj->value = i;
        ptrs.push_back(obj);
    }
    // 池已满，再分配会抛异常
    EXPECT_THROW(pool.Allocate(), std::bad_alloc);
    // 检查内容
    for (int i = 0; i < 10; ++i) {
        EXPECT_EQ(ptrs[i]->value, i);
    }
    // 释放所有对象
    for (auto p : ptrs) {
        pool.Deallocate(p);
    }
    // 现在池又可用10个对象
    EXPECT_EQ(pool.GetFreeCount(), 10);
}

// 2. 多次分配/释放，池可复用
TEST(MemoryPoolTest, ReuseAfterDeallocate) {
    MemoryPool<DummyObj> pool(5);
    DummyObj* a = pool.Allocate();
    DummyObj* b = pool.Allocate();
    pool.Deallocate(a);
    pool.Deallocate(b);
    // 再分配应不抛异常
    EXPECT_NO_THROW(pool.Allocate());
    EXPECT_NO_THROW(pool.Allocate());
}

// 3. 多线程安全性测试（简单并发压力）
#include <thread>
TEST(MemoryPoolTest, ThreadSafety) {
    MemoryPool<DummyObj> pool(100);
    std::vector<DummyObj*> ptrs;
    std::mutex vec_mutex;
    auto alloc_task = [&]() {
        for (int i = 0; i < 25; ++i) {
            DummyObj* obj = nullptr;
            EXPECT_NO_THROW(obj = pool.Allocate());
            if (obj) {
                std::lock_guard<std::mutex> lock(vec_mutex);
                ptrs.push_back(obj);
            }
        }
    };
    std::thread t1(alloc_task), t2(alloc_task), t3(alloc_task), t4(alloc_task);
    t1.join(); t2.join(); t3.join(); t4.join();
    EXPECT_EQ(ptrs.size(), 100);
    // 归还所有对象
    for (auto p : ptrs) pool.Deallocate(p);
    EXPECT_EQ(pool.GetFreeCount(), 100);
}

/*
==================
内存池使用方法总结：
==================
1. 创建池：
   MemoryPool<MyType> pool(1024); // 1024个MyType对象
2. 分配对象：
   MyType* obj = pool.Allocate();
   // 使用 obj ...
3. 释放对象：
   pool.Deallocate(obj);
4. 可用 GetFreeCount()/GetTotalCount() 监控池状态
5. 多线程环境下直接用，无需额外加锁
*/ 