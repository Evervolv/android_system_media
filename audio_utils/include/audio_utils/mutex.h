/*
 * Copyright (C) 2023 The Android Open Source Project
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

#include <android-base/thread_annotations.h>
#include <utils/Log.h>
#include <utils/Timers.h>

#include <cmath>
#include <mutex>
#include <unistd.h>

#pragma push_macro("LOG_TAG")
#undef LOG_TAG
#define LOG_TAG "audio_utils::mutex"

namespace android::audio_utils {

// Define global capabilities for thread-safety annotation.
//
// These can be manually modified, or
// compile generate_mutex_order.cpp in the tests directory
// to generate this.

// --- Begin generated section

// Lock order
enum class MutexOrder : uint32_t {
    kEffectHandle_Mutex = 0,
    kEffectBase_PolicyMutex = 1,
    kAudioFlinger_Mutex = 2,
    kAudioFlinger_HardwareMutex = 3,
    kDeviceEffectManager_Mutex = 4,
    kPatchCommandThread_Mutex = 5,
    kThreadBase_Mutex = 6,
    kAudioFlinger_ClientMutex = 7,
    kMelReporter_Mutex = 8,
    kEffectChain_Mutex = 9,
    kDeviceEffectProxy_ProxyMutex = 10,
    kEffectBase_Mutex = 11,
    kAudioFlinger_UnregisteredWritersMutex = 12,
    kAsyncCallbackThread_Mutex = 13,
    kConfigEvent_Mutex = 14,
    kOutputTrack_TrackMetadataMutex = 15,
    kPassthruPatchRecord_ReadMutex = 16,
    kPatchCommandThread_ListenerMutex = 17,
    kPlaybackThread_AudioTrackCbMutex = 18,
    kMediaLogNotifier_Mutex = 19,
    kOtherMutex = 20,
    kSize = 21,
};

// Lock by name
inline constexpr const char* const gMutexNames[] = {
    "EffectHandle_Mutex",
    "EffectBase_PolicyMutex",
    "AudioFlinger_Mutex",
    "AudioFlinger_HardwareMutex",
    "DeviceEffectManager_Mutex",
    "PatchCommandThread_Mutex",
    "ThreadBase_Mutex",
    "AudioFlinger_ClientMutex",
    "MelReporter_Mutex",
    "EffectChain_Mutex",
    "DeviceEffectProxy_ProxyMutex",
    "EffectBase_Mutex",
    "AudioFlinger_UnregisteredWritersMutex",
    "AsyncCallbackThread_Mutex",
    "ConfigEvent_Mutex",
    "OutputTrack_TrackMetadataMutex",
    "PassthruPatchRecord_ReadMutex",
    "PatchCommandThread_ListenerMutex",
    "PlaybackThread_AudioTrackCbMutex",
    "MediaLogNotifier_Mutex",
    "OtherMutex",
};

// Forward declarations
class AudioMutexAttributes;
template <typename T> class mutex_impl;
using mutex = mutex_impl<AudioMutexAttributes>;

// Capabilities in priority order
// (declaration only, value is nullptr)
inline mutex* EffectHandle_Mutex;
inline mutex* EffectBase_PolicyMutex
        ACQUIRED_AFTER(android::audio_utils::EffectHandle_Mutex);
inline mutex* AudioFlinger_Mutex
        ACQUIRED_AFTER(android::audio_utils::EffectBase_PolicyMutex);
inline mutex* AudioFlinger_HardwareMutex
        ACQUIRED_AFTER(android::audio_utils::AudioFlinger_Mutex);
inline mutex* DeviceEffectManager_Mutex
        ACQUIRED_AFTER(android::audio_utils::AudioFlinger_HardwareMutex);
inline mutex* PatchCommandThread_Mutex
        ACQUIRED_AFTER(android::audio_utils::DeviceEffectManager_Mutex);
inline mutex* ThreadBase_Mutex
        ACQUIRED_AFTER(android::audio_utils::PatchCommandThread_Mutex);
inline mutex* AudioFlinger_ClientMutex
        ACQUIRED_AFTER(android::audio_utils::ThreadBase_Mutex);
inline mutex* MelReporter_Mutex
        ACQUIRED_AFTER(android::audio_utils::AudioFlinger_ClientMutex);
inline mutex* EffectChain_Mutex
        ACQUIRED_AFTER(android::audio_utils::MelReporter_Mutex);
inline mutex* DeviceEffectProxy_ProxyMutex
        ACQUIRED_AFTER(android::audio_utils::EffectChain_Mutex);
inline mutex* EffectBase_Mutex
        ACQUIRED_AFTER(android::audio_utils::DeviceEffectProxy_ProxyMutex);
inline mutex* AudioFlinger_UnregisteredWritersMutex
        ACQUIRED_AFTER(android::audio_utils::EffectBase_Mutex);
inline mutex* AsyncCallbackThread_Mutex
        ACQUIRED_AFTER(android::audio_utils::AudioFlinger_UnregisteredWritersMutex);
inline mutex* ConfigEvent_Mutex
        ACQUIRED_AFTER(android::audio_utils::AsyncCallbackThread_Mutex);
inline mutex* OutputTrack_TrackMetadataMutex
        ACQUIRED_AFTER(android::audio_utils::ConfigEvent_Mutex);
inline mutex* PassthruPatchRecord_ReadMutex
        ACQUIRED_AFTER(android::audio_utils::OutputTrack_TrackMetadataMutex);
inline mutex* PatchCommandThread_ListenerMutex
        ACQUIRED_AFTER(android::audio_utils::PassthruPatchRecord_ReadMutex);
inline mutex* PlaybackThread_AudioTrackCbMutex
        ACQUIRED_AFTER(android::audio_utils::PatchCommandThread_ListenerMutex);
inline mutex* MediaLogNotifier_Mutex
        ACQUIRED_AFTER(android::audio_utils::PlaybackThread_AudioTrackCbMutex);
inline mutex* OtherMutex
        ACQUIRED_AFTER(android::audio_utils::MediaLogNotifier_Mutex);

// Exclusion by capability
#define EXCLUDES_BELOW_OtherMutex
#define EXCLUDES_OtherMutex \
    EXCLUDES(android::audio_utils::OtherMutex) \
    EXCLUDES_BELOW_OtherMutex

#define EXCLUDES_BELOW_MediaLogNotifier_Mutex \
    EXCLUDES_OtherMutex
#define EXCLUDES_MediaLogNotifier_Mutex \
    EXCLUDES(android::audio_utils::MediaLogNotifier_Mutex) \
    EXCLUDES_BELOW_MediaLogNotifier_Mutex

#define EXCLUDES_BELOW_PlaybackThread_AudioTrackCbMutex \
    EXCLUDES_MediaLogNotifier_Mutex
#define EXCLUDES_PlaybackThread_AudioTrackCbMutex \
    EXCLUDES(android::audio_utils::PlaybackThread_AudioTrackCbMutex) \
    EXCLUDES_BELOW_PlaybackThread_AudioTrackCbMutex

#define EXCLUDES_BELOW_PatchCommandThread_ListenerMutex \
    EXCLUDES_PlaybackThread_AudioTrackCbMutex
#define EXCLUDES_PatchCommandThread_ListenerMutex \
    EXCLUDES(android::audio_utils::PatchCommandThread_ListenerMutex) \
    EXCLUDES_BELOW_PatchCommandThread_ListenerMutex

#define EXCLUDES_BELOW_PassthruPatchRecord_ReadMutex \
    EXCLUDES_PatchCommandThread_ListenerMutex
#define EXCLUDES_PassthruPatchRecord_ReadMutex \
    EXCLUDES(android::audio_utils::PassthruPatchRecord_ReadMutex) \
    EXCLUDES_BELOW_PassthruPatchRecord_ReadMutex

#define EXCLUDES_BELOW_OutputTrack_TrackMetadataMutex \
    EXCLUDES_PassthruPatchRecord_ReadMutex
#define EXCLUDES_OutputTrack_TrackMetadataMutex \
    EXCLUDES(android::audio_utils::OutputTrack_TrackMetadataMutex) \
    EXCLUDES_BELOW_OutputTrack_TrackMetadataMutex

#define EXCLUDES_BELOW_ConfigEvent_Mutex \
    EXCLUDES_OutputTrack_TrackMetadataMutex
#define EXCLUDES_ConfigEvent_Mutex \
    EXCLUDES(android::audio_utils::ConfigEvent_Mutex) \
    EXCLUDES_BELOW_ConfigEvent_Mutex

#define EXCLUDES_BELOW_AsyncCallbackThread_Mutex \
    EXCLUDES_ConfigEvent_Mutex
#define EXCLUDES_AsyncCallbackThread_Mutex \
    EXCLUDES(android::audio_utils::AsyncCallbackThread_Mutex) \
    EXCLUDES_BELOW_AsyncCallbackThread_Mutex

#define EXCLUDES_BELOW_AudioFlinger_UnregisteredWritersMutex \
    EXCLUDES_AsyncCallbackThread_Mutex
#define EXCLUDES_AudioFlinger_UnregisteredWritersMutex \
    EXCLUDES(android::audio_utils::AudioFlinger_UnregisteredWritersMutex) \
    EXCLUDES_BELOW_AudioFlinger_UnregisteredWritersMutex

#define EXCLUDES_BELOW_EffectBase_Mutex \
    EXCLUDES_AudioFlinger_UnregisteredWritersMutex
#define EXCLUDES_EffectBase_Mutex \
    EXCLUDES(android::audio_utils::EffectBase_Mutex) \
    EXCLUDES_BELOW_EffectBase_Mutex

#define EXCLUDES_BELOW_DeviceEffectProxy_ProxyMutex \
    EXCLUDES_EffectBase_Mutex
#define EXCLUDES_DeviceEffectProxy_ProxyMutex \
    EXCLUDES(android::audio_utils::DeviceEffectProxy_ProxyMutex) \
    EXCLUDES_BELOW_DeviceEffectProxy_ProxyMutex

#define EXCLUDES_BELOW_EffectChain_Mutex \
    EXCLUDES_DeviceEffectProxy_ProxyMutex
#define EXCLUDES_EffectChain_Mutex \
    EXCLUDES(android::audio_utils::EffectChain_Mutex) \
    EXCLUDES_BELOW_EffectChain_Mutex

#define EXCLUDES_BELOW_MelReporter_Mutex \
    EXCLUDES_EffectChain_Mutex
#define EXCLUDES_MelReporter_Mutex \
    EXCLUDES(android::audio_utils::MelReporter_Mutex) \
    EXCLUDES_BELOW_MelReporter_Mutex

#define EXCLUDES_BELOW_AudioFlinger_ClientMutex \
    EXCLUDES_MelReporter_Mutex
#define EXCLUDES_AudioFlinger_ClientMutex \
    EXCLUDES(android::audio_utils::AudioFlinger_ClientMutex) \
    EXCLUDES_BELOW_AudioFlinger_ClientMutex

#define EXCLUDES_BELOW_ThreadBase_Mutex \
    EXCLUDES_AudioFlinger_ClientMutex
#define EXCLUDES_ThreadBase_Mutex \
    EXCLUDES(android::audio_utils::ThreadBase_Mutex) \
    EXCLUDES_BELOW_ThreadBase_Mutex

#define EXCLUDES_BELOW_PatchCommandThread_Mutex \
    EXCLUDES_ThreadBase_Mutex
#define EXCLUDES_PatchCommandThread_Mutex \
    EXCLUDES(android::audio_utils::PatchCommandThread_Mutex) \
    EXCLUDES_BELOW_PatchCommandThread_Mutex

#define EXCLUDES_BELOW_DeviceEffectManager_Mutex \
    EXCLUDES_PatchCommandThread_Mutex
#define EXCLUDES_DeviceEffectManager_Mutex \
    EXCLUDES(android::audio_utils::DeviceEffectManager_Mutex) \
    EXCLUDES_BELOW_DeviceEffectManager_Mutex

#define EXCLUDES_BELOW_AudioFlinger_HardwareMutex \
    EXCLUDES_DeviceEffectManager_Mutex
#define EXCLUDES_AudioFlinger_HardwareMutex \
    EXCLUDES(android::audio_utils::AudioFlinger_HardwareMutex) \
    EXCLUDES_BELOW_AudioFlinger_HardwareMutex

#define EXCLUDES_BELOW_AudioFlinger_Mutex \
    EXCLUDES_AudioFlinger_HardwareMutex
#define EXCLUDES_AudioFlinger_Mutex \
    EXCLUDES(android::audio_utils::AudioFlinger_Mutex) \
    EXCLUDES_BELOW_AudioFlinger_Mutex

#define EXCLUDES_BELOW_EffectBase_PolicyMutex \
    EXCLUDES_AudioFlinger_Mutex
#define EXCLUDES_EffectBase_PolicyMutex \
    EXCLUDES(android::audio_utils::EffectBase_PolicyMutex) \
    EXCLUDES_BELOW_EffectBase_PolicyMutex

#define EXCLUDES_BELOW_EffectHandle_Mutex \
    EXCLUDES_EffectBase_PolicyMutex
#define EXCLUDES_EffectHandle_Mutex \
    EXCLUDES(android::audio_utils::EffectHandle_Mutex) \
    EXCLUDES_BELOW_EffectHandle_Mutex

#define EXCLUDES_AUDIO_ALL \
    EXCLUDES_EffectHandle_Mutex

// --- End generated section

/**
 * AudioMutexAttributes is a collection of types and constexpr configuration
 * used for the Android audio mutex.
 *
 * A different AudioMutexAttributes configuration will instantiate a completely
 * independent set of mutex strategies, statics and thread locals,
 * for a different type of mutexes.
 */

