#pragma once
#include <atomic>

template<typename T>
class AtomicWaitObject {
public:
    AtomicWaitObject(T v) : data(v) {}

    void wait(T desired) const noexcept {
        // 只要值 != desired 就睡觉，内部用 futex/WaitOnAddress
        T v = data.load(std::memory_order_acquire);
        while (v != desired)
        {
            std::atomic_wait(&data, v); // 阻塞，直到被 notify
            v = data.load(std::memory_order_acquire); // memory_order_acquire 保证之后的读操作能看到之前线程的写操作
        }
    }

    void notifyOne() noexcept {
        std::atomic_notify_one(&data); // 唤醒一个等待者
    }
    void notifyAll() noexcept {
        std::atomic_notify_all(&data); // 唤醒全部
    }

    void set(T v) noexcept {
        data.store(v, std::memory_order_release); // memory_order_release 保证之前的写操作对其他线程可见
    }

    T get() const noexcept {
        return data.load(std::memory_order_acquire); // memory_order_acquire 保证之后的读操作能看到之前线程的写操作
    }

    void setAndNotifyAll(T v) noexcept {
        set(v); // 设置新值
        notifyAll(); // 改完立即通知
    }
    void setAndNotifyOne(T v) noexcept {
        set(v); // 设置新值
        notifyOne(); // 改完立即通知
    }

private:
    alignas(64) std::atomic<T> data; // 64字节对齐
};
