/*
 * Copyright 2023 The Android Open Source Project
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

#include <audio_utils/mutex.h>

#include <benchmark/benchmark.h>
#include <thread>
#include <utils/Timers.h>

/*
On Pixel 7 U arm64-v8a

Note: to bump up the scheduler clock frequency, one can use the toybox uclampset:
$ adb shell uclampset -m 1024 /data/benchmarktest64/audio_mutex_benchmark/audio_mutex_benchmark

For simplicity these tests use the regular invocation:
$ atest audio_mutex_benchmark

PI disabled
Benchmark                                                     Time        CPU        Iteration
audio_mutex_benchmark:
  #BM_gettid                                                 2.5715349136988648 ns    2.5586918245763295 ns     273439199
  #BM_systemTime                                              52.73853465198624 ns     52.47834849877889 ns      13291180
  #BM_thread_8_variables                                     2.8220728658952146 ns     2.808457043254089 ns     247811101
  #BM_thread_local_8_variables                                2.819040735896784 ns     2.808194582584339 ns     249232048
  #BM_StdMutexLockUnlock                                     18.269513666323864 ns    18.187642353794956 ns      38449976
  #BM_AudioUtilsMutexLockUnlock                              27.471324314156924 ns    27.369819151278517 ns      25569990
  #BM_StdMutexInitializationLockUnlock                       27.642304788363877 ns    27.454472469163502 ns      25488292
  #BM_AudioUtilsMutexInitializationLockUnlock                 43.40413311227762 ns     43.20731457780621 ns      16200771
  #BM_StdMutexBlockingConditionVariable/threads:2            25165.942374951555 ns    28006.765750000013 ns         20000
  #BM_AudioUtilsMutexBlockingConditionVariable/threads:2      14197.24746364191 ns    16493.674750414313 ns         49482
  #BM_StdMutexScopedLockUnlock/threads:2                     109.94932188740843 ns    217.69939856399375 ns       2419210
  #BM_AudioUtilsMutexScopedLockUnlock/threads:2              430.52361814635617 ns     842.5634754177531 ns       1424646
  #BM_StdMutexReverseScopedLockUnlock/threads:2              111.16320912559515 ns     215.4758082434359 ns       4185682
  #BM_AudioUtilsMutexReverseScopedLockUnlock/threads:2        581.0104671603232 ns    1144.9661062112107 ns       1842668
  #BM_empty_while                                           0.35246984900004463 ns           0.351032429 ns    1000000000

PI enabled
Benchmark                                                     Time        CPU        Iteration
audio_mutex_benchmark:
  #BM_gettid                                                  2.57265770908791 ns     2.558915261731079 ns     272750108
  #BM_systemTime                                             45.02782731519532 ns     44.77538940288014 ns      15517990
  #BM_thread_8_variables                                      2.82048821432608 ns    2.8086737310128336 ns     249207682
  #BM_thread_local_8_variables                               2.821494282748445 ns    2.8082673263448075 ns     249269439
  #BM_StdMutexLockUnlock                                      18.2899207197116 ns     18.19974609562134 ns      38449908
  #BM_AudioUtilsMutexLockUnlock                             27.965107817753818 ns      27.7930076355372 ns      25186702
  #BM_StdMutexInitializationLockUnlock                      27.563432355780428 ns    27.451745319727934 ns      25501402
  #BM_AudioUtilsMutexInitializationLockUnlock               45.749190255258064 ns     45.56241199594726 ns      15367190
  #BM_StdMutexBlockingConditionVariable/threads:2            24866.48901250736 ns    27328.534937855024 ns         46826
  #BM_AudioUtilsMutexBlockingConditionVariable/threads:2    11861.117787962428 ns    14368.585792087117 ns         62430
  #BM_StdMutexScopedLockUnlock/threads:2                    103.39507704299898 ns    204.76349922317442 ns       3591540
  #BM_AudioUtilsMutexScopedLockUnlock/threads:2             1268.2326061774495 ns     1434.825134343846 ns       1079320
  #BM_StdMutexReverseScopedLockUnlock/threads:2               65.2728623119491 ns    125.24614052060664 ns       4876694
  #BM_AudioUtilsMutexReverseScopedLockUnlock/threads:2      1090.3344660587566 ns    1262.9917900152602 ns       5926564
  #BM_empty_while                                           0.4291939290051232 ns    0.4264520980000004 ns    1000000000

*/

// ---

// Benchmark gettid().  The mutex class uses this to get the linux thread id.
static void BM_gettid(benchmark::State &state) {
    int32_t value = 0;
    while (state.KeepRunning()) {
        value ^= gettid();  // ensure the return value used.
    }
    ALOGD("%s: value:%d", __func__, value);
}