class AudioMutexAttributes {
public:
    // Order types, name arrays.
    using order_t = MutexOrder;
    static constexpr auto& order_names_ = gMutexNames;
    static constexpr size_t order_size_ = static_cast<size_t>(MutexOrder::kSize);
    static constexpr order_t order_default_ = MutexOrder::kOtherMutex;

    // verify order information
    static_assert(std::size(order_names_) == order_size_);
    static_assert(static_cast<size_t>(order_default_) < order_size_);

    // Set mutex_tracking_enabled_ to true to enable mutex
    // statistics and debugging (order checking) features.
    static constexpr bool mutex_tracking_enabled_ = true;
};

/**
 * Helper method to accumulate floating point values to an atomic
 * prior to C++23 support of atomic<float> atomic<double> accumulation.
 */
template <typename AccumulateType, typename ValueType>
void atomic_add_to(std::atomic<AccumulateType> &dst, ValueType src) {
    static_assert(std::atomic<AccumulateType>::is_always_lock_free);
    AccumulateType expected;
    do {
        expected = dst;
    } while (!dst.compare_exchange_weak(expected, expected + src));
}

/**
 * mutex_stat is a struct composed of atomic members associated
 * with usage of a particular mutex order.
 *
 * Access of a snapshot of this does not have a global lock, so the reader
 * may experience temporal shear. Use of this by a different reader thread
 * is for informative purposes only.
 */

