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

#include <atomic>

#pragma push_macro("LOG_TAG")
#undef LOG_TAG
#define LOG_TAG "audio_utils::atomic"

#include <algorithm>
#include <atomic>
#include <functional>

namespace android::audio_utils {

// Rationale:

// std::atomic defaults to memory_order_seq_cst access, with no template options to change the
// default behavior (one must specify a different memory order on each method call).
// This confuses the atomic-by-method-access strategy used in Linux (and older Android methods)
// with an incomplete atomic-by-declaration strategy for C++.
//
// Although the std::atomic default memory_order_seq_cst is the safest and strictest,
// we can often relax the conditions of access based on the variable usage.
//
// The audio_utils atomic fixes this declaration deficiency of std::atomic.
// It allows template specification of relaxed and unordered access by default.
// Consistent atomic behavior is then based on the variable declaration, and switching
// and benchmarking different atomic safety guarantees is easy.

// About unordered access.
//
// memory_order_unordered implements data storage such that memory reads have a value
// consistent with a memory write in some order.
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

// std::atomic and the C11 atomics are not sufficient to implement these libraries
// with unordered access.  We use the C++ compiler built-ins.
//
// https://en.cppreference.com/w/c/language/atomic

enum memory_order {
    memory_order_unordered = -1,
    memory_order_relaxed = (int)std::memory_order_relaxed,
    // memory_order_consume = (int)std::memory_order_consume, // deprecated and omitted.
    memory_order_acquire = (int)std::memory_order_acquire,
    memory_order_release = (int)std::memory_order_release,
    memory_order_acq_rel = (int)std::memory_order_acq_rel,
    memory_order_seq_cst = (int)std::memory_order_seq_cst,
};

inline constexpr int to_gnu_memory_order(memory_order mo) {
    return mo == memory_order_relaxed ? __ATOMIC_RELAXED
    // : mo == memory_order_consume ? __ATOMIC_CONSUME  // deprecated and omitted.
    : mo == memory_order_acquire ? __ATOMIC_ACQUIRE
    : mo == memory_order_release ? __ATOMIC_RELEASE
    : mo == memory_order_acq_rel ? __ATOMIC_ACQ_REL
    : mo == memory_order_seq_cst ? __ATOMIC_SEQ_CST : -1;
}

inline constexpr int to_gnu_load_memory_order(memory_order mo) {
    return mo == memory_order_relaxed ? __ATOMIC_RELAXED
    // : mo == memory_order_consume ? __ATOMIC_CONSUME  // deprecated and omitted.
    : mo == memory_order_acquire ? __ATOMIC_ACQUIRE
    : mo == memory_order_release ? __ATOMIC_RELAXED  // see compare-exchange
    : mo == memory_order_acq_rel ? __ATOMIC_ACQUIRE
    : mo == memory_order_seq_cst ? __ATOMIC_SEQ_CST : -1;
}

inline constexpr int to_gnu_store_memory_order(memory_order mo) {
    return mo == memory_order_relaxed ? __ATOMIC_RELAXED
    // : mo == memory_order_consume ? __ATOMIC_CONSUME  // deprecated and omitted.
    : mo == memory_order_acquire ? __ATOMIC_RELAXED  // for symmetry with load.
    : mo == memory_order_release ? __ATOMIC_RELEASE
    : mo == memory_order_acq_rel ? __ATOMIC_RELEASE
    : mo == memory_order_seq_cst ? __ATOMIC_SEQ_CST : -1;
}

template <typename VT, memory_order MO>
class atomic {
    using T = std::decay_t<VT>;
    static_assert(std::atomic<T>::is_always_lock_free);
public:
    constexpr atomic(T desired = {}) : t_(desired) {}

    constexpr operator T() const { return load(); }

    constexpr T operator=(T desired) {
        store(desired);
        return desired;
    }

    constexpr T operator--(int) { return fetch_sub(1); }
    constexpr T operator++(int) { return fetch_add(1); }
    constexpr T operator--() { return operator-=(1); }
    constexpr T operator++() { return operator+=(1); }

    // these operations return the result.
    constexpr T operator+=(T value) {
        if constexpr (MO == memory_order_unordered) {
            if constexpr (std::is_integral_v<T>) {
                T output;
                // use 2's complement overflow to match atomic spec.
                (void)__builtin_add_overflow(t_, value, &output);
                return operator=(output);
            } else /* constexpr */ {
                return t_ += value;
            }
        } else /* constexpr */ {
            return __atomic_add_fetch(&t_, value, to_gnu_memory_order(MO));
        }
    }
    constexpr T operator-=(T value) {
        if constexpr (MO == memory_order_unordered) {
            if constexpr (std::is_integral_v<T>) {
                T output;
                // use 2's complement overflow to match atomic spec.
                (void)__builtin_sub_overflow(t_, value, &output);
                return operator=(output);
            } else /* constexpr */ {
                return t_ -= value;
            }
        } else /* constexpr */ {
            return __atomic_sub_fetch(&t_, value, to_gnu_memory_order(MO));
        }
    }
    constexpr T operator&=(T value) {
        if constexpr (MO == memory_order_unordered) {
            return t_ &= value;
        } else /* constexpr */ {
            return __atomic_and_fetch(&t_, value, to_gnu_memory_order(MO));
        }
    }
    constexpr T operator|=(T value) {
        if constexpr (MO == memory_order_unordered) {
            return t_ |= value;
        } else /* constexpr */ {
            return __atomic_or_fetch(&t_, value, to_gnu_memory_order(MO));
        }
    }
    constexpr T operator^=(T value) {
        if constexpr (MO == memory_order_unordered) {
            return t_ ^= value;
        } else /* constexpr */ {
            return __atomic_xor_fetch(&t_, value, to_gnu_memory_order(MO));
        }
    }

