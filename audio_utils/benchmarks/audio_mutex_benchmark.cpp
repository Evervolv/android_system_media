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

#include <android-base/logging.h>
#include <benchmark/benchmark.h>
#include <shared_mutex>
#include <thread>
#include <utils/RWLock.h>
#include <utils/Timers.h>

/*
On Pixel 7 U arm64-v8a

Note: to bump up the scheduler clock frequency, one can use the toybox uclampset:
$ adb shell uclampset -m 1024 /data/benchmarktest64/audio_mutex_benchmark/audio_mutex_benchmark

For simplicity these tests use the regular invocation:
$ atest audio_mutex_benchmark

Benchmark                                                     Time        CPU        Iteration
audio_mutex_benchmark:
  #BM_gettid                                                   2.1159776035370936 ns       2.10614115284375 ns     332373750
  #BM_systemTime                                                45.25256074688064 ns     45.040996499041846 ns      15560597
  #BM_thread_8_variables                                       2.8218847925890063 ns      2.808269438152783 ns     249265931
  #BM_thread_local_8_variables                                 2.8201526456300243 ns      2.808261059704455 ns     249274993
  #BM_thread_detach_async                                        66004.9478946627 ns      36389.48824859354 ns         19019
  #BM_thread_join_sync                                         226163.65853653837 ns     115567.64218708145 ns         11193
  #BM_StdMutexLockUnlock                                       20.148828538610392 ns      20.05356471045582 ns      33531405
  #BM_RWMutexReadLockUnlock                                     17.19880095571374 ns      17.12071584234365 ns      40958460
  #BM_RWMutexWriteLockUnlock                                   19.816517996240673 ns      19.73653147918977 ns      35482330
  #BM_SharedMutexReadLockUnlock                                38.818405116741836 ns      38.64741636654499 ns      18109051
  #BM_SharedMutexWriteLockUnlock                               41.472496065276786 ns      41.27604196581944 ns      16957610
  #BM_AudioUtilsMutexLockUnlock                                31.710274563460512 ns     31.560500556055743 ns      22196515
  #BM_AudioUtilsPIMutexLockUnlock                              32.041340383783485 ns     31.906233969585948 ns      21938766
  #BM_StdMutexInitializationLockUnlock                          29.75497564252243 ns     29.621176282468877 ns      23639560
  #BM_RWMutexInitializationReadLockUnlock                      27.335134720127595 ns     27.211004896327836 ns      25715190
  #BM_RWMutexInitializationWriteLockUnlock                      30.29120801891917 ns      30.12894761884541 ns      23244772
  #BM_SharedMutexInitializationReadLockUnlock                  56.699768519426065 ns     56.433009417474274 ns      12406617
  #BM_SharedMutexInitializationWriteLockUnlock                  57.55622814765764 ns     57.303949058279805 ns      12223851
  #BM_AudioUtilsMutexInitializationLockUnlock                   43.02546297081612 ns     42.823799365616175 ns      16355083
  #BM_AudioUtilsPIMutexInitializationLockUnlock                 47.94813033517548 ns     47.722581549497065 ns      14667687
  #BM_StdMutexBlockingConditionVariable/threads:2              26879.128794361706 ns      31117.70547138501 ns         37358
  #BM_AudioUtilsMutexBlockingConditionVariable/threads:2        46786.98106298265 ns      51810.75161375347 ns         15182
  #BM_AudioUtilsPIMutexBlockingConditionVariable/threads:2      48937.30165022134 ns      57147.57405000012 ns         20000
  #BM_StdMutexScopedLockUnlock/threads:1                        32.93981066359915 ns      32.76512553728174 ns      20414812
  #BM_StdMutexScopedLockUnlock/threads:2                        571.8094382500567 ns            1137.253613 ns       2000000
  #BM_StdMutexScopedLockUnlock/threads:4                       128.79479358731055 ns      404.5323853159404 ns       2347144
  #BM_StdMutexScopedLockUnlock/threads:8                       131.74327468115035 ns      517.3769108963708 ns       2534936
  #BM_RWMutexScopedReadLockUnlock/threads:1                    32.230652432400504 ns      32.08542964347514 ns      21723209
  #BM_RWMutexScopedReadLockUnlock/threads:2                     155.3515724990575 ns     307.43060200000036 ns       2000000
  #BM_RWMutexScopedReadLockUnlock/threads:4                     243.4595197344497 ns      961.0659840432863 ns        731980
  #BM_RWMutexScopedReadLockUnlock/threads:8                    253.54372642695628 ns     1952.4235494344487 ns        381920
  #BM_RWMutexScopedWriteLockUnlock/threads:1                    34.96165601863155 ns        34.794121014593 ns      20105476
  #BM_RWMutexScopedWriteLockUnlock/threads:2                   263.78236900200136 ns      514.6362324999996 ns       2000000
  #BM_RWMutexScopedWriteLockUnlock/threads:4                     527.032631925838 ns      1792.191760456376 ns       1008284
  #BM_RWMutexScopedWriteLockUnlock/threads:8                    483.5519374297695 ns     1863.0285003579795 ns        435784
  #BM_SharedMutexScopedReadLockUnlock/threads:1                  69.9000512364522 ns      69.56623691277518 ns       9561519
  #BM_SharedMutexScopedReadLockUnlock/threads:2                351.76991433261804 ns      695.5628134433418 ns       1410542
  #BM_SharedMutexScopedReadLockUnlock/threads:4                367.56704147658854 ns     1254.6058120544424 ns        771500
  #BM_SharedMutexScopedReadLockUnlock/threads:8                378.72729057260494 ns     1560.9589011997377 ns        470768
  #BM_SharedMutexScopedWriteLockUnlock/threads:1                71.71541684004359 ns       71.4271749213539 ns       8818020
  #BM_SharedMutexScopedWriteLockUnlock/threads:2               277.63172079762904 ns      515.4668504883387 ns       1055008
  #BM_SharedMutexScopedWriteLockUnlock/threads:4               2755.3780970431444 ns      6906.134785989446 ns        226344
  #BM_SharedMutexScopedWriteLockUnlock/threads:8               3699.5667828023215 ns     12626.038175000023 ns         80000
  #BM_AudioUtilsMutexScopedLockUnlock/threads:1                 65.17313525653955 ns       64.8294116126777 ns      10795219
  #BM_AudioUtilsMutexScopedLockUnlock/threads:2                 446.8434749672579 ns       885.912395366048 ns       1181022
  #BM_AudioUtilsMutexScopedLockUnlock/threads:4                333.62697061454804 ns     1070.9685904025976 ns        964928
  #BM_AudioUtilsMutexScopedLockUnlock/threads:8                 314.9844454113926 ns      1212.955095620149 ns        582304
  #BM_AudioUtilsPIMutexScopedLockUnlock/threads:1               65.65269811876958 ns      65.34601386374699 ns       9144281
  #BM_AudioUtilsPIMutexScopedLockUnlock/threads:2               5017.670372810213 ns      6042.387748137168 ns       2288452
  #BM_AudioUtilsPIMutexScopedLockUnlock/threads:4              29737.776158677327 ns      42942.53487809585 ns         20672
  #BM_AudioUtilsPIMutexScopedLockUnlock/threads:8               39162.52888815035 ns      49320.37778141362 ns         12224
  #BM_StdMutexReverseScopedLockUnlock/threads:1                  32.9542161313857 ns      32.80702810699402 ns      20720857
  #BM_StdMutexReverseScopedLockUnlock/threads:2                 60.71405982389486 ns     116.22175457775452 ns       5378730
  #BM_StdMutexReverseScopedLockUnlock/threads:4                130.27275630605487 ns     426.97113497151395 ns       2300604
  #BM_StdMutexReverseScopedLockUnlock/threads:8                146.68614021968642 ns      525.7952204915628 ns       1277872
  #BM_AudioUtilsMutexReverseScopedLockUnlock/threads:1          64.96217937278401 ns      64.67648684147889 ns       9147495
  #BM_AudioUtilsMutexReverseScopedLockUnlock/threads:2         315.65038050030125 ns      621.0967409999993 ns       2000000
  #BM_AudioUtilsMutexReverseScopedLockUnlock/threads:4         248.41254045888044 ns      727.5389901889789 ns       1138312
  #BM_AudioUtilsMutexReverseScopedLockUnlock/threads:8         327.52771167681186 ns     1223.1142029482817 ns        606184
  #BM_AudioUtilsPIMutexReverseScopedLockUnlock/threads:1        65.70167857940193 ns       65.4193651777703 ns       9300777
  #BM_AudioUtilsPIMutexReverseScopedLockUnlock/threads:2       2553.0187592521543 ns      3020.612901000003 ns       2000000
  #BM_AudioUtilsPIMutexReverseScopedLockUnlock/threads:4        59069.56778749645 ns      74846.59661112927 ns         24728
  #BM_AudioUtilsPIMutexReverseScopedLockUnlock/threads:8        28588.76596390947 ns      34629.68940916421 ns         19904
  #BM_empty_while                                              0.3527790119987912 ns     0.3510390169999908 ns    1000000000

*/

