// Diagnostic LD_PRELOAD equipment. Counts C++ allocation requests only;
// atomic counters add overhead, so these are not throughput measurements.
#include <atomic>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <new>

namespace {
std::atomic<bool> active{false};
std::atomic<unsigned long long> calls{0},bytes{0};
void note(std::size_t size) {
    if(active.load(std::memory_order_relaxed)) {
        calls.fetch_add(1,std::memory_order_relaxed);
        bytes.fetch_add(size,std::memory_order_relaxed);
    }
}
}
extern "C" void engineAllocBegin() {
    calls.store(0);bytes.store(0);active.store(true);
}
extern "C" void engineAllocEnd() {
    active.store(false);
    std::fprintf(stderr,"ENGINE_ALLOC {\"calls\":%llu,\"requestedBytes\":%llu}\n",
        calls.load(),bytes.load());
}
void* operator new(std::size_t n) {
    note(n);if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p,std::size_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t) noexcept { std::free(p); }
void* operator new(std::size_t n,std::align_val_t a) {
    note(n);void* p=nullptr;
    if(!posix_memalign(&p,std::size_t(a),n?n:1))return p;throw std::bad_alloc();
}
void* operator new[](std::size_t n,std::align_val_t a) { return ::operator new(n,a); }
void operator delete(void* p,std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p,std::align_val_t) noexcept { std::free(p); }
void operator delete(void* p,std::size_t,std::align_val_t) noexcept { std::free(p); }
void operator delete[](void* p,std::size_t,std::align_val_t) noexcept { std::free(p); }