    // classic atomic load and store
    constexpr T load() const {
        if constexpr (MO == memory_order_unordered) {
            return t_;
        } else /* constexpr */ {
            return load(MO);
        }
    }
    constexpr T load(memory_order mo) const {
        if (mo == memory_order_unordered) {
            return t_;
        } else {
            T ret;
            __atomic_load(&t_, &ret, to_gnu_load_memory_order(mo));
            return ret;
        }
    }
    constexpr void store(T value) {
        if constexpr (MO == memory_order_unordered) {
            t_ = value;
        } else /* constexpr */ {
            store(value, MO);
        }
    }
    constexpr void store(T value, memory_order mo) {
        if (mo == memory_order_unordered) {
            t_ = value;
        } else {
            __atomic_store(&t_, &value, to_gnu_store_memory_order(mo));
        }
    }

    constexpr T apply(const std::function<T(T)>& f, memory_order mo = MO) {
        if (mo == memory_order_unordered) {
            return t_ = f(t_);
        } else {
            T expected, result;
            do {
                expected = t_;
                result = f(expected);
            } while (!compare_exchange_weak(expected, result, mo));
            return result;
        }
    }

    constexpr T max(T value) {
        if constexpr (MO == memory_order_unordered) {
            return t_ = std::max(t_, value);
        } else /* constexpr */ {
            return max(value, MO);
        }
    }

    constexpr T max(T value, memory_order mo) {
        if (mo == memory_order_unordered) {
            return t_ = std::max(t_, value);
        } else {
            return apply([value](T a) -> T { return std::max(value, a); }, mo);
        }
    }

    constexpr T min(T value) {
        if constexpr (MO == memory_order_unordered) {
            return t_ = std::min(t_, value);
        } else /* constexpr */ {
            return min(value, MO);
        }
    }

    constexpr T min(T value, memory_order mo) {
        if (mo == memory_order_unordered) {
            return t_ = std::min(t_, value);
        } else {
            return apply([value](T a) -> T { return std::min(value, a); }, mo);
        }
    }

    // these operations return the value prior to the result.
    constexpr T fetch_add(T value) {
        if constexpr (MO == memory_order_unordered) {
            auto old = t_;
            T output;
            if constexpr (std::is_floating_point_v<T>) {
                output = t_ + value;
            } else /* constexpr */ {
                (void)__builtin_add_overflow(t_, value, &output);
            }
            store(output);
            return old;
        } else /* constexpr */ {
            return fetch_add(value, MO);
        }
    }
    constexpr T fetch_add(T value, memory_order mo) {
        if (mo == memory_order_unordered) {
            auto old = t_;
            T output;
            if constexpr (std::is_floating_point_v<T>) {
                output = t_ + value;
            } else /* constexpr */ {
                (void)__builtin_add_overflow(t_, value, &output);
            }
            store(output, mo);
            return old;
        } else {
            return __atomic_fetch_add(&t_, value, to_gnu_memory_order(mo));
        }
    }
    constexpr T fetch_sub(T value) {
        if constexpr (MO == memory_order_unordered) {
            auto old = t_;
            T output;
            if constexpr (std::is_floating_point_v<T>) {
                output = t_ - value;
            } else /* constexpr */ {
                (void)__builtin_sub_overflow(t_, value, &output);
            }
            store(output);
            return old;
        } else /* constexpr */ {
            return fetch_sub(value, MO);
        }
    }
    constexpr T fetch_sub(T value, memory_order mo) {
        if (mo == memory_order_unordered) {
            auto old = t_;
            T output;
            if constexpr (std::is_floating_point_v<T>) {
                output = t_ - value;
            } else /* constexpr */ {
                (void)__builtin_sub_overflow(t_, value, &output);
            }
            store(output, mo);
            return old;
        } else {
            return __atomic_fetch_sub(&t_, value, to_gnu_memory_order(mo));
        }
    }
    constexpr T fetch_and(T value, memory_order mo = MO) {
        if (mo == memory_order_unordered) {
            auto old = t_;
            t_ &= value;
            return old;
        } else {
            return __atomic_fetch_and(&t_, value, to_gnu_memory_order(mo));
        }
    }
    constexpr T fetch_or(T value, memory_order mo = MO) {
        if (mo == memory_order_unordered) {
            auto old = t_;
            t_ |= value;
            return old;
        } else {
            return __atomic_fetch_or(&t_, value, to_gnu_memory_order(mo));
        }
    }
    constexpr T fetch_xor(T value, memory_order mo = MO) {
        if (mo == memory_order_unordered) {
            auto old = t_;
            t_ ^= value;
            return old;
        } else {
            return __atomic_fetch_xor(&t_, value, to_gnu_memory_order(mo));
        }
    }

    bool compare_exchange_weak(VT& expected, T desired, memory_order mo = MO) {
        if (mo == memory_order_unordered) {
            if (t_ == expected) {
                t_= desired;
                return true;
            } else {
                expected = t_;
                return false;
            }
        } else {
            return __atomic_compare_exchange(&t_,
                    const_cast<T*>(&expected),  // builtin does not take volatile ptr.
                    &desired,
                    true /* weak */,
                    to_gnu_memory_order(mo),
                    to_gnu_load_memory_order(mo));
        }
    }

private:
    // align 8 byte long long/double on 8 bytes on x86.
    VT t_ __attribute__((aligned(std::max(sizeof(VT), alignof(VT)))));
};

} // namespace android::audio_utils

#pragma pop_macro("LOG_TAG")
