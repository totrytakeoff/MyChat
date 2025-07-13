#include <mutex>
#include <memory>

// 单例模板类，支持线程安全的懒汉式单例实现
// 用法：class MyClass : public Singleton<MyClass> { friend class Singleton<MyClass>; ... };
template <typename T>
class Singleton
{
public:
    // 获取单例实例，返回 std::shared_ptr<T>
    // 首次调用时线程安全地创建实例，后续直接返回已创建的实例
    static std::shared_ptr<T> getInstance()
    {
        // std::once_flag 保证只初始化一次
        static std::once_flag flag;
        // std::call_once 保证 lambda 只被执行一次，实现线程安全的懒加载
        std::call_once(flag, [&]() {
            // 创建 T 的实例并赋值给静态成员 instance
            instance = std::shared_ptr<T>(new T());
        });

        // 返回单例实例
        return instance;
    }

protected:
    // 静态成员，保存单例实例
    static std::shared_ptr<T> instance;
    // 构造函数和析构函数设为 protected，防止外部直接构造和析构
    Singleton() = default;
    ~Singleton() = default;
    // 禁止拷贝构造和赋值，防止多实例
    Singleton(const Singleton<T>&) = delete;
    Singleton& operator=(const Singleton<T>& st) = delete;
};

// 静态成员初始化，初始为 nullptr
// 必须在类外定义模板静态成员
template <typename T>
std::shared_ptr<T> Singleton<T>::instance = nullptr;