BENCHMARK(BM_gettid);

// Benchmark systemTime().  The mutex class uses this for timing.
static void BM_systemTime(benchmark::State &state) {
    int64_t value = 0;
    while (state.KeepRunning()) {
        value ^= systemTime();
    }
    ALOGD("%s: value:%lld", __func__, (long long)value);
}

BENCHMARK(BM_systemTime);

// Benchmark access to 8 thread local storage variables by compiler built_in __thread.
__thread volatile int tls_value1 = 1;
__thread volatile int tls_value2 = 2;
__thread volatile int tls_value3 = 3;
__thread volatile int tls_value4 = 4;
__thread volatile int tls_value5 = 5;
__thread volatile int tls_value6 = 6;
__thread volatile int tls_value7 = 7;
__thread volatile int tls_value8 = 8;

static void BM_thread_8_variables(benchmark::State &state) {
    while (state.KeepRunning()) {
        tls_value1 ^= tls_value1 ^ tls_value2 ^ tls_value3 ^ tls_value4 ^
                tls_value5 ^ tls_value6 ^ tls_value7 ^ tls_value8;
    }
    ALOGD("%s: value:%d", __func__, tls_value1);
}

BENCHMARK(BM_thread_8_variables);

// Benchmark access to 8 thread local storage variables through the
// the C/C++ 11 standard thread_local.
thread_local volatile int tlsa_value1 = 1;
thread_local volatile int tlsa_value2 = 2;
thread_local volatile int tlsa_value3 = 3;
thread_local volatile int tlsa_value4 = 4;
thread_local volatile int tlsa_value5 = 5;
thread_local volatile int tlsa_value6 = 6;
thread_local volatile int tlsa_value7 = 7;
thread_local volatile int tlsa_value8 = 8;

static void BM_thread_local_8_variables(benchmark::State &state) {
    while (state.KeepRunning()) {
        tlsa_value1 ^= tlsa_value1 ^ tlsa_value2 ^ tlsa_value3 ^ tlsa_value4 ^
                tlsa_value5 ^ tlsa_value6 ^ tlsa_value7 ^ tlsa_value8;
    }
    ALOGD("%s: value:%d", __func__, tlsa_value1);
}

BENCHMARK(BM_thread_local_8_variables);

// ---

template <typename Mutex>
void MutexLockUnlock(benchmark::State& state) {
    Mutex m;
    while (state.KeepRunning()) {
        m.lock();
        m.unlock();
    }
}

static void BM_StdMutexLockUnlock(benchmark::State &state) {
    MutexLockUnlock<std::mutex>(state);
}

// Benchmark repeated mutex lock/unlock from a single thread
// using std::mutex.
BENCHMARK(BM_StdMutexLockUnlock);

static void BM_AudioUtilsMutexLockUnlock(benchmark::State &state) {
    MutexLockUnlock<android::audio_utils::mutex>(state);
}

// Benchmark repeated mutex lock/unlock from a single thread
// using audio_utils::mutex.  This will differ when priority inheritance
// is used or not.
BENCHMARK(BM_AudioUtilsMutexLockUnlock);

// ---

template <typename Mutex>
void MutexInitializationLockUnlock(benchmark::State& state) {
    while (state.KeepRunning()) {
        Mutex m;
        m.lock();
        m.unlock();
    }
}

static void BM_StdMutexInitializationLockUnlock(benchmark::State &state) {
    MutexInitializationLockUnlock<std::mutex>(state);
}

// Benchmark repeated mutex creation then lock/unlock from a single thread
// using std::mutex.
BENCHMARK(BM_StdMutexInitializationLockUnlock);

static void BM_AudioUtilsMutexInitializationLockUnlock(benchmark::State &state) {
    MutexInitializationLockUnlock<android::audio_utils::mutex>(state);
}

// Benchmark repeated mutex creation then lock/unlock from a single thread
// using audio_utils::mutex.  This will differ when priority inheritance
// is used or not.
BENCHMARK(BM_AudioUtilsMutexInitializationLockUnlock);

// ---

static constexpr size_t THREADS = 2;

template <typename Mutex, typename UniqueLock, typename ConditionVariable>
class MutexBlockingConditionVariable {
    Mutex m;
    ConditionVariable cv[THREADS];
    bool wake[THREADS];

public:
    void run(benchmark::State& state) {
        const size_t local = state.thread_index();
        const size_t remote = (local + 1) % THREADS;
        if (local == 0) wake[local] = true;
        for (auto _ : state) {
            UniqueLock ul(m);
            cv[local].wait(ul, [&]{ return wake[local]; });
            wake[remote] = true;
            wake[local] = false;
            cv[remote].notify_one();
        }
        UniqueLock ul(m);
        wake[remote] = true;
        cv[remote].notify_one();
    }
};

