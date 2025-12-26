#pragma once
#include <chrono>
#include <thread>



//template <class _Rep, class _Period>
//inline void SleepFor(const std::chrono::duration<_Rep, _Period>& duration)
//{
//    auto to = std::chrono::steady_clock::now() + duration;
//    while (std::chrono::steady_clock::now() < to)
//        std::this_thread::yield();  // 让出CPU时间片，降低资源占用
//}
//
////inline void SleepForNS(std::chrono::milliseconds duration)
////{
////    auto to = std::chrono::steady_clock::now() + duration;
////    while (std::chrono::steady_clock::now() < to)
////        std::this_thread::yield();  // 让出CPU时间片，降低资源占用
////}
////
////inline void SleepForUs(std::chrono::microseconds duration)
////{
////    auto tp = std::chrono::high_resolution_clock::now() + duration;
////    while (std::chrono::high_resolution_clock::now() < tp)
////        std::this_thread::yield();  // 让出CPU时间片，降低资源占用
////}
//
//inline void SleepForMs(size_t duration)
//{
//    auto to = std::chrono::steady_clock::now() + std::chrono::milliseconds(duration);
//    while (std::chrono::steady_clock::now() < to)
//        std::this_thread::yield();  // 让出CPU时间片，降低资源占用
//}
//
//
//inline void SleepForUs(size_t duration)
//{
//    auto tp = std::chrono::high_resolution_clock::now() + std::chrono::microseconds(duration);
//    while (std::chrono::high_resolution_clock::now() < tp)
//        std::this_thread::yield();  // 让出CPU时间片，降低资源占用
//}
//

template <class _Rep, class _Period>
inline void SleepFor(const std::chrono::duration<_Rep, _Period>& duration)
{
    std::this_thread::sleep_for(duration);
}

inline void SleepForS(size_t duration)
{
    SleepFor(std::chrono::seconds(duration));
}

inline void SleepForMs(size_t duration)
{
    SleepFor(std::chrono::milliseconds(duration));
}


inline void SleepForUs(size_t duration)
{
    SleepFor(std::chrono::microseconds(duration));
}

inline void SleepForNs(size_t duration)
{
    SleepFor(std::chrono::nanoseconds(duration));
}

