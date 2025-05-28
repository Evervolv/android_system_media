/*
 * Copyright 2025 The Android Open Source Project
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

#include <android-base/logging.h>
#include <benchmark/benchmark.h>

/*
On Pixel 9 Pro XL Android 16

Note: to bump up the scheduler clock frequency, one can use the toybox uclampset:
$ adb shell uclampset -m 1024 /data/benchmarktest64/audio_atomic_benchmark/audio_atomic_benchmark

For simplicity these tests use the regular invocation:
$ atest audio_atomic_benchmark

Benchmark                                             Time             CPU   Iterations
---------------------------------------------------------------------------------------
BM_std_atomic_add_equals<int32_t>                  6.09 ns         6.06 ns    111837415
BM_std_atomic_add_to_relaxed<int16_t>              4.73 ns         4.71 ns    148254244
BM_std_atomic_add_to_relaxed<int32_t>              4.74 ns         4.72 ns    148431804
BM_std_atomic_add_to_relaxed<int64_t>              4.73 ns         4.72 ns    148325212
BM_std_atomic_add_to_relaxed<float>                8.43 ns         8.40 ns     83275005
BM_std_atomic_add_to_relaxed<double>               8.44 ns         8.41 ns     83175275
BM_std_atomic_add_to_seq_cst<int16_t>              6.15 ns         6.12 ns    114333415
BM_std_atomic_add_to_seq_cst<int32_t>              6.14 ns         6.12 ns    114419640
BM_std_atomic_add_to_seq_cst<int64_t>              6.14 ns         6.12 ns    114268405
BM_std_atomic_add_to_seq_cst<float>                8.24 ns         8.22 ns     84437565
BM_std_atomic_add_to_seq_cst<double>               8.25 ns         8.22 ns     85743036
BM_atomic_add_to_unordered<int16_t>               0.324 ns        0.323 ns   2164817147
BM_atomic_add_to_unordered<int32_t>               0.324 ns        0.323 ns   2165111368
BM_atomic_add_to_unordered<int64_t>               0.324 ns        0.323 ns   2166007205
BM_atomic_add_to_unordered<float>                 0.650 ns        0.647 ns   1082261791
BM_atomic_add_to_unordered<double>                0.649 ns        0.647 ns   1084858584
BM_atomic_add_to_unordered<volatile_int16_t>       1.97 ns         1.97 ns    356078819
BM_atomic_add_to_unordered<volatile_int32_t>       1.97 ns         1.97 ns    356752252
BM_atomic_add_to_unordered<volatile_int64_t>       1.97 ns         1.97 ns    355550844
BM_atomic_add_to_unordered<volatile_float>         2.73 ns         2.72 ns    257345858
BM_atomic_add_to_unordered<volatile_double>        2.73 ns         2.72 ns    257569407
BM_atomic_add_to_relaxed<int16_t>                  4.66 ns         4.64 ns    150820948
BM_atomic_add_to_relaxed<int32_t>                  4.66 ns         4.65 ns    150876792
BM_atomic_add_to_relaxed<int64_t>                  4.66 ns         4.65 ns    150875922
BM_atomic_add_to_relaxed<float>                    8.59 ns         8.57 ns     81660591
BM_atomic_add_to_relaxed<double>                   8.59 ns         8.57 ns     81708337
BM_atomic_add_to_acq_rel<int16_t>                  6.09 ns         6.07 ns    115522143
BM_atomic_add_to_acq_rel<int32_t>                  6.09 ns         6.07 ns    115954305
BM_atomic_add_to_acq_rel<int64_t>                  6.09 ns         6.07 ns    115475851
BM_atomic_add_to_acq_rel<float>                    8.31 ns         8.28 ns     84750753
BM_atomic_add_to_acq_rel<double>                   8.33 ns         8.31 ns     84298009
BM_atomic_add_to_seq_cst<int16_t>                  6.08 ns         6.06 ns    115819800
BM_atomic_add_to_seq_cst<int32_t>                  6.09 ns         6.07 ns    115277139
BM_atomic_add_to_seq_cst<int64_t>                  6.09 ns         6.06 ns    115215686
BM_atomic_add_to_seq_cst<float>                    8.37 ns         8.35 ns     84116069
BM_atomic_add_to_seq_cst<double>                   8.35 ns         8.32 ns     83978265
BM_atomic_min_unordered<int16_t>                  0.324 ns        0.323 ns   2162398052
BM_atomic_min_unordered<int32_t>                  0.325 ns        0.324 ns   2167766537
BM_atomic_min_unordered<int64_t>                  0.324 ns        0.323 ns   2166667968
BM_atomic_min_unordered<float>                    0.325 ns        0.324 ns   2167960175
BM_atomic_min_unordered<double>                   0.325 ns        0.324 ns   2167053545
BM_atomic_min_seq_cst<int16_t>                     11.5 ns         11.5 ns     61168869
BM_atomic_min_seq_cst<int32_t>                     10.3 ns         10.2 ns     68411173
BM_atomic_min_seq_cst<int64_t>                     10.2 ns         10.2 ns     68716761
BM_atomic_min_seq_cst<float>                       10.6 ns         10.6 ns     66304219
BM_atomic_min_seq_cst<double>                      10.5 ns         10.5 ns     66700397

*/