MutexBlockingConditionVariable<std::mutex,
            std::unique_lock<std::mutex>,
            std::condition_variable> CvStd;

static void BM_StdMutexBlockingConditionVariable(benchmark::State &state) {
    CvStd.run(state);
}

// Benchmark 2 threads that use condition variables to wake each other up,
// where only one thread is active at a given time.  This benchmark
// uses std::mutex, std::unique_lock, and std::condition_variable.
BENCHMARK(BM_StdMutexBlockingConditionVariable)->Threads(THREADS);

MutexBlockingConditionVariable<android::audio_utils::mutex,
        android::audio_utils::unique_lock,
        android::audio_utils::condition_variable> CvAu;

static void BM_AudioUtilsMutexBlockingConditionVariable(benchmark::State &state) {
    CvAu.run(state);
}

// Benchmark 2 threads that use condition variables to wake each other up,
// where only one thread is active at a given time.  This benchmark
// uses audio_utils::mutex, audio_utils::unique_lock, and audio_utils::condition_variable.
//
// Priority inheritance mutexes will affect the performance of this benchmark.
BENCHMARK(BM_AudioUtilsMutexBlockingConditionVariable)->Threads(THREADS);

// ---

template <typename Mutex, typename ScopedLock>
class MutexScopedLockUnlock {
    const bool reverse_;
    Mutex m1_, m2_;
    int counter_ = 0;

public:
    MutexScopedLockUnlock(bool reverse = false) : reverse_(reverse) {}

    void run(benchmark::State& state) {
        const size_t index = state.thread_index();
        for (auto _ : state) {
            if (!reverse_ || index & 1) {
                ScopedLock s1(m1_, m2_);
                ++counter_;
            } else {
                ScopedLock s1(m2_, m1_);
                ++counter_;
            }
        }
    }
};

MutexScopedLockUnlock<std::mutex,
        std::scoped_lock<std::mutex, std::mutex>> ScopedStd;

static void BM_StdMutexScopedLockUnlock(benchmark::State &state) {
    ScopedStd.run(state);
}

// Benchmark scoped_lock where two threads try to obtain the
// same 2 locks with the same initial acquisition order.
// This uses std::mutex, std::scoped_lock.
BENCHMARK(BM_StdMutexScopedLockUnlock)->Threads(THREADS);

MutexScopedLockUnlock<android::audio_utils::mutex,
        android::audio_utils::scoped_lock<
                android::audio_utils::mutex, android::audio_utils::mutex>> ScopedAu;

static void BM_AudioUtilsMutexScopedLockUnlock(benchmark::State &state) {
    ScopedAu.run(state);
}

// Benchmark scoped_lock where two threads try to obtain the
// same 2 locks with the same initial acquisition order.
// This uses audio_utils::mutex and audio_utils::scoped_lock.
//
// Priority inheritance mutexes will affect the performance of this benchmark.
BENCHMARK(BM_AudioUtilsMutexScopedLockUnlock)->Threads(THREADS);

MutexScopedLockUnlock<std::mutex,
        std::scoped_lock<std::mutex, std::mutex>> ReverseScopedStd(true);

static void BM_StdMutexReverseScopedLockUnlock(benchmark::State &state) {
    ReverseScopedStd.run(state);
}

// Benchmark scoped_lock with odd thread having reversed scoped_lock mutex acquisition order.
// This uses std::mutex, std::scoped_lock.
BENCHMARK(BM_StdMutexReverseScopedLockUnlock)->Threads(THREADS);

MutexScopedLockUnlock<android::audio_utils::mutex,
        android::audio_utils::scoped_lock<
                android::audio_utils::mutex, android::audio_utils::mutex>> ReverseScopedAu(true);

static void BM_AudioUtilsMutexReverseScopedLockUnlock(benchmark::State &state) {
    ReverseScopedAu.run(state);
}

// Benchmark scoped_lock with odd thread having reversed scoped_lock mutex acquisition order.
// This uses audio_utils::mutex and audio_utils::scoped_lock.
//
// Priority inheritance mutexes will affect the performance of this benchmark.
BENCHMARK(BM_AudioUtilsMutexReverseScopedLockUnlock)->Threads(THREADS);

static void BM_empty_while(benchmark::State &state) {

    while (state.KeepRunning()) {
        ;
    }
    ALOGD("%s", android::audio_utils::mutex::all_stats_to_string().c_str());
}

// Benchmark to see the cost of doing nothing.
BENCHMARK(BM_empty_while);

BENCHMARK_MAIN();
