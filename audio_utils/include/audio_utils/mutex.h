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

#include <mutex>

#pragma push_macro("LOG_TAG")
#undef LOG_TAG
#define LOG_TAG "audio_utils::mutex"

namespace android::audio_utils {

// audio_utils::mutex, audio_utils::lock_guard, audio_utils::unique_lock,
// and audio_utils::condition_variable are method compatible versions
// of std::mutex, std::lock_guard, std::unique_lock, and std::condition_variable
// for optimizing audio thread performance and debugging.
//
// Note: we do not use std::timed_mutex as its Clang library implementation
// is inefficient.  One is better off making a custom timed implementation using
// pthread_mutex_timedlock() on the mutex::native_handle().

class CAPABILITY("mutex") mutex {
public:
    // We use composition here.
    // No copy/move ctors as the member std::mutex has it deleted.
    mutex() {
        if (!get_enable_flag()) return;

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
        auto handle = m_.native_handle();
        ret = pthread_mutex_init(handle, &attr);
        if (ret != 0) {
            ALOGW("%s, pthread_mutex_init returned %d", __func__, ret);
        }
        ALOGV("%s: audio_mutex initialized: %d", __func__, ret);
    }

    auto native_handle() {
        return m_.native_handle();
    }

    void lock() ACQUIRE() {
        m_.lock();
    }

    void unlock() RELEASE() {
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
            if (pthread_mutex_timedlock(m_.native_handle(), &ts) != 0) return false;
        }
        return true;
    }

    // additional method to obtain the underlying std::mutex.
    std::mutex& std_mutex() {
        return m_;
    }

private:
    static bool get_enable_flag();
    std::mutex m_;
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
        : ul_(m.std_mutex()) {}

    ~unique_lock() RELEASE() = default;

    void lock() ACQUIRE() {
       ul_.lock();
    }

    void unlock() RELEASE() {
        ul_.unlock();
    }

    bool try_lock() TRY_ACQUIRE(true) {
        return ul_.try_lock();
    }

    template<class Rep, class Period>
    bool try_lock_for(const std::chrono::duration<Rep,Period>& timeout_duration)
            TRY_ACQUIRE(true) {
        return ul_.try_lock_for(timeout_duration);
    }

    template<class Clock, class Duration>
    bool try_lock_until(const std::chrono::time_point<Clock,Duration>& timeout_time)
            TRY_ACQUIRE(true) {
        return ul_.try_lock_until(timeout_time);
    }

    // additional method to obtain the underlying std::unique_lock
    std::unique_lock<std::mutex>& std_unique_lock() {
        return ul_;
    }

private:
    std::unique_lock<std::mutex> ul_;
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
        cv_.wait(lock.std_unique_lock());
    }

    template<typename Predicate>
    void wait(unique_lock& lock, Predicate stop_waiting) {
        cv_.wait(lock.std_unique_lock(), std::move(stop_waiting));
    }

    template<typename Rep, typename Period>
    std::cv_status wait_for(unique_lock& lock,
            const std::chrono::duration<Rep, Period>& rel_time) {
        return cv_.wait_for(lock.std_unique_lock(), rel_time);
    }

    template<typename Rep, typename Period, typename Predicate>
    bool wait_for(unique_lock& lock,
            const std::chrono::duration<Rep, Period>& rel_time,
            Predicate stop_waiting) {
        return cv_.wait_for(lock.std_unique_lock(), rel_time, std::move(stop_waiting));
    }

    template<typename Clock, typename Duration>
    std::cv_status wait_until(unique_lock& lock,
            const std::chrono::time_point<Clock, Duration>& timeout_time) {
        return cv_.wait_until(lock.std_unique_lock(), timeout_time);
    }

    template<typename Clock, typename Duration, typename Predicate>
    bool wait_until(unique_lock& lock,
            const std::chrono::time_point<Clock, Duration>& timeout_time,
            Predicate stop_waiting) {
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

// Define global capabilities for thread-safety annotation.
//
// These can be manually modified, or
// compile generate_mutex_order.cpp in the tests directory
// to generate this.

// --- Begin generated section

// Capabilities in priority order
// (declaration only, value is nullptr)
inline mutex* AudioFlinger_Mutex;
inline mutex* EffectHandle_Mutex
        ACQUIRED_AFTER(android::audio_utils::AudioFlinger_Mutex);
inline mutex* AudioFlinger_HardwareMutex
        ACQUIRED_AFTER(android::audio_utils::EffectHandle_Mutex);
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
inline mutex* EffectBase_Mutex
        ACQUIRED_AFTER(android::audio_utils::EffectChain_Mutex);
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

// Exclusion by capability
#define EXCLUDES_BELOW_PlaybackThread_AudioTrackCbMutex
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

#define EXCLUDES_BELOW_EffectChain_Mutex \
    EXCLUDES_EffectBase_Mutex
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

#define EXCLUDES_BELOW_EffectHandle_Mutex \
    EXCLUDES_AudioFlinger_HardwareMutex
#define EXCLUDES_EffectHandle_Mutex \
    EXCLUDES(android::audio_utils::EffectHandle_Mutex) \
    EXCLUDES_BELOW_EffectHandle_Mutex

#define EXCLUDES_BELOW_AudioFlinger_Mutex \
    EXCLUDES_EffectHandle_Mutex
#define EXCLUDES_AudioFlinger_Mutex \
    EXCLUDES(android::audio_utils::AudioFlinger_Mutex) \
    EXCLUDES_BELOW_AudioFlinger_Mutex

#define EXCLUDES_AUDIO_ALL \
    EXCLUDES_AudioFlinger_Mutex

// --- End generated section

} // namespace android::audio_utils

#pragma pop_macro("LOG_TAG")