// CounterType == uint64_t, AccumulatorType == double
template <typename CounterType, typename AccumulatorType>
struct mutex_stat {
    static_assert(std::is_floating_point_v<AccumulatorType>);
    static_assert(std::is_integral_v<CounterType>);
    std::atomic<CounterType> locks = 0;        // number of times locked
    std::atomic<CounterType> unlocks = 0;      // number of times unlocked
    std::atomic<CounterType> waits = 0;         // number of locks that waitedwa
    std::atomic<AccumulatorType> wait_sum_ns = 0.;    // sum of time waited.
    std::atomic<AccumulatorType> wait_sumsq_ns = 0.;  // sumsq of time waited.

    template <typename WaitTimeType>
    void add_wait_time(WaitTimeType wait_ns) {
        AccumulatorType value_ns = wait_ns;
        atomic_add_to(wait_sum_ns, value_ns);
        atomic_add_to(wait_sumsq_ns, value_ns * value_ns);
    }

    std::string to_string() const {
        CounterType uncontested = locks - waits;
        AccumulatorType recip = waits == 0 ? 0. : 1. / waits;
        AccumulatorType avg_wait_ms = waits == 0 ? 0. : wait_sum_ns * 1e-6 * recip;
        AccumulatorType std_wait_ms = waits < 2 ? 0. :
                std::sqrt(wait_sumsq_ns * recip * 1e-12 - avg_wait_ms * avg_wait_ms);
        return std::string("locks: ").append(std::to_string(locks))
            .append("\nuncontested: ").append(std::to_string(uncontested))
            .append("\nwaits: ").append(std::to_string(waits))
            .append("\nunlocks: ").append(std::to_string(unlocks))
            .append("\navg_wait_ms: ").append(std::to_string(avg_wait_ms))
            .append("\nstd_wait_ms: ").append(std::to_string(std_wait_ms))
            .append("\n");
    }
};

