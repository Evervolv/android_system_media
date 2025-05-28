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

#include <audio_utils/atomic.h>
#include <gtest/gtest.h>

#include <random>
#include <thread>

using namespace android::audio_utils;

// fetch_op always returns previous value
static_assert(atomic<int, memory_order_unordered>(1).fetch_add(1) == 1);
static_assert(atomic<int, memory_order_unordered>(1).fetch_sub(1) == 1);
static_assert(atomic<int, memory_order_unordered>(1).fetch_and(1, memory_order_unordered) == 1);
static_assert(atomic<int, memory_order_unordered>(1).fetch_or(1, memory_order_unordered) == 1);
static_assert(atomic<int, memory_order_unordered>(1).fetch_xor(1, memory_order_unordered) == 1);

// op equals always returns current (updated) value
static_assert(atomic<int, memory_order_unordered>(1).operator+=(1) == 2);
static_assert(atomic<int, memory_order_unordered>(1).operator-=(1) == 0);
static_assert(atomic<int, memory_order_unordered>(1).operator&=(1) == 1);
static_assert(atomic<int, memory_order_unordered>(1).operator|=(1) == 1);
static_assert(atomic<int, memory_order_unordered>(1).operator^=(1) == 0);

// min/max ops
static_assert(atomic<int, memory_order_unordered>(1).min(2, memory_order_unordered) == 1);
static_assert(atomic<int, memory_order_unordered>(3).min(2, memory_order_unordered) == 2);
static_assert(atomic<int, memory_order_unordered>(1).max(2, memory_order_unordered) == 2);
static_assert(atomic<int, memory_order_unordered>(3).max(2, memory_order_unordered) == 3);

// overflow
static_assert(atomic<int, memory_order_unordered>(INT_MAX).operator+=(INT_MAX)
         == (INT_MAX << 1));
static_assert(atomic<int, memory_order_unordered>(-INT_MAX).operator-=(INT_MAX)
         == (-INT_MAX << 1));