// ---

// Benchmark gettid().  The mutex class uses this to get the linux thread id.
static void BM_gettid(benchmark::State &state) {
    int32_t value = 0;
    while (state.KeepRunning()) {
        value ^= android::audio_utils::gettid_wrapper();  // ensure the return value used.
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

static void BM_thread_detach_async(benchmark::State &state) {
    while (state.KeepRunning()) {
        std::thread([](){}).detach();
    }
}

BENCHMARK(BM_thread_detach_async);

static void BM_thread_join_sync(benchmark::State &state) {
    while (state.KeepRunning()) {
        std::thread([](){}).join();
    }
}

BENCHMARK(BM_thread_join_sync);

// ---

// std::mutex is the reference mutex that we compare against.
// It is based on Bionic pthread_mutex* support.

// RWLock is a specialized Android mutex class based on
// Bionic pthread_rwlock* which in turn is based on the
// original ART shared reader mutex.

// Test shared read lock performance.
class RWReadMutex : private android::RWLock {
public:
    void lock() { readLock(); }
    bool try_lock() { return tryReadLock() == 0; }
    using android::RWLock::unlock;
};

// Test exclusive write lock performance.
class RWWriteMutex : private android::RWLock {
public:
    void lock() { writeLock(); }
    bool try_lock() { return tryWriteLock() == 0; }
    using android::RWLock::unlock;
};

// std::shared_mutex lock/unlock behavior is default exclusive.
// We subclass to create the shared reader equivalent.
//
// Unfortunately std::shared_mutex implementation can contend on an internal
// mutex with multiple readers (even with no writers), resulting in worse lock performance
// than other shared mutexes.
// This is due to the portability desire in the original reference implementation:
// https://www.open-std.org/jtc1/sc22/wg21/docs/papers/2007/n2406.html#shared_mutex_imp
class SharedReadMutex : private std::shared_mutex {
public:
    void lock() { std::shared_mutex::lock_shared(); }
    bool try_lock() { return std::shared_mutex::try_lock_shared(); }
    void unlock() { std::shared_mutex::unlock_shared(); }
};

// audio_utils mutex is designed to have mutex order checking, statistics,
// deadlock detection, and priority inheritance capabilities,
// so it is higher overhead than just the std::mutex that it is based upon.
//
// audio_utils mutex without priority inheritance.
class AudioMutex : public android::audio_utils::mutex {
public:
    AudioMutex() : android::audio_utils::mutex(false /* priority_inheritance */) {}
};

// audio_utils mutex with priority inheritance.
class AudioPIMutex : public android::audio_utils::mutex {
public:
    AudioPIMutex() : android::audio_utils::mutex(true /* priority_inheritance */) {}
};

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
// using the mutex.
BENCHMARK(BM_StdMutexLockUnlock);

static void BM_RWMutexReadLockUnlock(benchmark::State& state) {
    MutexLockUnlock<RWReadMutex>(state);
}

BENCHMARK(BM_RWMutexReadLockUnlock);

static void BM_RWMutexWriteLockUnlock(benchmark::State& state) {
    MutexLockUnlock<RWWriteMutex>(state);
}

BENCHMARK(BM_RWMutexWriteLockUnlock);

static void BM_SharedMutexReadLockUnlock(benchmark::State& state) {
    MutexLockUnlock<SharedReadMutex>(state);
}

BENCHMARK(BM_SharedMutexReadLockUnlock);

static void BM_SharedMutexWriteLockUnlock(benchmark::State& state) {
    MutexLockUnlock<std::shared_mutex>(state);
}

BENCHMARK(BM_SharedMutexWriteLockUnlock);

static void BM_AudioUtilsMutexLockUnlock(benchmark::State &state) {
    MutexLockUnlock<AudioMutex>(state);
}

BENCHMARK(BM_AudioUtilsMutexLockUnlock);

static void BM_AudioUtilsPIMutexLockUnlock(benchmark::State &state) {
    MutexLockUnlock<AudioPIMutex>(state);
}

BENCHMARK(BM_AudioUtilsPIMutexLockUnlock);

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
// using the mutex.
BENCHMARK(BM_StdMutexInitializationLockUnlock);

static void BM_RWMutexInitializationReadLockUnlock(benchmark::State& state) {
    MutexInitializationLockUnlock<RWReadMutex>(state);
}

BENCHMARK(BM_RWMutexInitializationReadLockUnlock);

static void BM_RWMutexInitializationWriteLockUnlock(benchmark::State& state) {
    MutexInitializationLockUnlock<RWWriteMutex>(state);
}

BENCHMARK(BM_RWMutexInitializationWriteLockUnlock);

static void BM_SharedMutexInitializationReadLockUnlock(benchmark::State& state) {
    MutexInitializationLockUnlock<SharedReadMutex>(state);
}

BENCHMARK(BM_SharedMutexInitializationReadLockUnlock);

static void BM_SharedMutexInitializationWriteLockUnlock(benchmark::State& state) {
    MutexInitializationLockUnlock<std::shared_mutex>(state);
}

BENCHMARK(BM_SharedMutexInitializationWriteLockUnlock);

static void BM_AudioUtilsMutexInitializationLockUnlock(benchmark::State &state) {
    MutexInitializationLockUnlock<AudioMutex>(state);
}

BENCHMARK(BM_AudioUtilsMutexInitializationLockUnlock);

static void BM_AudioUtilsPIMutexInitializationLockUnlock(benchmark::State &state) {
    MutexInitializationLockUnlock<AudioPIMutex>(state);
}

BENCHMARK(BM_AudioUtilsPIMutexInitializationLockUnlock);

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
// uses mutex, unique_lock, and condition_variable.
BENCHMARK(BM_StdMutexBlockingConditionVariable)->Threads(THREADS);

MutexBlockingConditionVariable<AudioMutex,
        android::audio_utils::unique_lock<AudioMutex>,
        android::audio_utils::condition_variable> CvAu;

static void BM_AudioUtilsMutexBlockingConditionVariable(benchmark::State &state) {
    CvAu.run(state);
}

BENCHMARK(BM_AudioUtilsMutexBlockingConditionVariable)->Threads(THREADS);

MutexBlockingConditionVariable<AudioPIMutex,
        android::audio_utils::unique_lock<AudioPIMutex>,
        android::audio_utils::condition_variable> CvAuPI;

static void BM_AudioUtilsPIMutexBlockingConditionVariable(benchmark::State &state) {
    CvAuPI.run(state);
}

BENCHMARK(BM_AudioUtilsPIMutexBlockingConditionVariable)->Threads(THREADS);

// ---

// Benchmark scoped_lock where two threads try to obtain the
// same 2 locks with the same initial acquisition order.
// This uses std::scoped_lock.

static constexpr size_t THREADS_SCOPED = 8;

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

BENCHMARK(BM_StdMutexScopedLockUnlock)->ThreadRange(1, THREADS_SCOPED);

MutexScopedLockUnlock<RWReadMutex,
        std::scoped_lock<RWReadMutex, RWReadMutex>> ScopedRwRead;

static void BM_RWMutexScopedReadLockUnlock(benchmark::State &state) {
    ScopedRwRead.run(state);
}

BENCHMARK(BM_RWMutexScopedReadLockUnlock)->ThreadRange(1, THREADS_SCOPED);

MutexScopedLockUnlock<RWWriteMutex,
        std::scoped_lock<RWWriteMutex, RWWriteMutex>> ScopedRwWrite;

static void BM_RWMutexScopedWriteLockUnlock(benchmark::State &state) {
    ScopedRwWrite.run(state);
}

BENCHMARK(BM_RWMutexScopedWriteLockUnlock)->ThreadRange(1, THREADS_SCOPED);

MutexScopedLockUnlock<SharedReadMutex,
        std::scoped_lock<SharedReadMutex, SharedReadMutex>> ScopedSharedRead;

static void BM_SharedMutexScopedReadLockUnlock(benchmark::State &state) {
    ScopedSharedRead.run(state);
}

BENCHMARK(BM_SharedMutexScopedReadLockUnlock)->ThreadRange(1, THREADS_SCOPED);

MutexScopedLockUnlock<std::shared_mutex,
        std::scoped_lock<std::shared_mutex, std::shared_mutex>> ScopedSharedWrite;

static void BM_SharedMutexScopedWriteLockUnlock(benchmark::State &state) {
    ScopedSharedWrite.run(state);
}

BENCHMARK(BM_SharedMutexScopedWriteLockUnlock)->ThreadRange(1, THREADS_SCOPED);

MutexScopedLockUnlock<AudioMutex,
        android::audio_utils::scoped_lock<
                AudioMutex, AudioMutex>> ScopedAu;

static void BM_AudioUtilsMutexScopedLockUnlock(benchmark::State &state) {
    ScopedAu.run(state);
}

BENCHMARK(BM_AudioUtilsMutexScopedLockUnlock)->ThreadRange(1, THREADS_SCOPED);

MutexScopedLockUnlock<AudioPIMutex,
        android::audio_utils::scoped_lock<
                AudioPIMutex, AudioPIMutex>> ScopedAuPI;

static void BM_AudioUtilsPIMutexScopedLockUnlock(benchmark::State &state) {
    ScopedAuPI.run(state);
}

BENCHMARK(BM_AudioUtilsPIMutexScopedLockUnlock)->ThreadRange(1, THREADS_SCOPED);

MutexScopedLockUnlock<std::mutex,
        std::scoped_lock<std::mutex, std::mutex>> ReverseScopedStd(true);

static void BM_StdMutexReverseScopedLockUnlock(benchmark::State &state) {
    ReverseScopedStd.run(state);
}

// Benchmark scoped_lock with odd thread having reversed scoped_lock mutex acquisition order.
// This uses std::scoped_lock.
BENCHMARK(BM_StdMutexReverseScopedLockUnlock)->ThreadRange(1, THREADS_SCOPED);

MutexScopedLockUnlock<AudioMutex,
        android::audio_utils::scoped_lock<
                AudioMutex, AudioMutex>> ReverseScopedAu(true);

static void BM_AudioUtilsMutexReverseScopedLockUnlock(benchmark::State &state) {
    ReverseScopedAu.run(state);
}

BENCHMARK(BM_AudioUtilsMutexReverseScopedLockUnlock)->ThreadRange(1, THREADS_SCOPED);

MutexScopedLockUnlock<AudioPIMutex,
        android::audio_utils::scoped_lock<
                AudioPIMutex, AudioPIMutex>> ReverseScopedAuPI(true);

static void BM_AudioUtilsPIMutexReverseScopedLockUnlock(benchmark::State &state) {
    ReverseScopedAuPI.run(state);
}

BENCHMARK(BM_AudioUtilsPIMutexReverseScopedLockUnlock)->ThreadRange(1, THREADS_SCOPED);

static void BM_empty_while(benchmark::State &state) {

    while (state.KeepRunning()) {
        ;
    }
    ALOGD("%s", android::audio_utils::mutex::all_stats_to_string().c_str());
}

// Benchmark to see the cost of doing nothing.
BENCHMARK(BM_empty_while);

BENCHMARK_MAIN();
