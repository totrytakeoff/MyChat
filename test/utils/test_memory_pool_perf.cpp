#include <gtest/gtest.h>
#include "../../common/utils/memory_pool.hpp"
#include <chrono>
#include <vector>
#include <iostream>

struct PerfObj {
    int a, b, c, d;
    PerfObj() : a(0), b(0), c(0), d(0) {}
};

TEST(MemoryPoolPerfTest, CompareWithNewDelete) {
    constexpr size_t N = 1000000;
    std::vector<PerfObj*> ptrs(N);

    // 1. 普通 new/delete
    auto t1 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        ptrs[i] = new PerfObj();
    }
    for (size_t i = 0; i < N; ++i) {
        delete ptrs[i];
    }
    auto t2 = std::chrono::high_resolution_clock::now();
    auto newdel_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    // 2. 内存池分配/释放
    MemoryPool<PerfObj> pool(N);
    auto t3 = std::chrono::high_resolution_clock::now();
    for (size_t i = 0; i < N; ++i) {
        ptrs[i] = pool.Allocate();
    }
    for (size_t i = 0; i < N; ++i) {
        pool.Deallocate(ptrs[i]);
    }
    auto t4 = std::chrono::high_resolution_clock::now();
    auto pool_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t4 - t3).count();

    std::cout << "[性能对比] new/delete: " << newdel_ms << " ms, MemoryPool: " << pool_ms << " ms\n";
    EXPECT_LT(pool_ms, newdel_ms); // 内存池应更快
} 