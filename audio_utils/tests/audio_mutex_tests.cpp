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

#include <audio_utils/mutex.h>
#include <gtest/gtest.h>

// Currently tests mutex priority-inheritance (or not) based on flag
// adb shell setprop \
// persist.device_config.aconfig_flags.media_audio.\
// com.android.media.audio.flags.mutex_priority_inheritance true
//
// TODO(b/209491695) enable both PI/non-PI mutex tests regardless of flag.

namespace android {

namespace audio_locks {

// Audio capabilities are typically assigned a partial order -
// but to furnish a proof of deadlock free audio, we create a global order
// (which isn't unique, just used to prove deadlock free access).

// Clang thread-safety can assigns capabilities to mutex-like objects.
// Here we use pointers to audio_utils::mutex*, they can be nullptr (never allocated)
// because they are used solely as a capability declaration, not as a mutex
// instance.
//
// In this example, we have 4 capabilities, labeled cap1, cap2, cap3, cap4.
// We'll show later in the IContainer.h example how we assign
// capabilities to actual mutexes.

// Add highest priority here
inline audio_utils::mutex* cap1;
inline audio_utils::mutex* cap2 ACQUIRED_AFTER(cap1);
inline audio_utils::mutex* cap3 ACQUIRED_AFTER(cap2);
inline audio_utils::mutex* cap4 ACQUIRED_AFTER(cap3);
// Add lowest priority here

// Now ACQUIRED_AFTER() isn't implemented (yet) in Clang Thread-Safety.
// As a solution, we define priority exclusion for lock acquisition.
// if you lock at a capability n, then you cannot hold
// locks at a lower capability (n+1, n+2, ...) otherwise
// there is lock inversion.

#define EXCLUDES_BELOW_4  // no capability below 4.
#define EXCLUDES_4 EXCLUDES(cap4) EXCLUDES_BELOW_4

#define EXCLUDES_BELOW_3 EXCLUDES_4
#define EXCLUDES_3 EXCLUDES(cap3) EXCLUDES_BELOW_3

#define EXCLUDES_BELOW_2 EXCLUDES_3
#define EXCLUDES_2 EXCLUDES(cap2) EXCLUDES_BELOW_2

#define EXCLUDES_BELOW_1 EXCLUDES_2
#define EXCLUDES_1 EXCLUDES(cap1) EXCLUDES_BELOW_1

#define EXCLUDES_ALL EXCLUDES_1

}  // namespace audio_locks

using namespace audio_locks;
// -----------------------------------------

// Example IContainer.h
//
// This is a Container interface with multiple exposed mutexes.
// Since the capabilities file audio_locks.h is global,
// this interface can be used in projects outside of the implementing
// project.

// Here RETURN_CAPABILITY is used to assign a capability to a mutex.

class IContainer {
public:
    virtual ~IContainer() = default;

    // This is an example of returning many mutexes for test purposes.
    // In AudioFlinger interfaces, we may return mutexes for locking
    // AudioFlinger_Mutex, AudioFlinger_ClientMutex, ThreadBase_Mutex, etc. for interface methods.
    //
    // We do not allow access to the mutex when holding a mutex of lower priority, so
    // we use EXCLUDES_....
    virtual audio_utils::mutex& mutex1() const RETURN_CAPABILITY(cap1) EXCLUDES_BELOW_1 = 0;
    virtual audio_utils::mutex& mutex2() const RETURN_CAPABILITY(cap2) EXCLUDES_BELOW_2 = 0;
    virtual audio_utils::mutex& mutex3() const RETURN_CAPABILITY(cap3) EXCLUDES_BELOW_3 = 0;

    // acquires capability 1, can't hold cap1 or lower level locks
    virtual int value1() const EXCLUDES_1 = 0;

    virtual int value1_l() const REQUIRES(cap1) = 0;
    virtual int value2_l() const REQUIRES(cap2) = 0;
    virtual int value3_l() const REQUIRES(cap3) = 0;

    virtual int combo12_l() const REQUIRES(cap1) EXCLUDES_2 = 0;

    // As value1, value2, value3 access requires cap1, cap2, cap3 individually,
    // combo123_lll() requires all three.
    virtual int combo123_lll() const REQUIRES(cap1, cap2, cap3) = 0;

    // We cannot use any of mutex1, mutex2, mutex3, as they are acquired
    // within the combo123() method.
    // If we happen to know EXCLUDES_1 > EXCLUDES2 > EXCLUDES_3
    // we could just use EXCLUDES_1.  We include all 3 here.
    virtual int combo123() const EXCLUDES_1 EXCLUDES_2 EXCLUDES_3 = 0;
};

// -----------------------------------------
//
// Example Container.cpp
//
// The container implemented the IContainer interface.
//
// We see here how the RETURN_CAPABILITY() is used to assign capability to mutexes.

class Container : public IContainer {
public:
    // Here we implement the mutexes that has the global capabilities.
    audio_utils::mutex& mutex1() const override RETURN_CAPABILITY(cap1) EXCLUDES_BELOW_1 {
        return mMutex1;
    }