// audio_utils::mutex, audio_utils::lock_guard, audio_utils::unique_lock,
// and audio_utils::condition_variable are method compatible versions
// of std::mutex, std::lock_guard, std::unique_lock, and std::condition_variable
// for optimizing audio thread performance and debugging.
//
// Note: we do not use std::timed_mutex as its Clang library implementation
// is inefficient.  One is better off making a custom timed implementation using
// pthread_mutex_timedlock() on the mutex::native_handle().

extern bool mutex_get_enable_flag();

template <typename Attributes>
class CAPABILITY("mutex") mutex_impl {
public:
    // We use composition here.
    // No copy/move ctors as the member std::mutex has it deleted.
    mutex_impl(typename Attributes::order_t order = Attributes::order_default_)
        : order_(order)
        , stat_{mutex_stat_[static_cast<size_t>(order)]}
    {
        LOG_ALWAYS_FATAL_IF(static_cast<size_t>(order) >= Attributes::order_size_,
                "mutex order %u is equal to or greater than order limit:%zu",
                order, Attributes::order_size_);

        if (!mutex_get_enable_flag()) return;

        pthread_mutexattr_t attr;
        int ret = pthread_mutexattr_init(&attr);
        if (ret != 0) {
            ALOGW("%s, pthread_mutexattr_init returned %d", __func__, ret);
            return;
        }

        ret = pthread_mutexattr_setprotocol(&attr, PTHREAD_PRIO_INHERIT);
        if (ret != 0) {
            ALOGW("%s, pthread_mutexattr_setprotocol returned %d", __func__, ret);
            return;
        }

        // use of the native_handle() is implementation defined.
        const auto handle = m_.native_handle();
        ret = pthread_mutex_init(handle, &attr);
        if (ret != 0) {
            ALOGW("%s, pthread_mutex_init returned %d", __func__, ret);
        }
        ALOGV("%s: audio_mutex initialized: ret:%d  order:%u", __func__, ret, order_);
    }