// ---

template<typename Integer>
static void BM_std_atomic_add_equals(benchmark::State &state) {
    Integer i = 10;
    std::atomic<Integer> dst;
    while (state.KeepRunning()) {
        dst += i;
    }
}

BENCHMARK(BM_std_atomic_add_equals<int32_t>);

template <typename T, android::audio_utils::memory_order MO>
static void BM_atomic_add_to(benchmark::State &state) {
    int64_t i64 = 10;
    android::audio_utils::atomic<T, MO> dst;
    while (state.KeepRunning()) {
        dst.fetch_add(i64, MO);
    }
}

template <typename T>
static void BM_std_atomic_add_to(benchmark::State &state, std::memory_order order) {
    int64_t i64 = 10;
    std::atomic<T> dst;
    while (state.KeepRunning()) {
        dst.fetch_add(i64, order);
    }
}

template <typename T>
static void BM_std_atomic_add_to_relaxed(benchmark::State &state) {
    BM_std_atomic_add_to<T>(state, std::memory_order_relaxed);
}

BENCHMARK(BM_std_atomic_add_to_relaxed<int16_t>);
BENCHMARK(BM_std_atomic_add_to_relaxed<int32_t>);
BENCHMARK(BM_std_atomic_add_to_relaxed<int64_t>);
BENCHMARK(BM_std_atomic_add_to_relaxed<float>);
BENCHMARK(BM_std_atomic_add_to_relaxed<double>);

template <typename T>
static void BM_std_atomic_add_to_seq_cst(benchmark::State &state) {
    BM_std_atomic_add_to<T>(state, std::memory_order_seq_cst);
}

BENCHMARK(BM_std_atomic_add_to_seq_cst<int16_t>);
BENCHMARK(BM_std_atomic_add_to_seq_cst<int32_t>);
BENCHMARK(BM_std_atomic_add_to_seq_cst<int64_t>);
BENCHMARK(BM_std_atomic_add_to_seq_cst<float>);
BENCHMARK(BM_std_atomic_add_to_seq_cst<double>);

template <typename T>
static void BM_atomic_add_to_unordered(benchmark::State &state) {
    BM_atomic_add_to<T, android::audio_utils::memory_order_unordered>(state);
}

BENCHMARK(BM_atomic_add_to_unordered<int16_t>);
BENCHMARK(BM_atomic_add_to_unordered<int32_t>);
BENCHMARK(BM_atomic_add_to_unordered<int64_t>);
BENCHMARK(BM_atomic_add_to_unordered<float>);
BENCHMARK(BM_atomic_add_to_unordered<double>);

