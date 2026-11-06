#pragma once
#include <atomic>
#include <memory>

namespace CustomComponents::Core {
// NDK r27's libc++ lacks atomic<shared_ptr>. Keep the portable implementation
// here so callers use the same immutable snapshot contract on both toolchains.
template<class T> class AtomicSnapshot {
public:
    using Pointer=std::shared_ptr<const T>;
    explicit AtomicSnapshot(Pointer initial):value_(std::move(initial)){}
    Pointer load() const {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        return value_.load();
#else
        return std::atomic_load(&value_);
#endif
    }
    void store(Pointer next) {
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
        value_.store(std::move(next));
#else
        std::atomic_store(&value_,std::move(next));
#endif
    }
private:
#if defined(__cpp_lib_atomic_shared_ptr) && __cpp_lib_atomic_shared_ptr >= 201711L
    std::atomic<Pointer> value_;
#else
    Pointer value_;
#endif
};
}
