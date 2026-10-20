#pragma once

#include <array>
#include <atomic>
#include <cstddef>

namespace coldx::detail {

// Single-producer, single-consumer ring with no locks: one thread only pushes, one thread
// only pops. Used between the game thread and the audio thread, where a lock could make
// the audio thread wait on the game.
template <class T, std::size_t Capacity>
class SpscQueue {
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity must be a power of two");

public:
    // Producer only. False when full.
    bool Push(const T& value) {
        const std::size_t tail = tail_.load(std::memory_order_relaxed);
        if (tail - head_.load(std::memory_order_acquire) == Capacity) return false;
        items_[tail & (Capacity - 1)] = value;
        tail_.store(tail + 1, std::memory_order_release);
        return true;
    }

    // Consumer only. False when empty.
    bool Pop(T& value) {
        const std::size_t head = head_.load(std::memory_order_relaxed);
        if (head == tail_.load(std::memory_order_acquire)) return false;
        value = items_[head & (Capacity - 1)];
        head_.store(head + 1, std::memory_order_release);
        return true;
    }

private:
    std::array<T, Capacity> items_{};
    std::atomic<std::size_t> head_{0};
    std::atomic<std::size_t> tail_{0};
};

} // namespace coldx::detail