    ~mutex_impl() {
        // Note: std::mutex behavior is undefined if released holding ownership.
    }

    auto native_handle() {
        return m_.native_handle();
    }

    void lock() ACQUIRE() {
        if (!m_.try_lock()) {  // if we directly use futex, we can optimize this with m_.lock().
            // lock_scoped_stat_t accumulates waiting time for the mutex lock call.
            lock_scoped_stat_t ls(*this);
            m_.lock();
        }
        lock_scoped_stat_t::post_lock(*this);
    }

    void unlock() RELEASE() {
        lock_scoped_stat_t::pre_unlock(*this);
        m_.unlock();
    }

    bool try_lock(int64_t timeout_ns = 0) TRY_ACQUIRE(true) {
        if (timeout_ns <= 0) {
            if (!m_.try_lock()) return false;
        } else {
            const int64_t deadline_ns = timeout_ns + systemTime(SYSTEM_TIME_REALTIME);
            const struct timespec ts = {
                .tv_sec = static_cast<time_t>(deadline_ns / 1'000'000'000),
                .tv_nsec = static_cast<long>(deadline_ns % 1'000'000'000),
            };
            lock_scoped_stat_t ls(*this);
            if (pthread_mutex_timedlock(m_.native_handle(), &ts) != 0) {
                ls.ignoreWaitTime();  // didn't get lock, don't count wait time
                return false;
            }
        }
        lock_scoped_stat_t::post_lock(*this);
        return true;
    }

    // additional method to obtain the underlying std::mutex.
    std::mutex& std_mutex() {
        return m_;
    }

    using mutex_stat_t = mutex_stat<uint64_t, double>;

    mutex_stat_t& get_stat() const {
        return stat_;
    }

    /**
     * Returns the locking statistics per mutex capability category.
     */
    static std::string all_stats_to_string() {
        std::string out("mutex stats: priority inheritance ");
        out.append(mutex_get_enable_flag() ? "enabled" : "disabled")
            .append("\n");
        for (size_t i = 0; i < std::size(mutex_stat_); ++i) {
            if (mutex_stat_[i].locks != 0) {
                out.append("Capability: ").append(gMutexNames[i]).append("\n")
                    .append(mutex_stat_[i].to_string());
            }
        }
        return out;
    }

    // helper class for registering statistics for a mutex lock.

    class lock_scoped_stat_enabled {
    public:
        explicit lock_scoped_stat_enabled(mutex& m)
            : mutex_(m)
            , time_(systemTime()) {
           ++mutex_.stat_.waits;
        }

        ~lock_scoped_stat_enabled() {
           if (!discard_wait_time_) mutex_.stat_.add_wait_time(systemTime() - time_);
        }

        void ignoreWaitTime() {
            discard_wait_time_ = true;
        }

        static void pre_unlock(mutex& m) {
            ++m.stat_.unlocks;
        }

        static void post_lock(mutex& m) {
            ++m.stat_.locks;
        }

    private:
        mutex& mutex_;
        const int64_t time_;
        bool discard_wait_time_ = false;
    };

    class lock_scoped_stat_disabled {
    public:
        explicit lock_scoped_stat_disabled(mutex&) {}

        void ignoreWaitTime() {}

        static void pre_unlock(mutex&) {}

        static void post_lock(mutex&) {}
    };

    using lock_scoped_stat_t = std::conditional_t<Attributes::mutex_tracking_enabled_,
            lock_scoped_stat_enabled, lock_scoped_stat_disabled>;

    // helper class for registering statistics for a cv wait.
    class cv_wait_scoped_stat_enabled {
    public:
        explicit cv_wait_scoped_stat_enabled(mutex& m) : mutex_(m) {
            ++mutex_.stat_.unlocks;
        }

        ~cv_wait_scoped_stat_enabled() {
            ++mutex_.stat_.locks;
        }
    private:
        mutex& mutex_;
    };

    class cv_wait_scoped_stat_disabled {
        explicit cv_wait_scoped_stat_disabled(mutex&) {}
    };

    using cv_wait_scoped_stat_t = std::conditional_t<Attributes::mutex_tracking_enabled_,
            cv_wait_scoped_stat_enabled, cv_wait_scoped_stat_disabled>;

private:

    std::mutex m_;
    const typename Attributes::order_t order_;
    mutex_stat_t& stat_;  // set in ctor

    // Per-process mutex statistics, one instance per template typename.
    static inline mutex_stat_t mutex_stat_[Attributes::order_size_];
};

// audio_utils::lock_guard only works with the defined mutex.
using lock_guard = std::lock_guard<mutex>;

// audio_utils::unique_lock is based on std::unique_lock<std::mutex>
// because std::condition_variable is optimized for std::unique_lock<std::mutex>
//
// Note: std::unique_lock **does not** have thread safety annotations.
// We annotate correctly here.  Essentially, this is the same as an annotated
// using unique_lock = std::unique_lock<std::mutex>;
//
// We omit swap(), release() and move methods which don't have thread
// safety annotations.

class SCOPED_CAPABILITY unique_lock {
public:
    explicit unique_lock(mutex& m) ACQUIRE(m)
        : ul_(m.std_mutex(), std::defer_lock)
        , mutex_(m) {
        lock();
    }

    ~unique_lock() RELEASE() {
        if (held) unlock();
    }

    void lock() ACQUIRE() {
        if (!ul_.try_lock()) {
            mutex::lock_scoped_stat_t ls(mutex_);
            ul_.lock();
        }
        mutex::lock_scoped_stat_t::post_lock(mutex_);
        held = true;
    }

    void unlock() RELEASE() {
        mutex::lock_scoped_stat_t::pre_unlock(mutex_);
        held = false;
        ul_.unlock();
    }

    bool try_lock() TRY_ACQUIRE(true) {
        if (!ul_.try_lock()) return false;
        mutex::lock_scoped_stat_t::post_lock(mutex_);
        held = true;
        return true;
    }

    template<class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep,Period>& timeout_duration)
            TRY_ACQUIRE(true) {
        if (!ul_.try_lock_for(timeout_duration)) return false;
        mutex::lock_scoped_stat_t::post_lock(mutex_);
        held = true;
        return true;
    }

    template<class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock,Duration>& timeout_time)
            TRY_ACQUIRE(true) {
        if (!ul_.try_lock_until(timeout_time)) return false;
        mutex::lock_scoped_stat_t::post_lock(mutex_);
        held = true;
        return true;
    }

