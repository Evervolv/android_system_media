/*
 * Copyright (C) 2025 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#pragma once
#include <utils/Log.h>

#pragma push_macro("LOG_TAG")
#undef LOG_TAG
#define LOG_TAG "audio_utils::atomic"

namespace android::audio_utils {

// relaxed_atomic implements the same features as std::atomic<T> but using
// std::memory_order_relaxed as default.
//
// This is the minimum consistency for the multiple writer multiple reader case.

template <typename T>
class relaxed_atomic : private std::atomic<T> {
public:
    constexpr relaxed_atomic(T desired = {}) : std::atomic<T>(desired) {}
    operator T() const { return std::atomic<T>::load(std::memory_order_relaxed); }
    T operator=(T desired) {
        std::atomic<T>::store(desired, std::memory_order_relaxed); return desired;
    }

    T operator--() { return std::atomic<T>::fetch_sub(1, std::memory_order_relaxed) - 1; }
    T operator++() { return std::atomic<T>::fetch_add(1, std::memory_order_relaxed) + 1;  }
    T operator+=(const T value) {
        return std::atomic<T>::fetch_add(value, std::memory_order_relaxed) + value;
    }

    T load(std::memory_order order = std::memory_order_relaxed) const {
        return std::atomic<T>::load(order);
    }
    T fetch_add(T arg, std::memory_order order =std::memory_order_relaxed) {
        return std::atomic<T>::fetch_add(arg, order);
    }
    bool compare_exchange_weak(
            T& expected, T desired, std::memory_order order = std::memory_order_relaxed) {
        return std::atomic<T>::compare_exchange_weak(expected, desired, order);
    }
};

// unordered_atomic implements data storage such that memory reads have a value
// consistent with a memory write in some order, i.e. not having values
// "out of thin air".
//
// Unordered memory reads and writes may not actually take place but be implicitly cached.
// Nevertheless, a memory read should return at least as contemporaneous a value
// as the last memory write before the write thread memory barrier that
// preceded the most recent read thread memory barrier.
//
// This is weaker than relaxed_atomic and has no equivalent C++ terminology.
// unordered_atomic would be used for a single writer, multiple reader case,
// where data access of type T would be a implemented by the compiler and
// hw architecture with a single "uninterruptible" memory operation.
// (The current implementation holds true for general realized CPU architectures).
// Note that multiple writers would cause read-modify-write unordered_atomic
// operations to have inconsistent results.
//
// unordered_atomic is implemented with normal operations such that compiler
// optimizations can take place which would otherwise be discouraged for atomics.
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2016/p0062r1.html

// VT may be volatile qualified, if desired, or a normal arithmetic type.
template <typename VT>
class unordered_atomic {
    using T = std::decay_t<VT>;
    static_assert(std::atomic<T>::is_always_lock_free);
public:
    constexpr unordered_atomic(T desired = {}) : t_(desired) {}
    operator T() const { return t_; }
    T operator=(T desired) { t_ = desired; return desired; }

    // a volatile ++t_ or t_ += 1 is deprecated in C++20.
    T operator--() { return operator+=(-1); }
    T operator++() { return operator+=(1); }
    T operator+=(const T value) {
        T output;
        // atomic overflow is defined as 2's complement.
        (void)__builtin_add_overflow(t_, value, &output);
        return operator=(output);
    }

    T load(std::memory_order order = std::memory_order_relaxed) const { (void)order; return t_; }

private:
    VT t_;
};

} // namespace android::audio_utils