// type aliases to allow for macro parsing.
using volatile_int16_t = volatile int16_t;
using volatile_int32_t = volatile int32_t;
using volatile_int64_t = volatile int64_t;
using volatile_float = volatile float;
using volatile_double = volatile double;

BENCHMARK(BM_atomic_add_to_unordered<volatile_int16_t>);
BENCHMARK(BM_atomic_add_to_unordered<volatile_int32_t>);
BENCHMARK(BM_atomic_add_to_unordered<volatile_int64_t>);
BENCHMARK(BM_atomic_add_to_unordered<volatile_float>);
BENCHMARK(BM_atomic_add_to_unordered<volatile_double>);

template <typename T>
static void BM_atomic_add_to_relaxed(benchmark::State &state) {
    BM_atomic_add_to<T, android::audio_utils::memory_order_relaxed>(state);
}

BENCHMARK(BM_atomic_add_to_relaxed<int16_t>);
BENCHMARK(BM_atomic_add_to_relaxed<int32_t>);
BENCHMARK(BM_atomic_add_to_relaxed<int64_t>);
BENCHMARK(BM_atomic_add_to_relaxed<float>);
BENCHMARK(BM_atomic_add_to_relaxed<double>);

template <typename T>
static void BM_atomic_add_to_acq_rel(benchmark::State &state) {
    BM_atomic_add_to<T, android::audio_utils::memory_order_acq_rel>(state);
}

BENCHMARK(BM_atomic_add_to_acq_rel<int16_t>);
BENCHMARK(BM_atomic_add_to_acq_rel<int32_t>);
BENCHMARK(BM_atomic_add_to_acq_rel<int64_t>);
BENCHMARK(BM_atomic_add_to_acq_rel<float>);
BENCHMARK(BM_atomic_add_to_acq_rel<double>);

template <typename T>
static void BM_atomic_add_to_seq_cst(benchmark::State &state) {
    BM_atomic_add_to<T, android::audio_utils::memory_order_seq_cst>(state);
}

BENCHMARK(BM_atomic_add_to_seq_cst<int16_t>);
BENCHMARK(BM_atomic_add_to_seq_cst<int32_t>);
BENCHMARK(BM_atomic_add_to_seq_cst<int64_t>);
BENCHMARK(BM_atomic_add_to_seq_cst<float>);
BENCHMARK(BM_atomic_add_to_seq_cst<double>);

template <typename T, android::audio_utils::memory_order MO>
static void BM_atomic_min(benchmark::State &state) {
    int64_t i64 = 10;
    android::audio_utils::atomic<T, MO> dst;
    while (state.KeepRunning()) {
        dst.min(i64, MO);  // MO is optional, as same as atomic decl.
    }
}

template <typename T>
static void BM_atomic_min_unordered(benchmark::State &state) {
    BM_atomic_min<T, android::audio_utils::memory_order_unordered>(state);
}

BENCHMARK(BM_atomic_min_unordered<int16_t>);
BENCHMARK(BM_atomic_min_unordered<int32_t>);
BENCHMARK(BM_atomic_min_unordered<int64_t>);
BENCHMARK(BM_atomic_min_unordered<float>);
BENCHMARK(BM_atomic_min_unordered<double>);

template <typename T>
static void BM_atomic_min_seq_cst(benchmark::State &state) {
    BM_atomic_min<T, android::audio_utils::memory_order_seq_cst>(state);
}

BENCHMARK(BM_atomic_min_seq_cst<int16_t>);
BENCHMARK(BM_atomic_min_seq_cst<int32_t>);
BENCHMARK(BM_atomic_min_seq_cst<int64_t>);
BENCHMARK(BM_atomic_min_seq_cst<float>);
BENCHMARK(BM_atomic_min_seq_cst<double>);

BENCHMARK_MAIN();