template <android::audio_utils::memory_order MO>
void testAdd() {
    constexpr size_t kNumThreads = 10;
    constexpr size_t kWorkerIterations = 100;
    std::vector<std::thread> threads;
    atomic<size_t, MO> value;

    auto worker = [&] {
        for (size_t i = 0; i < kWorkerIterations; ++i) {
            ++value;
        }
    };
    for (size_t i = 0; i < kNumThreads; ++i) {
        threads.emplace_back(worker);
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(value, kNumThreads * kWorkerIterations);
}

TEST(audio_atomic_tests, add_relaxed) {
    testAdd<memory_order_relaxed>();
}
TEST(audio_atomic_tests, add_acquire) {
    testAdd<memory_order_acquire>();
}
TEST(audio_atomic_tests, add_release) {
    testAdd<memory_order_release>();
}
TEST(audio_atomic_tests, add_acq_rel) {
    testAdd<memory_order_acq_rel>();
}
TEST(audio_atomic_tests, add_seq_cst) {
    testAdd<memory_order_seq_cst>();
}

template <android::audio_utils::memory_order MO>
void testMin() {
    constexpr size_t kNumThreads = 10;
    std::vector<std::thread> threads;
    atomic<size_t, MO> value = INT32_MAX;

    for (size_t i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&value, i] {
            value.min(i);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(value, 0UL);
}

TEST(audio_atomic_tests, min_relaxed) {
testMin<memory_order_relaxed>();
}
TEST(audio_atomic_tests, min_acquire) {
testMin<memory_order_acquire>();
}
TEST(audio_atomic_tests, min_release) {
testMin<memory_order_release>();
}
TEST(audio_atomic_tests, min_acq_rel) {
testMin<memory_order_acq_rel>();
}
TEST(audio_atomic_tests, min_seq_cst) {
testMin<memory_order_seq_cst>();
}

template <android::audio_utils::memory_order MO>
void testMax() {
    constexpr size_t kNumThreads = 10;
    std::vector<std::thread> threads;
    atomic<size_t, MO> value = 0;

    for (size_t i = 0; i < kNumThreads; ++i) {
        threads.emplace_back([&value, i] {
            value.max(i);
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(value, kNumThreads - 1);
}

TEST(audio_atomic_tests, max_relaxed) {
testMax<memory_order_relaxed>();
}
TEST(audio_atomic_tests, max_acquire) {
testMax<memory_order_acquire>();
}
TEST(audio_atomic_tests, max_release) {
testMax<memory_order_release>();
}
TEST(audio_atomic_tests, max_acq_rel) {
testMax<memory_order_acq_rel>();
}
TEST(audio_atomic_tests, max_seq_cst) {
testMax<memory_order_seq_cst>();
}

template <typename T, android::audio_utils::memory_order MO>
void testOp() {
    size_t kTrials = 1000;
    std::minstd_rand gen(45);
    std::uniform_int_distribution<T> dis(-100, 100);

    for (size_t i = 0; i < kTrials; ++i) {
        int r = dis(gen);
        T value(r);
        atomic<T, MO> avalue(r);
        EXPECT_EQ(value, avalue);

        r = dis(gen);
        value += r;
        avalue += r;
        EXPECT_EQ(value, avalue);

        r = dis(gen);
        value -= r;
        avalue -= r;
        EXPECT_EQ(value, avalue);

        r = dis(gen);
        value &= r;
        avalue &= r;
        EXPECT_EQ(value, avalue);

        r = dis(gen);
        value |= r;
        avalue |= r;
        EXPECT_EQ(value, avalue);

        r = dis(gen);
        value ^= r;
        avalue ^= r;
        EXPECT_EQ(value, avalue);

        r = dis(gen);
        value  = std::min(value, r);
        avalue.min(r);
        EXPECT_EQ(value, avalue);

        r = dis(gen);
        value  = std::max(value, r);
        avalue.max(r);
        EXPECT_EQ(value, avalue);
    }
}

TEST(audio_atomic_tests, op_relaxed) {
    testOp<int32_t, memory_order_relaxed>();
}
TEST(audio_atomic_tests, op_acquire) {
    testOp<int32_t, memory_order_acquire>();
}
TEST(audio_atomic_tests, op_release) {
    testOp<int32_t, memory_order_release>();
}
TEST(audio_atomic_tests, op_acq_rel) {
    testOp<int32_t, memory_order_acq_rel>();
}
TEST(audio_atomic_tests, op_seq_cst) {
    testOp<int32_t, memory_order_seq_cst>();
}

template <typename T, android::audio_utils::memory_order MO>
void testOverflow() {
    atomic<T, MO> avalue(std::numeric_limits<T>::max());
    avalue += avalue;
    EXPECT_EQ(avalue, std::numeric_limits<T>::max() << 1);

    if constexpr (std::is_signed_v<T>) {
         avalue = -std::numeric_limits<T>::max();
         avalue -= std::numeric_limits<T>::max();
         EXPECT_EQ(avalue, -std::numeric_limits<T>::max() << 1);
    } else /* constexpr */ {
         avalue = 0;
         avalue -= std::numeric_limits<T>::max();
         EXPECT_EQ(avalue, static_cast<T>(-std::numeric_limits<T>::max()));
    }
}

TEST(audio_atomic_tests, overflow) {
    testOverflow<int32_t, memory_order_unordered>();
    testOverflow<uint32_t, memory_order_unordered>();
    testOverflow<int64_t, memory_order_unordered>();
    testOverflow<uint64_t, memory_order_unordered>();

    testOverflow<int32_t, memory_order_relaxed>();
    testOverflow<uint32_t, memory_order_relaxed>();
    testOverflow<int64_t, memory_order_relaxed>();
    testOverflow<uint64_t, memory_order_relaxed>();
}