    audio_utils::mutex& mutex2() const override RETURN_CAPABILITY(cap2) EXCLUDES_BELOW_2 {
        return mMutex2;
    }

    audio_utils::mutex& mutex3() const override RETURN_CAPABILITY(cap3) EXCLUDES_BELOW_3 {
        return mMutex3;
    }

    // We use EXCLUDES_1 (which is prevents cap1 and all lesser priority mutexes)
    // EXCLUDES(cap1) is not sufficient because if we acquire cap1,
    // we ALSO can't hold lower level locks.
    int value1() const override EXCLUDES_1 {

        // Locking is through mutex1() to get the proper capability.
        audio_utils::lock_guard l(mutex1());
        return value1_l();
    }
    int value1_l() const override REQUIRES(cap1) {
        return v1_;
    }
    int value2_l() const override REQUIRES(cap2) {
        return v2_;
    }
    int value3_l() const override REQUIRES(cap3)  {
        return v3_;
    }

    // This requires cap1, acquires cap2.
    int combo12_l() const override REQUIRES(cap1) EXCLUDES_2 {

        // Fails  return value1_l() + value2_l() + value3_l();
        audio_utils::lock_guard l(mutex2());
        return value1_l() + value2_l();
    }

#if 0
    // fails!
    int combo123_lll() REQUIRES(cap1, cap2, cap2) {
        return value1_l() + value2_l() + value3_l();
    }
#endif

    // We can use REQUIRES(cap1, cap2, cap3)
    //         or REQUIRES(cap1, cap2, mutex3())
    //
    // The REQUIRES expression must be valid as the first line in the function body.
    // This means that if the expression has visibility issues (accesses
    // a private base class member from a derived class, etc.) it won't work.
    // Quick test: Does a decltype(expr) work as the first line?
    // If not, then REQUIRES(expr) won't either.
    //
    int combo123_lll() const override REQUIRES(cap1, cap2, mutex3()) {
        return value1_l() + value2_l() + value3_l();
    }

#if 1
    // In this example, the 3 exclusions are needed because we acquire
    // 3 locks.  If we happen to know EXCLUDES_1 > EXCLUDES2 > EXCLUDES_3
    // we could just use EXCLUDES_1.  We include all 3 here.
    int combo123() const override EXCLUDES_1 EXCLUDES_2 EXCLUDES_3 {
        audio_utils::lock_guard l1(mutex1());
        audio_utils::lock_guard l2(mutex2());
        audio_utils::lock_guard l3(mutex3());

        return value1_l() + value2_l() + value3_l();
    }

#else
    // THIS FAILS
    int combo123() const override EXCLUDES_1 EXCLUDES_2 EXCLUDES_3 {
        audio_utils::lock_guard l2(mutex2());
        audio_utils::lock_guard l1(mutex1());  // 2 is acquired before 1.
        audio_utils::lock_guard l3(mutex3());

        return value1_l() + value2_l() + value3_l();
    }
#endif

private:
    // The actual implementation mutexes are never directly exposed.
    mutable audio_utils::mutex mMutex1;
    mutable audio_utils::mutex mMutex2;
    mutable audio_utils::mutex mMutex3;

    int v1_ GUARDED_BY(cap1) = 1;
    int v2_ GUARDED_BY(mutex2()) = 2;
    int v3_ GUARDED_BY(cap3) = 3;
};

TEST(audio_mutex_tests, Container) {
    Container c;

    EXPECT_EQ(1, c.value1()); // success

#if 0
    // fails
    {
        audio_utils::lock_guard(c.mutex1());
        EXPECT_EQ(3, c.combo12_l());
    }
#endif

    {
        audio_utils::lock_guard l(c.mutex1());
        EXPECT_EQ(3, c.combo12_l()); // success
    }
}

// Test access through the IContainer interface -
// see that mutex checking is done without knowledge of
// the actual implementation.

TEST(audio_mutex_tests, Interface) {
    Container c;
    IContainer *i = static_cast<IContainer*>(&c);

    EXPECT_EQ(1, i->value1()); // success

#if 0
    // fails
    {
        audio_utils::lock_guard(c.mutex1());
        EXPECT_EQ(3, c.combo12_l());
    }
#endif

    {
        audio_utils::lock_guard l(i->mutex1());
        EXPECT_EQ(3, i->combo12_l()); // success
    }

    ALOGD("%s: %s", __func__, audio_utils::mutex::all_stats_to_string().c_str());
}

} // namespace android

