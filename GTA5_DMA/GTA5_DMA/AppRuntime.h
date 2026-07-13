#pragma once

#include <atomic>

class AppRuntime
{
public:
    static bool IsRunning() noexcept
    {
        return running.load(std::memory_order_acquire);
    }

    static void RequestStop() noexcept
    {
        running.store(false, std::memory_order_release);
    }

    static void Reset() noexcept
    {
        running.store(true, std::memory_order_release);
    }

private:
    static inline std::atomic_bool running{ true };
};
