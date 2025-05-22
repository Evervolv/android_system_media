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
BM_std_atomic_add_equals<int32_t>                  6.08 ns         6.05 ns    110640147
BM_std_atomic_add_to_relaxed<int16_t>              4.74 ns         4.72 ns    148511326
BM_std_atomic_add_to_relaxed<int32_t>              4.74 ns         4.72 ns    148343747
BM_std_atomic_add_to_relaxed<int64_t>              4.73 ns         4.71 ns    148560293
BM_std_atomic_add_to_relaxed<float>                10.1 ns         10.1 ns     69656453
BM_std_atomic_add_to_relaxed<double>               10.1 ns         10.1 ns     69111694
BM_std_atomic_add_to_seq_cst<int16_t>              6.15 ns         6.12 ns    113981457
BM_std_atomic_add_to_seq_cst<int32_t>              6.14 ns         6.11 ns    114541314
BM_std_atomic_add_to_seq_cst<int64_t>              6.15 ns         6.12 ns    114542833
BM_std_atomic_add_to_seq_cst<float>                10.2 ns         10.2 ns     68650295
BM_std_atomic_add_to_seq_cst<double>               10.2 ns         10.2 ns     68695088
BM_atomic_add_to_unordered<int16_t>               0.324 ns        0.322 ns   2170285304
BM_atomic_add_to_unordered<int32_t>               0.325 ns        0.324 ns   2165224174
BM_atomic_add_to_unordered<int64_t>               0.325 ns        0.323 ns   2166623969
BM_atomic_add_to_unordered<float>                 0.650 ns        0.646 ns   1080684524
BM_atomic_add_to_unordered<double>                0.648 ns        0.646 ns   1083652559
BM_atomic_add_to_unordered<volatile_int16_t>       1.97 ns         1.97 ns    355375324
BM_atomic_add_to_unordered<volatile_int32_t>       1.97 ns         1.96 ns    357236012
BM_atomic_add_to_unordered<volatile_int64_t>       1.97 ns         1.96 ns    357296847
BM_atomic_add_to_unordered<volatile_float>         2.73 ns         2.72 ns    257564754
BM_atomic_add_to_unordered<volatile_double>        2.73 ns         2.72 ns    257254223
BM_atomic_add_to_relaxed<int16_t>                  4.66 ns         4.64 ns    150588767
BM_atomic_add_to_relaxed<int32_t>                  4.63 ns         4.59 ns    152128364
BM_atomic_add_to_relaxed<int64_t>                  4.66 ns         4.64 ns    150650980
BM_atomic_add_to_relaxed<float>                    8.34 ns         8.30 ns     84584618
BM_atomic_add_to_relaxed<double>                   8.19 ns         8.16 ns     90196988
BM_atomic_add_to_acq_rel<int16_t>                  6.08 ns         6.06 ns    115712285
BM_atomic_add_to_acq_rel<int32_t>                  6.09 ns         6.07 ns    115737507
BM_atomic_add_to_acq_rel<int64_t>                  6.08 ns         6.05 ns    115364041
BM_atomic_add_to_acq_rel<float>                    8.42 ns         8.40 ns     83578269
BM_atomic_add_to_acq_rel<double>                   7.94 ns         7.90 ns     83344879
BM_atomic_add_to_seq_cst<int16_t>                  6.08 ns         6.05 ns    115703258
BM_atomic_add_to_seq_cst<int32_t>                  6.09 ns         6.06 ns    115860124
BM_atomic_add_to_seq_cst<int64_t>                  6.08 ns         6.06 ns    115305035
BM_atomic_add_to_seq_cst<float>                    8.44 ns         8.40 ns     83275488
BM_atomic_add_to_seq_cst<double>                   8.42 ns         8.39 ns     83474250

*/

// ---

template<typename Integer>
static void BM_std_atomic_add_equals(benchmark::State &state) {
    Integer i = 10;
    std::atomic<Integer> dst;
    while (state.KeepRunning()) {
        dst += i;
    }
    LOG(DEBUG) << __func__ << "  " << dst.load();
}

BENCHMARK(BM_std_atomic_add_equals<int32_t>);

template <typename T, android::audio_utils::memory_order MO>
static void BM_atomic_add_to(benchmark::State &state) {
    int64_t i64 = 10;
    android::audio_utils::atomic<T, MO> dst;
    while (state.KeepRunning()) {
        android::audio_utils::atomic_add_to(dst, i64, MO);
    }
    LOG(DEBUG) << __func__ << "  " << dst.load();
}

template <typename T>
static void BM_std_atomic_add_to(benchmark::State &state, std::memory_order order) {
    int64_t i64 = 10;
    std::atomic<T> dst;
    while (state.KeepRunning()) {
        android::audio_utils::atomic_add_to(dst, i64, order);
    }
    LOG(DEBUG) << __func__ << "  " << dst.load();
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

BENCHMARK_MAIN();
