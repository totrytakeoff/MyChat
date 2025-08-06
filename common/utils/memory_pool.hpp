#ifndef MEMORY_POOL_HPP
#define MEMORY_POOL_HPP

#include <vector>
#include <mutex>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace im {
namespace utils {

/**
 * @brief 通用线程安全固定大小对象内存池
 *
 * 用法：
 *   MemoryPool<MyType> pool(1024);
 *   MyType* obj = pool.Allocate();
 *   ...
 *   pool.Deallocate(obj);
 *
 * 适合高频分配/释放的小对象，如消息、缓冲区、事件等。
 */
template <typename T>
class MemoryPool {
public:
    explicit MemoryPool(size_t poolSize = 1024)
        : m_freeList(nullptr), m_poolSize(poolSize) {
        if (poolSize == 0) throw std::invalid_argument("poolSize must be > 0");
        m_buffer.resize(poolSize * sizeof(T));
        // 构建空闲链表
        for (size_t i = 0; i < poolSize; ++i) {
            Node* node = reinterpret_cast<Node*>(&m_buffer[i * sizeof(T)]);
            node->next = m_freeList;
            m_freeList = node;
        }
    }

    ~MemoryPool() {
        // 不需要手动 delete，每个 T 的析构由用户负责
    }

    T* Allocate() {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_freeList) {
            throw std::bad_alloc();
        }
        Node* node = m_freeList;
        m_freeList = node->next;
        return reinterpret_cast<T*>(node);
    }

    void Deallocate(T* ptr) {
        std::lock_guard<std::mutex> lock(m_mutex);
        Node* node = reinterpret_cast<Node*>(ptr);
        node->next = m_freeList;
        m_freeList = node;
    }

    size_t GetFreeCount() const {
        std::lock_guard<std::mutex> lock(m_mutex);
        size_t count = 0;
        Node* cur = m_freeList;
        while (cur) {
            ++count;
            cur = cur->next;
        }
        return count;
    }

    size_t GetTotalCount() const { return m_poolSize; }

private:
    struct Node { Node* next; };
    Node* m_freeList;
    std::vector<uint8_t> m_buffer;
    size_t m_poolSize;
    mutable std::mutex m_mutex;
};

} // namespace utils
} // namespace im

#endif // MEMORY_POOL_HPP