    // additional method to obtain the underlying std::unique_lock
    std::unique_lock<std::mutex>& std_unique_lock() {
        return ul_;
    }

    // additional method to obtain the underlying mutex
    mutex& native_mutex() {
        return mutex_;
    }

private:
    std::unique_lock<std::mutex> ul_;
    bool held = false;
    mutex& mutex_;
};

// audio_utils::condition_variable uses the optimized version of
// std::condition_variable for std::unique_lock<std::mutex>
// It is possible to use std::condition_variable_any for a generic mutex type,
// but it is less efficient.

class condition_variable {
public:
    void notify_one() noexcept {
        cv_.notify_one();
    }

    void notify_all() noexcept {
        cv_.notify_all();
    }

    void wait(unique_lock& lock) {
        mutex::cv_wait_scoped_stat_t ws(lock.native_mutex());
        cv_.wait(lock.std_unique_lock());
    }

    template<typename Predicate>
    void wait(unique_lock& lock, Predicate stop_waiting) {
        mutex::cv_wait_scoped_stat_t ws(lock.native_mutex());
        cv_.wait(lock.std_unique_lock(), std::move(stop_waiting));
    }

    template<typename Rep, typename Period>
    std::cv_status wait_for(unique_lock& lock,
            const std::chrono::duration<Rep, Period>& rel_time) {
        mutex::cv_wait_scoped_stat_t ws(lock.native_mutex());
        return cv_.wait_for(lock.std_unique_lock(), rel_time);
    }

