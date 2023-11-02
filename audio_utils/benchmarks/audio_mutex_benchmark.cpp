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
  #BM_gettid                                                 2.115342172044339 ns    2.1061632376274173 ns     331472681
  #BM_systemTime                                             43.03447060436191 ns    42.859535677190046 ns      16327477
  #BM_thread_8_variables                                     2.822119749500643 ns    2.8087018476524515 ns     249247687
  #BM_thread_local_8_variables                              2.8188819195970716 ns    2.8080987231726664 ns     249230645
  #BM_StdMutexLockUnlock                                     18.42923637531376 ns    18.355880590413975 ns      38122065
  #BM_AudioUtilsMutexLockUnlock                             20.530347308731717 ns     20.44086977790593 ns      34261160
  #BM_StdMutexInitializationLockUnlock                       30.94981066330117 ns    30.772674894724165 ns      22748099
  #BM_AudioUtilsMutexInitializationLockUnlock                33.60465197933802 ns     33.45088721995021 ns      20933197
  #BM_StdMutexBlockingConditionVariable/threads:2           15598.164938497572 ns      17862.5971845997 ns         45038
  #BM_AudioUtilsMutexBlockingConditionVariable/threads:2    25141.637174999687 ns     27852.38660000005 ns         20000
  #BM_StdMutexScopedLockUnlock/threads:2                     380.0348418207431 ns     753.7573942934673 ns       4086124
  #BM_AudioUtilsMutexScopedLockUnlock/threads:2              204.0065714999173 ns    403.36853050000013 ns       2000000
  #BM_StdMutexReverseScopedLockUnlock/threads:2             198.50036686369555 ns    389.49119346043153 ns       4417172
  #BM_AudioUtilsMutexReverseScopedLockUnlock/threads:2       79.61722432988687 ns    151.68738380988114 ns       4728242
  #BM_empty_while                                            0.352485920999925 ns    0.3510130680000003 ns    1000000000

PI enabled
Benchmark                                                     Time        CPU        Iteration
audio_mutex_benchmark:
  #BM_gettid                                                 2.115841043574212 ns      2.106164556890608 ns     314075356
  #BM_systemTime                                            43.068210659253694 ns      42.89487830998286 ns      16326935
  #BM_thread_8_variables                                     2.820321515912982 ns     2.8082632519905406 ns     249264987
  #BM_thread_local_8_variables                               2.820691332360452 ns      2.808216987710365 ns     249210206
  #BM_StdMutexLockUnlock                                      18.4656142717955 ns     18.366808223235488 ns      38184970
  #BM_AudioUtilsMutexLockUnlock                             21.241130324140514 ns      21.14499540195153 ns      33086863
  #BM_StdMutexInitializationLockUnlock                       30.90515911655626 ns      30.77286089397384 ns      22747131
  #BM_AudioUtilsMutexInitializationLockUnlock                  39.248949517401 ns      39.07192182682415 ns      17915813
  #BM_StdMutexBlockingConditionVariable/threads:2           14131.826570412255 ns     16092.564409410255 ns         48968
  #BM_AudioUtilsMutexBlockingConditionVariable/threads:2     17858.91191208355 ns      18889.89038833846 ns         52686
  #BM_StdMutexScopedLockUnlock/threads:2                     282.3911431662386 ns      560.1412155450038 ns       2963576
  #BM_AudioUtilsMutexScopedLockUnlock/threads:2             1887.8786730000456 ns     2293.6081274999997 ns       2000000
  #BM_StdMutexReverseScopedLockUnlock/threads:2             155.36251825000136 ns             269.308256 ns       2000000
  #BM_AudioUtilsMutexReverseScopedLockUnlock/threads:2      373.71898424999017 ns     486.31225649999953 ns       2000000
  #BM_empty_while                                           0.3571080320000419 ns    0.35527909100000166 ns    1000000000

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
    ALOGD("%s", "");
}

// Benchmark to see the cost of doing nothing.
BENCHMARK(BM_empty_while);

BENCHMARK_MAIN();
