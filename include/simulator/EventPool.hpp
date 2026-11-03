#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <new>
#include <utility>

// Reuses the memory of finished events. An event and its shared_ptr control
// block are one allocation (std::allocate_shared); a freed block goes on a
// per-thread free list for its size and is handed out again instead of
// returning to the heap. Ownership, weak references (inertial cancellation)
// and destruction order are those of std::make_shared.
namespace circuit::detail {

class EventPool {
public:
    static constexpr std::size_t kGrain = 16;
    static constexpr std::size_t kLargest = 256;

    static void* take(std::size_t bytes) {
        if (bytes > kLargest) return ::operator new(bytes);
        // Every small block has its class's full size, so any thread's list
        // may take it back.
        if (!alive_) return ::operator new(classBytes(bytes));
        auto& head = local().free_[index(bytes)];
        if (head) {
            Block* block = head;
            head = block->next;
            return block;
        }
        return ::operator new(classBytes(bytes));
    }

    static void give(void* memory, std::size_t bytes) noexcept {
        if (bytes > kLargest || !alive_) {
            ::operator delete(memory);
            return;
        }
        auto& head = local().free_[index(bytes)];
        head = ::new (memory) Block{head};
    }

    ~EventPool() {
        alive_ = false;
        for (Block* head : free_)
            while (head) {
                Block* next = head->next;
                ::operator delete(head);
                head = next;
            }
    }

private:
    struct Block { Block* next; };
    std::array<Block*, kLargest / kGrain> free_{};
    // Cleared when this thread's pool is destroyed; blocks freed after that
    // (events outliving it at thread exit) go straight back to the heap.
    static inline thread_local bool alive_ = true;

    static constexpr std::size_t index(std::size_t bytes) { return (bytes - 1) / kGrain; }
    static constexpr std::size_t classBytes(std::size_t bytes) { return (index(bytes) + 1) * kGrain; }
    static EventPool& local() {
        thread_local EventPool pool;
        return pool;
    }
};

template<class T>
struct EventAllocator {
    using value_type = T;
    static_assert(alignof(T) <= alignof(std::max_align_t), "Event pool blocks have default alignment");

    EventAllocator() noexcept = default;
    template<class U> EventAllocator(const EventAllocator<U>&) noexcept {}

    T* allocate(std::size_t n) { return static_cast<T*>(EventPool::take(n * sizeof(T))); }
    void deallocate(T* memory, std::size_t n) noexcept { EventPool::give(memory, n * sizeof(T)); }

    template<class U> bool operator==(const EventAllocator<U>&) const noexcept { return true; }
};

} // namespace circuit::detail

namespace circuit {
// std::make_shared for events, with the memory taken from the event pool.
template<class T, class... Args>
std::shared_ptr<T> makeEvent(Args&&... args) {
    return std::allocate_shared<T>(detail::EventAllocator<T>{}, std::forward<Args>(args)...);
}
} // namespace circuit