    template<typename Rep, typename Period, typename Predicate>
    bool wait_for(unique_lock& lock,
            const std::chrono::duration<Rep, Period>& rel_time,
            Predicate stop_waiting) {
        mutex::cv_wait_scoped_stat_t ws(lock.native_mutex());
        return cv_.wait_for(lock.std_unique_lock(), rel_time, std::move(stop_waiting));
    }

    template<typename Clock, typename Duration>
    std::cv_status wait_until(unique_lock& lock,
            const std::chrono::time_point<Clock, Duration>& timeout_time) {
        mutex::cv_wait_scoped_stat_t ws(lock.native_mutex());
        return cv_.wait_until(lock.std_unique_lock(), timeout_time);
    }

    template<typename Clock, typename Duration, typename Predicate>
    bool wait_until(unique_lock& lock,
            const std::chrono::time_point<Clock, Duration>& timeout_time,
            Predicate stop_waiting) {
        mutex::cv_wait_scoped_stat_t ws(lock.native_mutex());
        return cv_.wait_until(lock.std_unique_lock(), timeout_time, std::move(stop_waiting));
    }

    auto native_handle() {
        return cv_.native_handle();
    }

private:
    std::condition_variable cv_;
};

// audio_utils::scoped_lock has proper thread safety annotations.
// std::scoped_lock does not have thread safety annotations for greater than 1 lock
// since the variadic template doesn't convert to the variadic macro def.

template <typename ...Mutexes>
class scoped_lock;

template <typename Mutex1>
class SCOPED_CAPABILITY scoped_lock<Mutex1>
    : private std::scoped_lock<Mutex1> {
public:
    explicit scoped_lock(Mutex1& m) ACQUIRE(m) : std::scoped_lock<Mutex1>(m) {}
    ~scoped_lock() RELEASE() = default;
};

template <typename Mutex1, typename Mutex2>
class SCOPED_CAPABILITY scoped_lock<Mutex1, Mutex2>
    : private std::scoped_lock<Mutex1, Mutex2> {
public:
    scoped_lock(Mutex1& m1, Mutex2& m2) ACQUIRE(m1, m2)
        : std::scoped_lock<Mutex1, Mutex2>(m1, m2) {}
    ~scoped_lock() RELEASE() = default;
};

template <typename Mutex1, typename Mutex2, typename Mutex3>
class SCOPED_CAPABILITY scoped_lock<Mutex1, Mutex2, Mutex3>
    : private std::scoped_lock<Mutex1, Mutex2, Mutex3> {
public:
    scoped_lock(Mutex1& m1, Mutex2& m2, Mutex3& m3) ACQUIRE(m1, m2, m3)
        : std::scoped_lock<Mutex1, Mutex2, Mutex3>(m1, m2, m3) {}
    ~scoped_lock() RELEASE() = default;
};

template <typename ...Mutexes>
class scoped_lock : private std::scoped_lock<Mutexes...> {
public:
    scoped_lock(Mutexes&... mutexes)
      : std::scoped_lock<Mutexes...>(mutexes...) {}
};

// audio_utils::lock_guard_no_thread_safety_analysis is used to lock
// the second mutex when the same global capability is aliased
// to 2 (or more) different mutexes.
//
// Example usage:
//
// // Suppose the interface IAfThreadBase::mutex() returns a global capability
// // ThreadBase_Mutex.
//
// sp<IAfThreadBase> srcThread, dstThread;
//
// lock_guard(srcThread->mutex());  // acquires global capability ThreadBase_Mutex;
// ...
// lock_guard_no_thread_safety_analysis(  // lock_guard would fail here as
//         dstThread->mutex());           // the same global capability is assigned to
//                                        // dstThread->mutex().
//                                        // lock_guard_no_thread_safety_analysis
//                                        // prevents a thread safety error.

template<typename Mutex1>
class lock_guard_no_thread_safety_analysis : private std::lock_guard<Mutex1> {
public:
    lock_guard_no_thread_safety_analysis(Mutex1& m) : std::lock_guard<Mutex1>(m) {}
};

} // namespace android::audio_utils

#pragma pop_macro("LOG_TAG")
